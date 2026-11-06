#pragma once
// Presets de Semitones (NINE HANDS | DSP), les mêmes que dans le plugin Semitones : transposition du son
// en entrée d'Octopus (avant toute la chaîne), comme une pédale de transposition posée en premier.
namespace semi
{
    struct Preset { const char* name; int semis; bool mixOn; float mix; };

    inline const Preset* presets()
    {
        static const Preset list[] = {
            { "Standard",          0, false, 50.0f },
            { "Eb Standard",      -1, false, 50.0f },
            { "D Standard",       -2, false, 50.0f },
            { "C# Standard",      -3, false, 50.0f },
            { "C Standard",       -4, false, 50.0f },
            { "B Standard",       -5, false, 50.0f },
            { "Bb Standard",      -6, false, 50.0f },
            { "Baritone A",       -7, false, 50.0f },
            { "Octave down",     -12, false, 50.0f },
            { "Octave doubler",  -12, true,  50.0f },
            { "Sub octave",      -12, true,  30.0f },
            { "Power fifth",      -7, true,  45.0f },
            { "Capo 1",            1, false, 50.0f },
            { "Capo 2",            2, false, 50.0f },
            { "Capo 3",            3, false, 50.0f },
            { "Capo 5",            5, false, 50.0f },
            { "Capo 7",            7, false, 50.0f },
            { "Fourth harmony",    5, true,  40.0f },
            { "Fifth harmony",     7, true,  40.0f },
            { "Octave up",        12, false, 50.0f },
            { "12-string",        12, true,  30.0f },
        };
        return list;
    }
    constexpr int kCount = 21;
    inline const Preset& get (int i) { return presets()[i < 0 ? 0 : (i >= kCount ? kCount - 1 : i)]; }
}
