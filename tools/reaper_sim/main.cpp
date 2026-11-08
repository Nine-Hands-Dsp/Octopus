// Test du pont REAPER d'Octopus contre une simulation de l'API de REAPER (hors plugin, Linux).
// La simulation reproduit le comportement documenté des fonctions utilisées (reaper_plugin_functions.h) et produit /
// relit l'état des pistes au format RPPXML (<TRACK ... <FXCHAIN ... BYPASS / <VST ... > / FXID / WAK ...).
#include <juce_core/juce_core.h>
#include <pluginterfaces/base/funknown.h>
#include "ReaperBridge.h"
#include "OctopusEngine.h"
#include "FxCategory.h"
#include "OctoSettings.h"
#include "Routing.h"
#include <map>

using juce::String;

// ------------------------------------------------------------------ monde simulé
struct FakeFx
{
    String name, renamed, guid, state;
    bool enabled = true, offline = false, open = false;
    int parallel = 0;
    std::vector<double> params { 0.5, 0.5, 0.5 };
    juce::StringArray paramNames;             // noms réels des paramètres (sinon « Param n »)
    std::vector<int> paramStepCount;          // 2 = interrupteur, 0 = continu (si paramNames est rempli)
    const double* token = nullptr;            // != nullptr : c'est un Octopus (paramètre 0 = jeton)
    int serial = 0;                           // identité de l'instance (pour vérifier qu'elle n'est pas recréée)
    int presetIdx = -1;                       // preset affiché par REAPER (-1 = réglages modifiés)
};
// presets de chaque plugin : usine (programmes) puis utilisateur (fichier .ini)
static const juce::StringArray kFactoryPresets { "Default", "Bright", "Dark" };
static const juce::StringArray kUserPresets { "My lead" };
static juce::File gUserIni;
struct FakeTrack { String name; std::vector<FakeFx> fx, rec; int recArm = 0, nchan = 2; };
static juce::File gResource;              // dossier de ressources de REAPER simulé (effets JS de routage)
static int gPlayState = 0;

static std::vector<std::unique_ptr<FakeTrack>> gTracks;
static FakeTrack* gParent = nullptr;
static int gSerial = 1, gUndoOpen = 0, gUndoBlocks = 0, gRefresh = 0;
static juce::StringArray gInstalled { "VST3: CHROMA (NINE HANDS | DSP)", "VST3: Doorn (NINE HANDS | DSP)", "VST3: Holyverb (NINE HANDS | DSP)",
                                      "VST3: Pipeline (NINE HANDS | DSP)", "VST3: ReaEQ (Cockos)", "VST3i: Surge XT (Surge Synth Team)",
                                      "VST3: Octopus (NINE HANDS | DSP)" };

static String newGuid() { return "{" + juce::Uuid().toString().toUpperCase() + "}"; }
static FakeFx makeFx (const String& n) { FakeFx f; f.name = n; f.guid = newGuid(); f.state = "state-of-" + n.replace ("|", "/"); f.serial = gSerial++; return f; }   // "|" sert de separateur dans le chunk simule
static FakeTrack* T (void* t) { return static_cast<FakeTrack*> (t); }
static std::vector<FakeFx>& chainOf (void* t, int& fx) { const bool rec = (fx & 0x1000000) != 0; fx &= 0xffffff; return rec ? T (t)->rec : T (t)->fx; }
static FakeFx* fxAt (void* t, int fx) { auto& c = chainOf (t, fx); return fx >= 0 && fx < (int) c.size() ? &c[(size_t) fx] : nullptr; }
static void copyOut (const String& s, char* buf, int sz) { if (sz <= 0) return; auto u = s.toStdString(); std::strncpy (buf, u.c_str(), (size_t) sz - 1); buf[sz - 1] = 0; }

static String fxChunk (const FakeFx& f)
{
    String p;
    for (auto v : f.params) p << String (v, 6) << ",";
    String e;
    e << "BYPASS " << (f.enabled ? 0 : 1) << " " << (f.offline ? 1 : 0) << " 0\n"
      << "<VST \"" << f.name << "\" " << f.name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") << ".vst3 0 \"\" 1234{ABCD} \"\"\n"
      << "  " << juce::Base64::toBase64 (f.state + "|" + p) << "\n"
      << ">\n";
    if (f.renamed.isNotEmpty()) e << "RENAME \"" << f.renamed << "\"\n";
    e << "FLOATPOS 0 0 0 0\nFXID " << f.guid << "\nWAK 0 0\n";
    return e;
}

static String trackChunk (const FakeTrack& t)
{
    String c;
    c << "<TRACK {TRACKGUID}\nNAME \"" << t.name << "\"\nPEAKCOL 16576\nVOLPAN 1 0 -1 -1 1\n";
    if (! t.fx.empty())
    {
        c << "<FXCHAIN\nWNDRECT 24 52 655 408\nSHOW 0\nLASTSEL 0\nDOCKED 0\n";
        for (auto& f : t.fx) c << fxChunk (f);
        c << ">\n";
    }
    if (! t.rec.empty())
    {
        c << "<FXCHAIN_REC\nSHOW 0\nLASTSEL 0\nDOCKED 0\n";
        for (auto& f : t.rec) c << fxChunk (f);
        c << ">\n";
    }
    c << ">\n";
    return c;
}

// relit un effet écrit au format texte (comme REAPER au chargement)
static FakeFx parseFx (const String& e)
{
    FakeFx f;
    f.name = ChainChunk::displayName (e);
    f.enabled = ! ChainChunk::bypassed (e);
    f.offline = ChainChunk::offline (e);
    juce::StringArray l; l.addLines (e);
    bool inBlock = false;
    for (auto& raw : l)
    {
        auto t = raw.trim();
        if (t.startsWith ("<VST")) { inBlock = true; continue; }
        if (inBlock && t == ">") { inBlock = false; continue; }
        if (inBlock && t.isNotEmpty())
        {
            juce::MemoryOutputStream mo;
            juce::Base64::convertFromBase64 (mo, t);
            auto dec = mo.toString();
            f.state = dec.upToFirstOccurrenceOf ("|", false, false);
            f.params.clear();
            for (auto& v : juce::StringArray::fromTokens (dec.fromFirstOccurrenceOf ("|", false, false), ",", "")) if (v.isNotEmpty()) f.params.push_back (v.getDoubleValue());
        }
        if (t.startsWith ("FXID ")) f.guid = t.fromFirstOccurrenceOf ("FXID ", false, false).trim();
        if (t.startsWith ("RENAME ")) f.renamed = t.fromFirstOccurrenceOf ("RENAME ", false, false).unquoted();
    }
    if (f.guid.isEmpty()) f.guid = newGuid();       // REAPER crée un identifiant neuf
    f.serial = gSerial++;
    return f;
}

// ------------------------------------------------------------------ API simulée
namespace api
{
    int TrackFX_GetCount (void* t) { return (int) T (t)->fx.size(); }
    int TrackFX_GetRecCount (void* t) { return (int) T (t)->rec.size(); }
    bool TrackFX_GetFXName (void* t, int fx, char* b, int sz) { auto* f = fxAt (t, fx); if (! f) return false; copyOut (f->renamed.isNotEmpty() ? f->renamed : f->name, b, sz); return true; }
    bool TrackFX_GetNamedConfigParm (void* t, int fx, const char* p, char* b, int sz)
    {
        auto* f = fxAt (t, fx); if (! f) return false;
        const String k (p);
        if (k == "fx_name") { copyOut (f->name, b, sz); return true; }
        if (k == "fx_ident") { copyOut ("/plugins/" + f->name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") + ".vst3", b, sz); return true; }
        if (k == "parallel") { copyOut (String (f->parallel), b, sz); return true; }
        return false;
    }
    bool TrackFX_SetNamedConfigParm (void* t, int fx, const char* p, const char* v) { auto* f = fxAt (t, fx); if (! f || String (p) != "parallel") return false; f->parallel = String (v).getIntValue(); return true; }
    bool TrackFX_GetEnabled (void* t, int fx) { auto* f = fxAt (t, fx); return f && f->enabled; }
    void TrackFX_SetEnabled (void* t, int fx, bool e) { if (auto* f = fxAt (t, fx)) f->enabled = e; }
    bool TrackFX_GetOffline (void* t, int fx) { auto* f = fxAt (t, fx); return f && f->offline; }
    void TrackFX_SetOffline (void* t, int fx, bool o) { if (auto* f = fxAt (t, fx)) f->offline = o; }
    bool TrackFX_GetOpen (void* t, int fx) { auto* f = fxAt (t, fx); return f && f->open; }
    void* TrackFX_GetFXGUID (void* t, int fx) { auto* f = fxAt (t, fx); return f ? (void*) &f->guid : nullptr; }
    void guidToString (const void* g, char* dest) { copyOut (*static_cast<const String*> (g), dest, 64); }
    int TrackFX_GetNumParams (void* t, int fx) { auto* f = fxAt (t, fx); return f ? (f->token ? 8 : (int) f->params.size()) : 0; }
    bool TrackFX_GetParamName (void* t, int fx, int p, char* b, int sz)
    {
        auto* f = fxAt (t, fx); if (! f) return false;
        copyOut (f->token ? (p == 0 ? String ("Octopus ID") : "Footswitch " + String (p))
                          : (p < f->paramNames.size() ? f->paramNames[p] : "Param " + String (p)), b, sz);
        return true;
    }
    bool TrackFX_GetFormattedParamValue (void* t, int fx, int p, char* b, int sz)
    {
        auto* f = fxAt (t, fx); if (! f || f->token || p >= (int) f->params.size()) return false;
        copyOut (String (f->params[(size_t) p] * 10.0, 1) + " dB", b, sz); return true;
    }
    double TrackFX_GetParamNormalized (void* t, int fx, int p)
    {
        auto* f = fxAt (t, fx); if (! f) return 0.0;
        if (f->token) return p == 0 ? *f->token : 0.0;
        return p < (int) f->params.size() ? f->params[(size_t) p] : 0.0;
    }
    bool TrackFX_SetParamNormalized (void* t, int fx, int p, double v) { auto* f = fxAt (t, fx); if (! f || f->token || p >= (int) f->params.size()) return false; f->params[(size_t) p] = v; return true; }
    int TrackFX_AddByName (void* t, const char* n, bool rec, int inst)
    {
        const String name (n);
        const bool js = name.startsWith ("JS:NINE HANDS/Routing/") && gResource.getChildFile ("Effects").getChildFile (name.fromFirstOccurrenceOf ("JS:", false, false)).existsAsFile();
        if (! js && ! gInstalled.contains (name)) return -1;
        auto& c = rec ? T (t)->rec : T (t)->fx;
        const int at = inst <= -1000 ? juce::jlimit (0, (int) c.size(), -inst - 1000) : (int) c.size();
        auto f = makeFx (n);
        if (name.contains ("nh_merge"))      // curseurs du JS Merge (valeurs normalisées par défaut)
        {
            f.params = { 0.0, 1.0 };
            for (int l = 0; l < 3; ++l) for (double v : { 60.0 / 72.0, 0.5, 0.0, 0.0, 1.0 }) f.params.push_back (v);
        }
        else if (js) f.params.clear();
        c.insert (c.begin() + at, f);
        return at;
    }
    bool TrackFX_Delete (void* t, int fx) { auto& c = chainOf (t, fx); if (fx < 0 || fx >= (int) c.size()) return false; c.erase (c.begin() + fx); return true; }
    // comportement de REAPER : l'effet est retiré puis inséré à dest_fx (borné à la taille de la chaîne)
    void TrackFX_CopyToTrack (void* s, int sfx, void* d, int dfx, bool move)
    {
        auto& sc = chainOf (s, sfx);
        if (sfx < 0 || sfx >= (int) sc.size()) return;
        FakeFx f = sc[(size_t) sfx];
        if (move) sc.erase (sc.begin() + sfx); else { f.guid = newGuid(); f.serial = gSerial++; }
        auto& dc = chainOf (d, dfx);
        dfx = juce::jlimit (0, (int) dc.size(), dfx);
        dc.insert (dc.begin() + dfx, f);
    }
    void TrackFX_Show (void* t, int fx, int flag) { if (auto* f = fxAt (t, fx)) f->open = flag == 3; }
    double GetMediaTrackInfo_Value (void* t, const char* p) { return String (p) == "I_RECARM" ? (double) T (t)->recArm : String (p) == "I_NCHAN" ? (double) T (t)->nchan : 0.0; }
    bool SetMediaTrackInfo_Value (void* t, const char* p, double v) { if (String (p) == "I_NCHAN") { T (t)->nchan = (int) v; return true; } return false; }
    const char* GetResourcePath() { static std::string s; s = gResource.getFullPathName().toStdString(); return s.c_str(); }
    int GetPlayStateEx (void*) { return gPlayState; }
    bool GetTrackName (void* t, char* b, int sz) { copyOut (T (t)->name, b, sz); return true; }
    bool ValidatePtr2 (void*, void* p, const char*) { for (auto& tr : gTracks) if (tr.get() == p) return true; return false; }
    void Undo_BeginBlock2 (void*) { ++gUndoOpen; }
    void Undo_EndBlock2 (void*, const char*, int) { --gUndoOpen; ++gUndoBlocks; }
    void PreventUIRefresh (int c) { gRefresh += c; }
    bool GetTrackStateChunk (void* t, char* b, int sz, bool) { copyOut (trackChunk (*T (t)), b, sz); return true; }
    bool SetTrackStateChunk (void* t, const char* str, bool)
    {
        juce::StringArray e;
        auto* tr = T (t);
        tr->fx.clear();
        if (ChainChunk::extract (String (juce::CharPointer_UTF8 (str)), false, e))
            for (auto& x : e)
            {
                auto f = parseFx (x);
                const bool js = f.name.startsWith ("JS:NINE HANDS/Routing/") && gResource.getChildFile ("Effects").getChildFile (f.name.fromFirstOccurrenceOf ("JS:", false, false)).existsAsFile();
                if (js || gInstalled.contains (f.name)) tr->fx.push_back (f);      // plugin absent : REAPER ne le charge pas
            }
        return true;
    }
    void InsertTrackInProject (void*, int idx, int) { auto t = std::make_unique<FakeTrack>(); t->name = "Track " + String (idx + 1); gTracks.insert (gTracks.begin() + juce::jlimit (0, (int) gTracks.size(), idx), std::move (t)); }
    void DeleteTrack (void* t) { for (size_t i = 0; i < gTracks.size(); ++i) if (gTracks[i].get() == t) { gTracks.erase (gTracks.begin() + (long) i); return; } }
    void* GetTrack (void*, int i) { return i >= 0 && i < (int) gTracks.size() ? gTracks[(size_t) i].get() : nullptr; }
    int CountTracks (void*) { return (int) gTracks.size(); }
    bool EnumInstalledFX (int i, const char** n, const char** id)
    {
        static std::vector<std::string> keep;
        if (i >= gInstalled.size()) return false;
        keep.push_back (gInstalled[i].toStdString()); *n = keep.back().c_str();
        keep.push_back ("/plugins/x.vst3"); *id = keep.back().c_str();
        return true;
    }
    const char* GetAppVersion() { return "7.42/linux-x86_64"; }

    // ---- presets
    static double presetValue (int idx, int p) { return 0.1 + 0.2 * idx + 0.05 * p; }
    int TrackFX_GetPresetIndex (void* t, int fx, int* n) { auto* f = fxAt (t, fx); if (n) *n = f && ! f->token ? kFactoryPresets.size() + kUserPresets.size() : 0; return f ? f->presetIdx : -1; }
    bool TrackFX_SetPresetByIndex (void* t, int fx, int idx)
    {
        auto* f = fxAt (t, fx);
        if (! f || f->token || idx < 0 || idx >= kFactoryPresets.size() + kUserPresets.size()) return false;
        f->presetIdx = idx;
        for (size_t p = 0; p < f->params.size(); ++p) f->params[p] = presetValue (idx, (int) p);
        return true;
    }
    static String nameOf (int idx) { return idx < kFactoryPresets.size() ? kFactoryPresets[idx] : kUserPresets[idx - kFactoryPresets.size()]; }
    bool TrackFX_GetPreset (void* t, int fx, char* b, int sz) { auto* f = fxAt (t, fx); if (! f) return false; copyOut (f->presetIdx >= 0 ? nameOf (f->presetIdx) : String(), b, sz); return f->presetIdx >= 0; }
    bool TrackFX_SetPreset (void* t, int fx, const char* name)
    {
        for (int k = 0; k < kFactoryPresets.size() + kUserPresets.size(); ++k) if (nameOf (k) == String (name)) return TrackFX_SetPresetByIndex (t, fx, k);
        return false;
    }
    bool TrackFX_NavigatePresets (void* t, int fx, int move)
    {
        auto* f = fxAt (t, fx); const int n = kFactoryPresets.size() + kUserPresets.size();
        return f && TrackFX_SetPresetByIndex (t, fx, ((f->presetIdx < 0 ? 0 : f->presetIdx) + move + n) % n);
    }
    void TrackFX_GetUserPresetFilename (void*, int, char* b, int sz) { copyOut (gUserIni.getFullPathName(), b, sz); }
    // paramètre 1 : 4 positions (0..3 par pas de 1) ; paramètre 2 : interrupteur ; paramètre 0 : continu
    bool TrackFX_GetParameterStepSizes (void* t, int fx, int p, double* st, double* sm, double* lg, bool* tog)
    {
        *st = *sm = *lg = 0.0; *tog = false;
        if (auto* f = fxAt (t, fx); f != nullptr && f->paramNames.size() > 0)
        {
            if (p < (int) f->paramStepCount.size() && f->paramStepCount[(size_t) p] == 2) { *tog = true; return true; }
            return false;
        }
        if (p == 1) { *st = 1.0; return true; }
        if (p == 2) { *tog = true; return true; }
        return false;
    }
    double TrackFX_GetParamEx (void* t, int fx, int p, double* mn, double* mx, double* md)
    {
        *mn = 0.0; *mx = p == 1 ? 3.0 : 1.0; *md = *mx * 0.5;
        auto* f = fxAt (t, fx);
        return f && p < (int) f->params.size() ? f->params[(size_t) p] : 0.0;
    }
    // état exact d'un plugin (vst_chunk) : réglages + preset affiché
    String chunkOf (const FakeFx& f) { String p; for (auto v : f.params) p << String (v, 9) << ","; return juce::Base64::toBase64 (f.state + "|" + p + "|" + String (f.presetIdx)); }
}

namespace api
{
    bool TrackFX_GetNamedConfigParm2 (void* t, int fx, const char* p, char* b, int sz)
    {
        auto* f = fxAt (t, fx); if (! f) return false;
        if (String (p) == "vst_chunk") { copyOut (chunkOf (*f), b, sz); return true; }
        return TrackFX_GetNamedConfigParm (t, fx, p, b, sz);
    }
    bool TrackFX_SetNamedConfigParm2 (void* t, int fx, const char* p, const char* v)
    {
        auto* f = fxAt (t, fx); if (! f) return false;
        if (String (p) == "vst_chunk")
        {
            juce::MemoryOutputStream mo; juce::Base64::convertFromBase64 (mo, String (v));
            auto parts = juce::StringArray::fromTokens (mo.toString(), "|", "");
            f->params.clear();
            for (auto& x : juce::StringArray::fromTokens (parts[1], ",", "")) if (x.isNotEmpty()) f->params.push_back (x.getDoubleValue());
            f->presetIdx = parts[2].getIntValue();
            return true;
        }
        return TrackFX_SetNamedConfigParm (t, fx, p, v);
    }
}

// ------------------------------------------------------------------ IReaperHostApplication simulée (même disposition que le SDK)
class FakeHost : public Steinberg::FUnknown
{
public:
    virtual void* PLUGIN_API getReaperApi (Steinberg::CStringA name)
    {
        static std::map<std::string, void*> m {
            #define F(x) { #x, (void*) &api::x }
            { "TrackFX_GetNamedConfigParm", (void*) &api::TrackFX_GetNamedConfigParm2 }, { "TrackFX_SetNamedConfigParm", (void*) &api::TrackFX_SetNamedConfigParm2 },
            F(TrackFX_GetParameterStepSizes), F(TrackFX_GetParamEx), F(TrackFX_GetPresetIndex), F(TrackFX_SetPresetByIndex), F(TrackFX_GetPreset), F(TrackFX_SetPreset), F(TrackFX_NavigatePresets), F(TrackFX_GetUserPresetFilename),
            F(TrackFX_GetCount), F(TrackFX_GetRecCount), F(TrackFX_GetFXName),
            F(TrackFX_GetEnabled), F(TrackFX_SetEnabled), F(TrackFX_GetOffline), F(TrackFX_SetOffline), F(TrackFX_GetOpen), F(TrackFX_GetFXGUID),
            F(guidToString), F(TrackFX_GetNumParams), F(TrackFX_GetParamName), F(TrackFX_GetFormattedParamValue), F(TrackFX_GetParamNormalized),
            F(TrackFX_SetParamNormalized), F(TrackFX_AddByName), F(TrackFX_Delete), F(TrackFX_CopyToTrack), F(TrackFX_Show), F(GetTrackName),
            F(ValidatePtr2), F(Undo_BeginBlock2), F(Undo_EndBlock2), F(PreventUIRefresh), F(GetTrackStateChunk), F(SetTrackStateChunk),
            F(InsertTrackInProject), F(DeleteTrack), F(GetTrack), F(CountTracks), F(EnumInstalledFX), F(GetAppVersion), F(GetMediaTrackInfo_Value), F(GetPlayStateEx), F(SetMediaTrackInfo_Value), F(GetResourcePath)
            #undef F
        };
        auto it = m.find (name);
        return it != m.end() ? it->second : nullptr;
    }
    virtual void* PLUGIN_API getReaperParent (Steinberg::uint32 w) { return w == 1 ? (void*) gParent : (w == 3 ? (void*) &gTracks : nullptr); }
    virtual void* PLUGIN_API reaperExtended (Steinberg::uint32, void*, void*, void*) { return nullptr; }

    Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID iid, void** obj) override
    {
        const Steinberg::TUID mine = INLINE_UID (0x79655E36, 0x77EE4267, 0xA573FEF7, 0x4912C27C);
        if (std::memcmp (iid, mine, sizeof (Steinberg::TUID)) == 0) { *obj = this; ++refs; return Steinberg::kResultOk; }
        *obj = nullptr; return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return (Steinberg::uint32) ++refs; }
    Steinberg::uint32 PLUGIN_API release() override { return (Steinberg::uint32) --refs; }
    int refs = 1;
};

class NotReaper : public Steinberg::FUnknown
{
public:
    Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID, void** obj) override { *obj = nullptr; return Steinberg::kNoInterface; }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
};

// ------------------------------------------------------------------ tests
static int failures = 0;
static void check (bool ok, const String& what) { std::printf ("%s %s\n", ok ? "  OK  " : "  FAIL", what.toRawUTF8()); if (! ok) ++failures; }
static String rnames (const FakeTrack& t)      // chaîne avec les repères de voies : < = Split, |B |C, > = Merge
{
    juce::StringArray s;
    for (auto& f : t.fx)
    {
        FxInfo fi; fi.name = fi.original = f.name;
        const int r = Routing::roleOf (fi);
        s.add (f.token ? String ("[OCTO]") : r == Routing::split ? String ("<") : r == Routing::pathB ? String ("|B") : r == Routing::pathC ? String ("|C")
               : r == Routing::merge ? String (">") : FxCat::shortName (f.name) + (f.enabled ? "" : "(off)"));
    }
    return s.joinIntoString (" ");
}

static String names (const FakeTrack& t)
{
    juce::StringArray s;
    for (auto& f : t.fx) s.add ((f.token ? String ("[OCTO]") : FxCat::shortName (f.name)) + (f.enabled ? "" : "(off)"));
    return s.joinIntoString (" > ");
}

int main()
{
    // projet : piste 1 "Guitare" = CHROMA > Octopus > Doorn > Holyverb ; piste 2 = ReaEQ
    double token = 0.4242, token2 = 0.7311;
    for (auto* n : { "Guitare", "Basse" }) { auto t = std::make_unique<FakeTrack>(); t->name = n; gTracks.push_back (std::move (t)); }
    auto& tr = *gTracks[0];
    tr.fx.push_back (makeFx ("VST3: CHROMA (NINE HANDS | DSP)"));
    auto octo = makeFx ("VST3: Octopus (NINE HANDS | DSP)"); octo.token = &token; octo.renamed = "Mon pedalier"; tr.fx.push_back (octo);
    tr.fx.push_back (makeFx ("VST3: Doorn (NINE HANDS | DSP)"));
    tr.fx.push_back (makeFx ("VST3: Holyverb (NINE HANDS | DSP)"));
    gTracks[1]->fx.push_back (makeFx ("VST3: ReaEQ (Cockos)"));
    gParent = &tr;
    const int octoSerial = tr.fx[1].serial;

    std::printf ("Pont REAPER (API simulée)\n");
    {
        ReaperBridge notReaper ([] { return 0.0; });
        NotReaper nr;
        notReaper.attach (&nr);
        check (! notReaper.ready() && notReaper.statusText().contains ("REAPER"), "hors REAPER : inactif (" + notReaper.statusText() + ")");
    }

    FakeHost host;
    ReaperBridge bridge ([&] { return token; });
    bridge.attach (&host);
    OctopusEngine eng (bridge);
    bool clash = false;
    eng.onTokenClash = [&] { clash = true; };
    eng.refreshNow();
    check (eng.isReady(), "REAPER reconnu, piste trouvée (" + eng.trackName() + ")");
    check (eng.blocks().size() == 3 && eng.chain().getReference (1).isSelf, "Octopus (renommé « Mon pedalier ») retrouvé en 2e position, 3 blocs affichés");
    check (eng.chain().getReference (0).name == "VST3: CHROMA (NINE HANDS | DSP)", "noms lus");

    eng.toggle (eng.blocks()[1]);
    check (! tr.fx[2].enabled && gUndoOpen == 0, "ON/OFF de Doorn (point d'annulation fermé)");
    eng.remove (1);
    check (tr.fx.size() == 4 && tr.fx[1].serial == octoSerial, "Octopus refuse de se supprimer lui-même");

    eng.save (0);
    check (eng.preset (0).entries.size() == 3 && eng.preset (0).selfPos == 1, "1A : 3 effets, Octopus en position 1");

    // réglage modifié + effet rallumé -> 1B
    bridge.setParam (eng.blocks()[2], 0, 0.9);
    eng.toggle (eng.blocks()[1]);
    eng.save (1);
    const int holySerialB = tr.fx[3].serial;
    const int chromaSerial = tr.fx[0].serial;

    eng.load (0);
    check (names (tr) == "CHROMA > [OCTO] > Doorn(off) > Holyverb", "rappel 1A : " + names (tr));
    check (std::abs (tr.fx[3].params[0] - 0.5) < 1e-6, "réglage de Holyverb rendu (0.5)");
    check (tr.fx[3].serial != holySerialB && tr.fx[0].serial == chromaSerial && tr.fx[2].serial != 0, "seul Holyverb (réglage différent) est rechargé ; CHROMA intact");
    check (tr.fx[1].serial == octoSerial && tr.fx[1].renamed == "Mon pedalier", "Octopus jamais recréé ni déplacé");
    check (gTracks.size() == 2 && gRefresh == 0 && gUndoOpen == 0, "piste temporaire supprimée, rafraîchissement et annulation équilibrés");

    eng.load (1);
    check (names (tr) == "CHROMA > [OCTO] > Doorn > Holyverb" && std::abs (tr.fx[3].params[0] - 0.9) < 1e-6, "rappel 1B : " + names (tr));

    // structure différente : on vide et on reconstruit
    eng.remove (eng.blocks()[0]);
    eng.addFx ({ "VST3: Pipeline (NINE HANDS | DSP)", "" }, 99);
    eng.addFx ({ "VST3: ReaEQ (Cockos)", "" }, 0);
    check (names (tr) == "[OCTO] > ReaEQ > Doorn > Holyverb > Pipeline", "ajout devant le 1er bloc / à la fin : " + names (tr));
    eng.moveBlock (eng.blocks()[3], 1);
    check (names (tr) == "[OCTO] > ReaEQ > Pipeline > Doorn > Holyverb", "déplacement de Pipeline en 2e bloc : " + names (tr));
    eng.moveBlock (eng.blocks()[0], 2);
    check (names (tr) == "[OCTO] > Pipeline > Doorn > ReaEQ > Holyverb", "déplacement de ReaEQ après Doorn : " + names (tr));
    eng.moveBlock (eng.blocks()[0], 99);
    check (names (tr) == "[OCTO] > Doorn > ReaEQ > Holyverb > Pipeline", "déplacement de Pipeline à la fin : " + names (tr));
    // piste armée : seulement celle d'Octopus compte
    {
        eng.refreshNow(); const bool idle = eng.recState() == 0;
        gTracks.back()->recArm = gTracks.back().get() != &tr ? 1 : 0; gPlayState = 5; eng.refreshNow();
        const bool other = eng.recState() == 0;
        tr.recArm = 1; gPlayState = 1; eng.refreshNow(); const bool armed = eng.recState() == 1;
        gPlayState = 5; eng.refreshNow(); const bool recording = eng.recState() == 2;
        tr.recArm = 0; gPlayState = 0; for (auto& t : gTracks) t->recArm = 0; eng.refreshNow();
        check (idle && other && armed && recording && eng.recState() == 0, "piste armée (clignote) / en enregistrement (fixe), autres pistes ignorées");
    }
    eng.addFx ({ "VST3: Inexistant (X)", "" }, 0);
    check (eng.message().contains ("could not"), "plugin introuvable signalé : " + eng.message());

    eng.load (0);
    check (names (tr) == "CHROMA > [OCTO] > Doorn(off) > Holyverb" && tr.fx[1].serial == octoSerial, "rappel 1A après changement de structure : " + names (tr));

    // preset contenant un plugin désinstallé
    gInstalled.removeString ("VST3: Doorn (NINE HANDS | DSP)");
    eng.remove (eng.blocks()[0]);                               // structure différente -> rechargement complet
    eng.load (1);
    check (names (tr) == "CHROMA > [OCTO] > Holyverb" && eng.message().contains ("Doorn"), "plugin manquant : les autres à leur place, message : " + eng.message());
    gInstalled.add ("VST3: Doorn (NINE HANDS | DSP)");

    // parallèle, fenêtre, paramètres
    eng.load (1);
    eng.setParallel (eng.blocks()[2], true);
    check (tr.fx[3].parallel == 1, "Holyverb en parallèle");
    eng.openWindow (eng.blocks()[0], true);
    check (tr.fx[0].open && eng.chain().getReference (0).open, "fenêtre de CHROMA ouverte");
    check (bridge.paramText (eng.blocks()[1], 0) == "5.0 dB" && bridge.paramName (eng.blocks()[1], 1) == "Param 1", "paramètres lus (nom, valeur affichée)");

    // effets installés : instruments listés (filtrés à l'écran), Octopus exclu
    const auto& inst = eng.installedList();
    bool hasOcto = false; for (auto& f : inst) hasOcto = hasOcto || f.name.contains ("Octopus");
    check (inst.size() == 6 && ! hasOcto, "liste des plugins installés (Octopus exclu)");

    // piste dupliquée : deux Octopus avec le même jeton
    auto twin = tr.fx[1]; twin.serial = gSerial++; twin.guid = newGuid();
    tr.fx.push_back (twin);
    eng.refreshNow();
    check (clash, "deux Octopus au même jeton : détecté (nouveau jeton demandé)");
    tr.fx.back().token = &token2;
    clash = false; eng.refreshNow();
    check (! clash && eng.chain().getReference (1).isSelf && ! eng.chain().getReference (4).isSelf, "jetons différents : chaque Octopus se reconnaît");
    tr.fx.pop_back();

    // Octopus dans les effets d'entrée (enregistrement)
    {
        auto o = tr.fx[1];
        tr.fx.erase (tr.fx.begin() + 1);
        tr.rec.push_back (makeFx ("VST3: CHROMA (NINE HANDS | DSP)"));
        tr.rec.push_back (o);
        eng.refreshNow();
        check (eng.isReady() && bridge.isInputChain() && eng.blocks().size() == 1, "Octopus dans les effets d'entrée : pilote cette chaîne-là");
        eng.save (5);
        eng.toggle (eng.blocks()[0]);
        eng.load (5);
        check (tr.rec.size() == 2 && tr.rec[0].enabled && tr.rec[1].token != nullptr && tr.fx.size() == 3, "preset dans les effets d'entrée (chaîne normale intacte)");
    }

    // ---- presets d'un plugin (liste usine + utilisateur, état exact rétabli après la lecture des noms)
    {
        tr.fx.clear(); tr.rec.clear();
        auto o = makeFx ("VST3: Octopus (NINE HANDS | DSP)"); o.token = &token; tr.fx.push_back (o);
        gInstalled.addIfNotAlreadyThere ("VST3: Holyverb (NINE HANDS | DSP)");
        eng.refreshNow();
        juce::Array<InstalledFx> several;
        for (auto* n : { "VST3: Holyverb (NINE HANDS | DSP)", "VST3: Inexistant (X)", "VST3: ReaEQ (Cockos)", "VST3: Doorn (NINE HANDS | DSP)" }) several.add ({ n, "" });
        eng.addFxList (several, 0);
        check (names (tr) == "[OCTO] > Holyverb > ReaEQ > Doorn" && gUndoOpen == 0, "ajout de plusieurs effets d'un coup, dans l'ordre (le plugin absent est signalé) : " + names (tr));
        gUserIni = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vst3-Holyverb.ini");
        gUserIni.replaceWithText ("[General]\nNbPresets=1\n\n[Preset0]\nData=00\nName=My lead\nLen=1\n");
        auto& holy = tr.fx[1];
        holy.params = { 0.33, 0.44, 0.55 }; holy.presetIdx = -1;                 // réglages modifiés à la main
        const auto pr = bridge.presets (eng.blocks()[0], true);
        check (pr.names.joinIntoString (",") == "Default,Bright,Dark,My lead" && ! pr.user[0] && pr.user[3], "noms des presets lus (usine + utilisateur) : " + pr.names.joinIntoString (", "));
        check (std::abs (holy.params[0] - 0.33) < 1e-9 && std::abs (holy.params[2] - 0.55) < 1e-9 && holy.presetIdx == -1, "réglages du plugin rétablis exactement après la lecture des noms");
        bridge.loadPreset (eng.blocks()[0], 2, "Dark");
        check (holy.presetIdx == 2 && std::abs (holy.params[0] - api::presetValue (2, 0)) < 1e-9 && bridge.presetName (eng.blocks()[0]) == "Dark", "preset « Dark » chargé");
        bridge.stepPreset (eng.blocks()[0], 1);
        check (bridge.presetName (eng.blocks()[0]) == "My lead", "preset suivant : My lead");
        gUserIni.deleteFile();
        const int hb = eng.blocks()[0];
        check (bridge.paramSteps (hb, 0) == 0 && bridge.paramSteps (hb, 1) == 4 && bridge.paramSteps (hb, 2) == 2,
               "paramètres à crans : continu / 4 positions / interrupteur");
        // preset utilisateur d'Octopus : état exact du plugin lu puis rétabli
        holy.params = { 0.12, 0.34, 0.56 }; holy.presetIdx = -1;
        String key, data;
        const bool got = bridge.getFxState (hb, key, data);
        holy.params = { 0.9, 0.9, 0.9 }; holy.presetIdx = 1;
        const bool set = bridge.setFxState (hb, key, data);
        check (got && set && key == "vst_chunk" && std::abs (holy.params[0] - 0.12) < 1e-9 && std::abs (holy.params[2] - 0.56) < 1e-9 && holy.presetIdx == -1,
               "preset utilisateur d'Octopus : état du plugin lu (" + key + ") puis rétabli");
    }

    // ---- Eight Echo en STOMP : le footswitch coupe la pédale avec son propre interrupteur « Effect On »
    //      (les répétitions continuent grâce au Carryover), jamais avec le bypass de REAPER (queue coupée net)
    {
        tr.fx.clear(); tr.rec.clear();
        auto o = makeFx ("VST3: Octopus (NINE HANDS | DSP)"); o.token = &token; tr.fx.push_back (o);
        auto ee = makeFx ("VST3: Eight Echo (NINE HANDS | DSP)");
        // liste réelle vue par l'hôte (Eight Echo 1.8.3), puis les paramètres ajoutés par REAPER
        ee.paramNames = { "E.Level", "Mix", "Feedback", "Time", "Mode", "Effect On", "Footswitch (hold)", "Tap / Loop Stop",
                          "Twist / Loop Clear", "Carryover", "Stereo Mode", "Tap Division", "Long Delay (x2)", "Direct Mute",
                          "Host Sync", "Expression", "Exp > E.Level", "Exp > Feedback", "Exp > Time", "Bypass", "Program",
                          "Bypass", "Wet", "Delta" };
        ee.paramStepCount = { 0, 0, 0, 0, 11, 2, 2, 2, 2, 2, 3, 7, 2, 2, 2, 0, 2, 2, 2, 2, 26, 2, 0, 2 };
        ee.params.assign ((size_t) ee.paramNames.size(), 0.0);
        ee.params[5] = 1.0; ee.params[9] = 1.0;                    // Effect On, Carryover
        tr.fx.push_back (ee);
        auto hv = makeFx ("VST3: Holyverb (NINE HANDS | DSP)"); tr.fx.push_back (hv);   // reverb ajoutée après le delay
        eng.refreshNow();
        const int eb = eng.blocks()[0];
        bool inv = true;
        check (eng.pedalBypassParam (eb, inv) == 5 && ! inv && eng.usesPedalBypass (eb), "Eight Echo : interrupteur de la pédale trouvé (Effect On)");
        eng.setMode (OctopusEngine::stompMode);
        eng.toggleAssign (0, tr.fx[1].guid);
        eng.press (0);
        check (tr.fx[1].enabled && tr.fx[1].params[5] < 0.5, "STOMP A : Eight Echo coupé par son interrupteur (REAPER le laisse tourner : la queue continue)");
        eng.refreshNow();
        check (! eng.fsLit (0), "STOMP A éteint à l'écran");
        eng.press (0);
        check (tr.fx[1].enabled && tr.fx[1].params[5] > 0.5, "STOMP A : Eight Echo rallumé");
        eng.refreshNow();
        check (eng.fsLit (0), "STOMP A allumé à l'écran");
        eng.setMode (OctopusEngine::presetMode);
    }

    // la piste disparaît (supprimée) : plus prêt, pas de plantage
    gParent = nullptr;
    eng.refreshNow();
    check (! eng.isReady(), "piste absente : " + eng.statusText());

    // voies parallèles (split / merge)
    {
        gResource = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("octo_sim_reaper");
        gResource.deleteRecursively(); gResource.createDirectory();
        gParent = &tr;
        eng.setMode (OctopusEngine::presetMode);
        tr.fx.clear(); tr.rec.clear(); tr.nchan = 2;
        auto o = makeFx ("VST3: Octopus (NINE HANDS | DSP)"); o.token = &token; tr.fx.push_back (o);
        for (auto* n : { "VST3: Doorn (NINE HANDS | DSP)", "VST3: Pipeline (NINE HANDS | DSP)", "VST3: Holyverb (NINE HANDS | DSP)" }) tr.fx.push_back (makeFx (n));
        eng.refreshNow();
        const bool okSplit = eng.split (eng.blocks()[0]);
        check (okSplit && rnames (tr) == "[OCTO] < Doorn |B > Pipeline Holyverb" && tr.nchan == 8
               && gResource.getChildFile ("Effects/NINE HANDS/Routing/nh_merge.jsfx").existsAsFile(),
               "split : Doorn en voie A, voie B vide, piste à 8 canaux, effets JS écrits : " + rnames (tr));
        check (eng.groups().size() == 1 && eng.groups()[0].lanes.size() == 2 && eng.groups()[0].lanes[1].isEmpty()
               && tr.fx[4].params[(size_t) Routing::laneParam (1, Routing::mixPresent)] < 0.5, "groupe lu : 2 voies, voie B vide et muette");
        eng.addFx ({ "VST3: Holyverb (NINE HANDS | DSP)", "" }, eng.laneEndBlock (0, 1));
        check (rnames (tr) == "[OCTO] < Doorn |B Holyverb > Pipeline Holyverb" && eng.laneAt (4) == 1
               && tr.fx[5].params[(size_t) Routing::laneParam (1, Routing::mixPresent)] > 0.5, "ajout au bout de la voie B (la voie n'est plus muette) : " + rnames (tr));
        eng.setMix (0, 1, Routing::mixLevel, -6.0);
        eng.setMix (0, 0, Routing::mixPan, -30.0);
        check (std::abs (eng.mixValue (0, 1, Routing::mixLevel) + 6.0) < 0.05 && std::abs (eng.mixValue (0, 0, Routing::mixPan) + 30.0) < 0.5, "mixeur : niveau B -6 dB, pan A L30");
        eng.toggle (eng.groups()[0].split);
        check (! eng.groups()[0].on && tr.fx[5].params[(size_t) Routing::pGroupOn] < 0.5 && tr.fx[2].enabled, "groupe coupé : signal sec (curseur Group), les blocs gardent leur état");
        eng.toggle (eng.groups()[0].split);
        check (eng.groups()[0].on, "groupe rallumé");
        eng.addPath (0, eng.blocks()[eng.blocks().size() - 2]);      // Pipeline glissé sous la voie B -> voie C
        check (rnames (tr) == "[OCTO] < Doorn |B Holyverb |C Pipeline > Holyverb" && eng.groups()[0].lanes.size() == 3
               && tr.fx[7].params[(size_t) Routing::pPaths] > 0.5, "voie C avec Pipeline : " + rnames (tr));
        eng.moveBlock (eng.groups()[0].lanes[2][0], eng.blocks().indexOf (eng.groups()[0].lanes[1][0]));
        check (rnames (tr) == "[OCTO] < Doorn |B Pipeline Holyverb |C > Holyverb", "Pipeline déplacé dans la voie B : " + rnames (tr));
        eng.toggleAssign (1, eng.swapKeyOf (0));
        eng.setMode (OctopusEngine::stompMode);
        eng.press (1);
        const bool soloA = eng.soloLane (0) == 0;
        eng.press (1);
        const bool soloB = eng.soloLane (0) == 1;
        eng.press (1);
        check (soloA && soloB && eng.soloLane (0) == 0, "footswitch B : A -> B -> A (voie C vide sautée, voies coupées dans Merge)");
        eng.setMode (OctopusEngine::presetMode);
        eng.save (20);
        eng.unsplit (0);
        check (rnames (tr) == "[OCTO] Doorn Pipeline Holyverb Holyverb" && eng.groups().empty(), "voies remises en série : " + rnames (tr));
        eng.load (20);
        check (rnames (tr) == "[OCTO] < Doorn |B Pipeline Holyverb |C > Holyverb" && eng.groups().size() == 1 && eng.groups()[0].lanes.size() == 3,
               "preset rappelé : groupe à 3 voies rétabli : " + rnames (tr));
        check (eng.isAssigned (1, eng.swapKeyOf (0)) && eng.soloLane (0) == 0, "preset rappelé : footswitch A/B et voie choisie gardés");
        eng.toggleAssign (1, eng.swapKeyOf (0));
        eng.removePath (0, 0);
        check (rnames (tr) == "[OCTO] < Pipeline Holyverb |B > Holyverb" && eng.groups()[0].lanes.size() == 2 && eng.groups()[0].lanes[1].isEmpty(),
               "voie A retirée : B devient A, C (vide) devient B : " + rnames (tr));
        eng.removePath (0, 1);
        check (rnames (tr) == "[OCTO] Pipeline Holyverb Holyverb" && eng.groups().empty(), "dernière voie retirée : le groupe disparaît : " + rnames (tr));
        eng.split (eng.blocks()[2], eng.blocks()[0]);                  // Pipeline glissé sous le 2e Holyverb
        check (rnames (tr) == "[OCTO] Holyverb < Holyverb |B Pipeline >", "glisser un bloc sous un autre : split avec ce bloc en voie B : " + rnames (tr));
        eng.toggleFold (0);
        check (eng.isFolded (0) && eng.blockName (eng.groups()[0].split) == "Holyverb || Pipeline", "groupe replié : " + eng.blockName (eng.groups()[0].split));
        gParent = nullptr;
    }

    // nuancier : les réglages de couleur / glow ne sont écrits sur le disque qu'une fois, à la fermeture
    {
        const auto f = OctoSettings::file();
        const bool existed = f.existsAsFile(); const auto before = f.loadFileAsString();
        for (int pc = 0; pc <= 200; pc += 10) OctoSettings::setGlowOf ("simTest", pc);     // curseur qu'on fait glisser
        const bool untouched = f.loadFileAsString() == before && OctoSettings::glowOf ("simTest") == 200;
        OctoSettings::flush();
        const bool written = f.loadFileAsString().contains ("simTestGlow=\"200\"");
        if (existed) f.replaceWithText (before); else f.deleteFile();
        check (untouched && written, "nuancier : glow gardé en mémoire pendant le glissement, écrit une fois à la fermeture");
    }

    std::printf ("%s\n", failures == 0 ? "TOUS LES TESTS OK" : (String (failures) + " ECHEC(S)").toRawUTF8());
    return failures == 0 ? 0 : 1;
}
