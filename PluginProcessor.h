#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "OctopusEngine.h"
#include "TunerEngine.h"
#include "Tunings.h"
#include "TuningLibrary.h"

// accordeur : réglages de CHROMA lus dans REAPER (paramètres + état du plugin)
struct ChromaState
{
    bool on = false;              // l'accordeur est affiché (bloc CHROMA allumé, ou accordeur intégré)
    bool internal = false;        // accordeur intégré d'Octopus (B+C), sans bloc CHROMA
    int index = -1;               // indice REAPER du bloc CHROMA (-1 : accordeur intégré)
    int mode = 0;                 // 0 MONO, 1 POLY, 2 STROBE
    int ref = 440;                // La de référence
    bool muted = false;
    juce::String tuningName, tuningKey;
    std::vector<int> notes;       // accordage choisi dans CHROMA (grave -> aigu)
    int muteParam = -1, modeParam = -1, refParam = -1;
};

class ReaperBridge;

// OCTOPUS (NINE HANDS | DSP) : contrôleur de la chaîne d'effets de la piste REAPER qui l'héberge.
// Le son traverse Octopus sans modification ; tout le travail se fait sur le thread de messages (timer à 30 Hz),
// via l'API de REAPER, même fenêtre fermée.
class OctopusProcessor : public juce::AudioProcessor,
                         public juce::VST3ClientExtensions,
                         private juce::Timer
{
public:
    explicit OctopusProcessor (bool demoChain = false);
    ~OctopusProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Octopus"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }      // le MIDI continue vers les effets suivants
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // VST3 : REAPER se fait reconnaître ici (IReaperHostApplication)
    juce::VST3ClientExtensions* getVST3ClientExtensions() override { return this; }
    void setIHostApplication (Steinberg::FUnknown*) override;

    OctopusEngine& getEngine() noexcept { return *engine; }
    const ChromaState& chroma() const noexcept { return chromaState; }
    tuna::Snapshot tunerSnapshot() const { return tuner.getSnapshot(); }
    void chromaSetMute (bool mute);
    void chromaNextMode();
    void chromaExit();
    void refreshChroma (bool readTuning);     // relit les réglages de CHROMA (thread de messages)
    // presets d'accordage : bibliothèque de CHROMA (usine + accordages perso + favoris, fichier commun à CHROMA)
    TuningLibrary& tuningLibrary() noexcept { return library; }
    void chromaSelectTuning (int libraryIndex);
    void toggleInternalTuner();
    void chromaSetReference (int hz);     // -1 : revenir à l'accordage de CHROMA
    bool tuningOverridden() const { return overrideKey.isNotEmpty(); }
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> uiScaleIdx { -1 };     // -1 = taille par défaut (réglage commun) ; 0..3 = 50 / 60 / 75 / 100 %

    static constexpr const char* kTokenId = "octoid";
    static const char* const kTargetIds[OctopusEngine::numTargets];

private:
    std::unique_ptr<ChainHost> host;
    ReaperBridge* bridge = nullptr;         // = host quand on tourne pour de vrai
    std::unique_ptr<OctopusEngine> engine;
    std::atomic<float>* tokenParam = nullptr;
    std::atomic<float>* targetParam[OctopusEngine::numTargets] {};

    // MIDI reçu par le thread audio -> file sans verrou -> thread de messages (jamais d'appel à l'hôte en audio)
    struct MidiEv { juce::uint8 type, channel, number, value; double atMs; };     // type 0 = CC, 1 = Program Change ; atMs = heure d'arrivée
    juce::AbstractFifo midiFifo { 512 };
    // CC envoyés aux effets suivants par les doubles appuis (thread de messages -> thread audio, sans verrou)
    juce::AbstractFifo outFifo { 64 };
    std::array<juce::uint8, 64> outCc {};
    std::array<MidiEv, 512> midiBuf {};

    // état sauvé avec le projet : copie tenue à jour sur le thread de messages, lue par getStateInformation
    juce::CriticalSection stateLock;        // jamais pris par le thread audio
    juce::ValueTree engineState;
    juce::ValueTree pendingEngineState;     // reçu par setStateInformation hors du thread de messages

    // accordeur (moteur de CHROMA) : le thread audio copie le son dans une file sans verrou, un thread d'analyse
    // le traite ; seulement quand un bloc CHROMA est allumé
    struct AnalysisThread : public juce::Thread
    {
        explicit AnalysisThread (OctopusProcessor& o) : juce::Thread ("Octopus tuner"), owner (o) {}
        void run() override;
        OctopusProcessor& owner;
    };
    tuna::TunerEngine tuner;
    juce::AbstractFifo tunerFifo { 1 << 16 };
    std::vector<float> tunerBuf;
    std::atomic<bool> tunerOn { false };
    // accordeur intégré (B+C) : ses propres réglages, gardés avec le projet
    std::atomic<bool> innerOn { false }, innerMute { false };
    int innerMode = 0, innerRef = 440;
    double sampleRate = 48000.0;
    AnalysisThread analysis { *this };
    ChromaState chromaState;
    TuningLibrary library;
    // accordage choisi dans Octopus : prioritaire tant que l'accordage n'est pas changé dans CHROMA
    juce::String overrideKey, overrideName, lastChromaKey;
    std::vector<int> overrideNotes;
    juce::String chromaGuid;
    int chromaTick = 0;
    void updateChroma();

    void timerCallback() override;
    void newToken();
    void storeEngineState();

    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OctopusProcessor)
};
