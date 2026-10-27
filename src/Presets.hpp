// Kinetik KBAS-1 — acid line generator and factory presets
#pragma once
#include "Bank.hpp"
#include <vector>
#include <initializer_list>

namespace kn {

// ---- ACID GEN: a random line in the scale; notes mostly near the root, octave jumps, slides and accents
struct AcidRng { uint32_t s; explicit AcidRng(uint32_t seed) : s(seed ? seed : 1u) {} float u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFFFF) / 16777216.f; } };
inline void acidGen(Pattern& P, int scale, int root, uint32_t seed, float density = 0.7f, float slides = 0.25f, float accents = 0.3f)
{
    AcidRng r(seed);
    const int base = 36 + root;                          // root around C1..B1
    std::vector<int> pool;
    for (int n = base - 5; n <= base + 14; ++n) if (inScale(n, root, scale)) pool.push_back(n);
    const int L = P.length;
    int last = base;
    for (int s = 0; s < L; ++s) {
        P.on[s] = (s % 4 == 0) ? (r.u() < 0.9f) : (r.u() < density);
        // a walk around the root: small moves, sometimes a jump; the downbeats like the root
        int n;
        if (s % 8 == 0 && r.u() < 0.6f) n = base;
        else {
            int idx = 0; for (int i = 0; i < (int)pool.size(); ++i) if (pool[i] == last) idx = i;
            const int step = r.u() < 0.7f ? (int)(r.u() * 5.f) - 2 : (int)(r.u() * 9.f) - 4;
            idx = clampi(idx + step, 0, (int)pool.size() - 1);
            n = pool[idx];
        }
        if (r.u() < 0.18f) n += 12;                      // octave up
        P.note[s] = (uint8_t)clampi(n, 0, 127); last = clampi(n, base - 5, base + 14);
        P.accent[s] = P.on[s] && r.u() < accents;
        P.slide[s] = P.on[s] && r.u() < slides;
    }
}

struct PB {
    float* v; Bank& b;
    PB(float* values, Bank& bank) : v(values), b(bank) { for (int i = 0; i < kSavedParams; ++i) v[i] = kParamDefaults[i]; b.initDefault(); }
    void key(int root, int scale) { b.glob.root = (uint8_t)root; b.glob.scale = (uint8_t)scale; }
    void set(std::initializer_list<std::pair<int, float>> kv) { for (auto& p : kv) v[p.first] = p.second; }
    // notes: one token per step: "-" rest, "A1" note, suffix "!" accent, "~" slide into the next step ("C2!~")
    void line(int p, const char* s)
    {
        Pattern& P = b.patterns[p];
        int st = 0;
        const char* c = s;
        while (*c && st < kSteps) {
            while (*c == ' ') ++c;
            if (!*c) break;
            if (*c == '-') { P.on[st] = 0; ++c; ++st; continue; }
            static const int semi[7] = {9, 11, 0, 2, 4, 5, 7};   // A B C D E F G
            int n = semi[(*c - 'A') % 7]; ++c;
            if (*c == '#') { ++n; ++c; }
            const int oct = *c - '0'; ++c;
            P.on[st] = 1; P.note[st] = (uint8_t)clampi(n + (oct + 2) * 12, 0, 127);
            while (*c == '!' || *c == '~') { if (*c == '!') P.accent[st] = 1; else P.slide[st] = 1; ++c; }
            ++st;
        }
        P.length = (uint8_t)std::max(1, st);
    }
};

static void pInit(PB&) {}
static void pAcidHouse(PB& b)
{
    b.key(9, 1);
    b.set({{pCUTOFF, 38}, {pRESO, 92}, {pENVMOD, 78}, {pDECAY, 52}, {pACCENT, 80}, {pTEMPO, 124}});
    b.line(0, "A1 A1 A2! - A1 G1~ A1 - A1 C2! A1 - E2~ D2 A1! -");
}
static void pSquelch(PB& b)
{
    b.key(0, 1);
    b.set({{pCUTOFF, 26}, {pRESO, 112}, {pENVMOD, 100}, {pDECAY, 40}, {pACCENT, 110}, {pTEMPO, 128}});
    b.line(0, "C1 C1! C2 C1~ D#1 C1 F1! C1 C1 G1~ C2 C1! A#1 C1 C2~ G1");
}
static void pAcidTechno(PB& b)
{
    b.key(4, 2);
    b.set({{pCUTOFF, 30}, {pRESO, 100}, {pENVMOD, 90}, {pDECAY, 30}, {pACCENT, 90}, {pDRIVE, 70}, {pTONE, 70}, {pTEMPO, 138}});
    b.line(0, "E1 E1 E2! E1 E1 G1~ E1 E1! D2 E1 E1 B1~ E1 E1 E2! F1");
}
static void pRubber(PB& b)
{
    b.key(5, 1);
    b.set({{pWAVE, 127}, {pCUTOFF, 34}, {pRESO, 60}, {pENVMOD, 50}, {pDECAY, 80}, {pACCENT, 50}, {pSLIDE, 70}, {pTEMPO, 118}});
    b.line(0, "F1~ F1 - F1 G#1~ G1 - F1 - C2~ A#1 - F1 - D#1~ F1");
}
static void pSquareBubbles(PB& b)
{
    b.key(7, 1);
    b.set({{pWAVE, 127}, {pCUTOFF, 44}, {pRESO, 104}, {pENVMOD, 64}, {pDECAY, 36}, {pACCENT, 70}, {pDLYMIX, 40}, {pDLYTIME, 104}, {pDLYFB, 60}, {pTEMPO, 122}});
    b.line(0, "G1 - G2! - D2 - G1~ A#1 - G1 F2! - D2~ C2 - G1");
}
static void pPhrygianRun(PB& b)
{
    b.key(4, 2);
    b.set({{pCUTOFF, 32}, {pRESO, 96}, {pENVMOD, 84}, {pDECAY, 46}, {pACCENT, 85}, {pTEMPO, 130}});
    b.line(0, "E1 F1 E1 G1! E1 A1~ G1 F1 E1 F1! E2 E1 B1~ C2 A1 F1");
}
static void pSlideParty(PB& b)
{
    b.key(9, 1);
    b.set({{pCUTOFF, 40}, {pRESO, 88}, {pENVMOD, 70}, {pDECAY, 60}, {pACCENT, 75}, {pSLIDE, 55}, {pTEMPO, 126}});
    b.line(0, "A1~ C2~ E2~ A2! G2~ E2~ D2~ C2 A1~ E2~ G2~ A2! C3~ A2~ E2~ C2");
}
static void pDubAcid(PB& b)
{
    b.key(2, 1);
    b.set({{pCUTOFF, 36}, {pRESO, 98}, {pENVMOD, 66}, {pDECAY, 70}, {pACCENT, 70}, {pDLYMIX, 70}, {pDLYTIME, 104}, {pDLYFB, 82}, {pTONE, 60}, {pTEMPO, 120}});
    b.line(0, "D1! - - D2~ C2 - - A1 - - D1! - F1~ G1 - -");
}
static void pMinimalPulse(PB& b)
{
    b.key(0, 1);
    b.set({{pCUTOFF, 48}, {pRESO, 70}, {pENVMOD, 40}, {pDECAY, 20}, {pACCENT, 60}, {pAMPDEC, 60}, {pTEMPO, 126}});
    b.line(0, "C1 - C1 - C1! - C1 - C1 - C2 - C1! - C1 -");
}
static void pAccentWave(PB& b)
{
    b.key(9, 1);
    b.set({{pCUTOFF, 28}, {pRESO, 108}, {pENVMOD, 60}, {pDECAY, 44}, {pACCENT, 127}, {pTEMPO, 132}});
    b.line(0, "A1! A1! A1 A1 A1! A1! A1 A1 A1! A1! A1! A1 A1 A1! A1! A1!");
}
static void pDistortedLead(PB& b)
{
    b.key(9, 1);
    b.set({{pCUTOFF, 50}, {pRESO, 90}, {pENVMOD, 80}, {pDECAY, 56}, {pACCENT, 90}, {pDRIVE, 110}, {pTONE, 64}, {pTUNE, 64}, {pTEMPO, 135}});
    b.line(0, "A2 A2 C3! A2~ G2 A2 - E3! D3~ C3 A2 - G2~ A2 C3! -");
}
static void pLongLine(PB& b)
{
    b.key(7, 1);
    b.set({{pCUTOFF, 34}, {pRESO, 96}, {pENVMOD, 76}, {pDECAY, 50}, {pACCENT, 85}, {pTEMPO, 128}});
    b.line(0, "G1 G1 G2! G1 F1~ G1 - A#1 G1 G1 D2! G1 F1 G1~ A#1 C2 G1 G1 G2! G1 D#2~ D2 - G1 G1 F2! G1 D2 C2~ A#1 G1 F1!");
}

static void pTalkingAcid(PB& b)
{
    b.key(9, 1);
    b.set({{pCUTOFF, 30}, {pRESO, 90}, {pENVMOD, 100}, {pDECAY, 60}, {pACCENT, 90}, {pVOWEL, 100}, {pTEMPO, 126}});
    b.line(0, "A1 A1! C2 A1~ E2 A1 G1! A1 A1~ D2 A1 C2! A1 E2~ G2 A1");
}
static void pEvolvingLine(PB& b)
{
    b.key(7, 2);
    b.set({{pCUTOFF, 34}, {pRESO, 96}, {pENVMOD, 80}, {pDECAY, 46}, {pACCENT, 85}, {pEVOLVE, 50}, {pEVORESET, 75}, {pDLYMIX, 30}, {pDLYTIME, 104}, {pTEMPO, 132}});
    b.line(0, "G1 G1 G#1 G1! G1 A#1~ G1 G1 D2! G1 F1 G1~ G#1 G1 G2! G1");
}
static void pPumpingAcid(PB& b)
{
    b.key(2, 1);
    b.set({{pCUTOFF, 40}, {pRESO, 100}, {pENVMOD, 70}, {pDECAY, 40}, {pACCENT, 80}, {pPUMP, 100}, {pSUB, 60}, {pDRIVE, 50}, {pTEMPO, 128}});
    b.line(0, "D1 D1 D2! D1 D1 F1~ D1 D1! C2 D1 D1 A1~ D1 D1 D2! D1");
}
static void pPolymeter(PB& b)
{
    b.key(4, 2);
    b.set({{pCUTOFF, 32}, {pRESO, 104}, {pENVMOD, 90}, {pDECAY, 40}, {pACCENT, 110}, {pSLIDE, 50}, {pGROWL, 30}, {pTEMPO, 134}});
    b.line(0, "E1 E1 F1 E1 G1 E1 E2 E1 F1 E1 B1 E1 E1 C2 E1 F1");
    Pattern& P = b.b.patterns[0];
    for (int s = 0; s < 16; ++s) { P.accent[s] = 0; P.slide[s] = 0; }
    P.accent[0] = 1; P.accent[3] = 1; P.accLen = 5;          // accents turn every 5 steps
    P.slide[2] = 1; P.slideLen = 7;                          // a slide every 7
    P.ratchet[15] = 2; P.prob[6] = 50; P.prob[13] = 60;
}
static void pHarmonyLead(PB& b)
{
    b.key(9, 1);
    b.set({{pCUTOFF, 52}, {pRESO, 80}, {pENVMOD, 70}, {pDECAY, 56}, {pACCENT, 80}, {pHARMONY, 75}, {pDLYMIX, 40}, {pDLYTIME, 104}, {pDLYTIMER, 88}, {pTUNE, 64}, {pTEMPO, 124}});
    b.line(0, "A2 - C3! A2~ E3 - D3 C3~ A2 - G2! A2 - E3~ D3 C3");
}

struct PresetDef { const char* name; const char* desc; void (*build)(PB&); };
static const PresetDef kPresets[] = {
    {"Init",           "Base sound, empty pattern",                              pInit},
    {"Acid House",     "Classic 124 BPM line, octave jumps and slides",          pAcidHouse},
    {"Squelch",        "Closed filter, high resonance, hard accents",            pSquelch},
    {"Acid Techno",    "138 BPM, short decay, driven",                           pAcidTechno},
    {"Rubber",         "Square wave, slow slides, round",                        pRubber},
    {"Square Bubbles", "Square wave with resonance and a dotted echo",           pSquareBubbles},
    {"Phrygian Run",   "Dark phrygian sixteenths",                               pPhrygianRun},
    {"Slide Party",    "Almost every step slides",                               pSlideParty},
    {"Dub Acid",       "Sparse notes in a long ping-pong echo",                  pDubAcid},
    {"Minimal Pulse",  "Short eighth notes, tight amp",                          pMinimalPulse},
    {"Accent Wave",    "One note, the accents make the line",                    pAccentWave},
    {"Distorted Lead", "High line through heavy drive",                          pDistortedLead},
    {"Long Line",      "A 32-step line",                                         pLongLine},
    {"Talking Acid",   "The vowel filter: the line seems to speak",              pTalkingAcid},
    {"Evolving Line",  "EVOLVE changes it each loop, back every 4 loops",        pEvolvingLine},
    {"Pumping Acid",   "Ducked on every beat, with sub and drive",               pPumpingAcid},
    {"Polymeter",      "Accents every 5, slides every 7, chance and repeats",    pPolymeter},
    {"Harmony Lead",   "A second voice a fifth up, echoes of two lengths",       pHarmonyLead},
};
static constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

inline void buildPreset(int i, float* values, Bank& bank)
{
    PB b(values, bank);
    if (i >= 0 && i < kNumPresets) kPresets[i].build(b);
}

} // namespace kn
