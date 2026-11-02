// Kinetik KBAS-1 — acid line generator and factory presets
#pragma once
#include <string>
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

// ---- DENSITY (0.4): a generated line keeps every step's note, accent and slide plus a rank; DENSITY turns on the
// steps of lowest rank. Downbeats get low ranks (they stay longest), then the 8ths, then the 16ths: turning DENSITY
// down leaves the skeleton of the groove, turning it up adds notes back in the same order.
struct GenLine {
    int pattern = -1;                     // the pattern it was generated into (-1 = none)
    float rank[kSteps];
    uint8_t acc[kSteps], slide[kSteps];
};
inline int densityCount(int length, int pct) { return clampi((int)std::lround(length * clampi(pct, 0, 100) / 100.0), 0, length); }
inline void applyDensity(Pattern& P, const GenLine& g, int pct)
{
    const int L = P.length, want = densityCount(L, pct);
    for (int s = 0; s < L; ++s) {
        int below = 0;                    // steps ranked before this one
        for (int t = 0; t < L; ++t) if (g.rank[t] < g.rank[s] || (g.rank[t] == g.rank[s] && t < s)) ++below;
        const bool on = below < want;
        P.on[s] = on; P.accent[s] = on && g.acc[s]; P.slide[s] = on && g.slide[s];
        if (on) P.tie[s] = 0;
    }
}
// ACID GEN with a DENSITY: a full line, ranked, then thinned
inline void acidGenDense(Pattern& P, GenLine& g, int pattern, int scale, int root, uint32_t seed, int pct, float slides = 0.25f, float accents = 0.3f)
{
    acidGen(P, scale, root, seed, 1.f, 0.f, 0.f);
    AcidRng r(seed * 2654435761u + 7u);
    for (int s = 0; s < kSteps; ++s) {
        const float w = s % 4 == 0 ? 0.2f : (s % 2 == 0 ? 0.55f : 1.f);
        g.rank[s] = r.u() * w;
        g.acc[s] = r.u() < accents; g.slide[s] = r.u() < slides;
    }
    g.pattern = pattern;
    applyDensity(P, g, pct);
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

// ---- 0.5 factory presets: 4-bar pieces (64 steps) with per-step locks, chance, repeats, ties, polymetric accent /
// slide rows, LFOs, saturation, echo / reverb / CHAOS, and a second pattern in a CHAIN where it helps.
inline std::string bars(std::initializer_list<const char*> bs) { std::string s; for (auto x : bs) { s += x; s += ' '; } return s; }
struct PB2 {
    PB& p;
    explicit PB2(PB& b) : p(b) {}
    Pattern& P(int i) { return p.b.patterns[i]; }
    void line(int i, const std::string& s, int swing = 50) { p.line(i, s.c_str()); P(i).swing = (uint8_t)swing; }
    void lock(int i, int s, int prm, int v) { P(i).locks[s][prm] = (uint8_t)clampi(v, 0, 127); }
    // a value cycling over the notes of a pattern (any lockable param)
    void cyc(int i, int prm, std::initializer_list<int> vals)
    {
        const int n = (int)vals.size(); int k = 0; const int* a = vals.begin();
        for (int s = 0; s < P(i).length; ++s) if (P(i).on[s]) lock(i, s, prm, a[k++ % n]);
    }
    void prob(int i, std::initializer_list<int> steps, int pct) { for (int s : steps) P(i).prob[s] = (uint8_t)pct; }
    void rat(int i, int s, int n) { P(i).ratchet[s] = (uint8_t)n; }
    void tie(int i, std::initializer_list<int> steps) { for (int s : steps) { P(i).tie[s] = 1; P(i).on[s] = 1; P(i).note[s] = P(i).note[s > 0 ? s - 1 : 0]; } }
    // LFO l: wave TRI SINE SQUARE SAW UP SAW DN RANDOM, synced division (kLfoSyncNames), depth -63..+63, target param, retrig
    void lfo(int l, int wave, int div, int depth, int target, bool retrig = false)
    {
        p.v[lfoParam(l, 0)] = (float)(wave * 128 / kLfoWaves + 4);
        p.v[lfoParam(l, 1)] = (float)(div * 128 / kLfoSyncDivs + 2);
        p.v[lfoParam(l, 2)] = 127.f;
        p.v[lfoParam(l, 3)] = (float)clampi(64 + depth, 0, 127);
        p.v[lfoParam(l, 4)] = lfoDestKnob(lfoTargetSlot(target) + 1);
        p.v[lfoParam(l, 5)] = retrig ? 127.f : 0.f;
    }
    void copy(int src, int dst) { p.b.patterns[dst] = p.b.patterns[src]; }
    void chain(const char* spec) { p.b.chain.parse(spec, kPatterns); }
};
enum { lsd8Bars, lsd4Bars, lsd2Bars, lsd1Bar, lsdHalf, lsd4D, lsd4, lsd4T, lsd8D, lsd8, lsd8T, lsd16, lsd16T, lsd32 };
enum { dt32, dt16T, dt16, dt8T, dt16D, dt8, dt8D, dt4 };
static float dtv(int bin) { return bin * 16.f + 8.f; }   // echo TIME value of a division
static float satv(int type) { return type * 128.f / kSatTypes + 10.f; }   // TUBE TAPE DIODE FUZZ FOLD

static void pInit(PB&) {}
// Acid House 124, A minor: the line answers itself over 4 bars, the filter opens in bar 4; B = octave jumps and repeats.
static void pAcidHouse(PB& b)
{
    PB2 q(b);
    b.key(9, 1);
    b.set({{pCUTOFF, 38}, {pRESO, 92}, {pENVMOD, 78}, {pDECAY, 52}, {pACCENT, 80}, {pDLYTIME, dtv(dt8D)}, {pDLYFB, 50}, {pDLYMIX, 26},
           {pREVERB, 20}, {pSAT, 30}, {pTEMPO, 124}});
    q.line(0, bars({"A1 A1 A2! - A1 G1~ A1 - A1 C2! A1 - E2~ D2 A1! -", "A1 A1 A2! - A1 G1~ A1 - A1 C2! A1 - G1~ A1 E1 -",
                    "A1 A1 A2! - A1 G1~ A1 - A1 C2! A1 - E2~ D2 A1! -", "A1 A1 A2! - C2 D2~ E2 - G2! E2 D2 C2~ A1 G1 E1! -"}), 56);
    q.cyc(0, pCUTOFF, {38, 38, 40, 38, 42, 40, 38, 44});
    for (int s = 48; s < 64; ++s) if (q.P(0).on[s]) q.lock(0, s, pCUTOFF, 44 + (s - 48) * 2);
    q.prob(0, {6, 22, 38}, 70);
    q.copy(0, 1);
    q.line(1, bars({"A1 A2 A1 A2! - A1 G1~ A1 A1 C3! A1 - E2~ D2 A1! A2", "A1 A2 A1 A2! - A1 G1~ A1 A1 C3! A1 - G2~ A2 E2 -",
                    "A1 A2 A1 A2! - A1 G1~ A1 A1 C3! A1 - E2~ D2 A1! A2", "A1 A1 A2! - C2 D2~ E2 - G2! E2 D2 C2~ A1 G1 E1! -"}), 56);
    q.rat(1, 15, 2); q.rat(1, 47, 3);
    q.cyc(1, pENVMOD, {78, 84, 90, 96, 90, 84});
    q.lfo(0, 1, lsd4Bars, 14, pCUTOFF);
    q.chain("1 2 0:2 1:2");
}
// Squelch 128, C minor: closed filter, high resonance, accents and ratchets that the DECAY locks shape.
static void pSquelch(PB& b)
{
    PB2 q(b);
    b.key(0, 1);
    b.set({{pCUTOFF, 26}, {pRESO, 112}, {pENVMOD, 100}, {pDECAY, 40}, {pACCENT, 110}, {pDRIVE, 30}, {pTEMPO, 128}});
    q.line(0, bars({"C1 C1! C2 C1~ D#1 C1 F1! C1 C1 G1~ C2 C1! A#1 C1 C2~ G1", "C1 C1! C2 C1~ D#1 C1 F1! C1 C1 G1~ C2 C1! G1 F1 D#1~ C1",
                    "C1 C1! C2 C1~ D#1 C1 F1! C1 C1 G1~ C2 C1! A#1 C1 C2~ G1", "C1 C1! C2 C1~ D#1 C1 F1! C1 G1 G1! A#1 C2! D#2 C2~ A#1 G1!"}), 54);
    q.cyc(0, pDECAY, {40, 30, 50, 36, 60});
    q.rat(0, 9, 2); q.rat(0, 41, 2); q.rat(0, 57, 3); q.rat(0, 63, 4);
    q.prob(0, {4, 20, 36, 52}, 60);
    q.lfo(0, 0, lsd2Bars, 10, pCUTOFF); q.lfo(1, 5, lsd16, 8, pRESO);
}
// Acid Techno 138, E phrygian: driven TUBE line, every 4th bar the GROWL lock bites, pump on the beat.
static void pAcidTechno(PB& b)
{
    PB2 q(b);
    b.key(4, 2);
    b.set({{pCUTOFF, 30}, {pRESO, 100}, {pENVMOD, 90}, {pDECAY, 30}, {pACCENT, 90}, {pDRIVE, 70}, {pTONE, 70}, {pSAT, 50}, {pSATTYPE, satv(0)},
           {pPUMP, 40}, {pLOWCUT, 20}, {pTEMPO, 138}});
    q.line(0, bars({"E1 E1 E2! E1 E1 G1~ E1 E1! D2 E1 E1 B1~ E1 E1 E2! F1", "E1 E1 E2! E1 E1 G1~ E1 E1! D2 E1 E1 B1~ E1 F1 E1! E1",
                    "E1 E1 E2! E1 E1 G1~ E1 E1! D2 E1 E1 B1~ E1 E1 E2! F1", "E1 E1 E2! E1 F1 G1~ A1 B1! C2 B1 A1 G1~ F1 E1 E2! E2!"}), 52);
    for (int s = 48; s < 64; ++s) if (q.P(0).on[s]) q.lock(0, s, pGROWL, 60);
    q.rat(0, 63, 3);
    q.lfo(0, 3, lsd1Bar, 16, pCUTOFF, false);
}
// Rubber Dub 116, F minor: square wave, slow slides and ties, a dotted echo that the LFO opens, hall reverb.
static void pRubber(PB& b)
{
    PB2 q(b);
    b.key(5, 1);
    b.set({{pWAVE, 127}, {pCUTOFF, 34}, {pRESO, 60}, {pENVMOD, 50}, {pDECAY, 80}, {pACCENT, 50}, {pSLIDE, 70}, {pDLYTIME, dtv(dt8D)}, {pDLYFB, 70},
           {pDLYMIX, 40}, {pREVERB, 40}, {pREVSIZE, 80}, {pSUB, 40}, {pTEMPO, 116}});
    q.line(0, bars({"F1~ F1 - F1 G#1~ G1 - F1 - C2~ A#1 - F1 - D#1~ F1", "F1~ F1 - F1 G#1~ G1 - F1 - C2 - - C#2~ C2 A#1 -",
                    "F1~ F1 - F1 G#1~ G1 - F1 - C2~ A#1 - F1 - D#1~ F1", "C#1 - - - C#1~ D#1 - F1 - G#1~ G1 - F1 - - -"}), 58);
    q.tie(0, {50, 51, 61, 62, 63});
    q.lfo(0, 1, lsd4Bars, 20, pDLYMIX); q.lfo(1, 0, lsd2Bars, 10, pCUTOFF);
}
// Phrygian Run 130: dark sixteenths with chance on the off-notes and a TAPE saturation; B climbs.
static void pPhrygianRun(PB& b)
{
    PB2 q(b);
    b.key(4, 2);
    b.set({{pCUTOFF, 32}, {pRESO, 96}, {pENVMOD, 84}, {pDECAY, 46}, {pACCENT, 85}, {pSAT, 40}, {pSATTYPE, satv(1)}, {pDLYTIME, dtv(dt16D)}, {pDLYFB, 40}, {pDLYMIX, 20}, {pTEMPO, 130}});
    q.line(0, bars({"E1 F1 E1 G1! E1 A1~ G1 F1 E1 F1! E2 E1 B1~ C2 A1 F1", "E1 F1 E1 G1! E1 A1~ G1 F1 E1 F1! E2 E1 D2~ C2 B1 G1",
                    "E1 F1 E1 G1! E1 A1~ G1 F1 E1 F1! E2 E1 B1~ C2 A1 F1", "E1 F1 G1 A1! B1 C2~ D2 E2 F2! E2 D2 C2~ B1 A1 G1 F1!"}), 52);
    q.prob(0, {1, 7, 17, 23, 33, 39}, 65);
    q.copy(0, 1);
    for (int s = 0; s < 64; ++s) if (q.P(1).on[s] && (s / 16) % 2) q.P(1).note[s] = (uint8_t)(q.P(1).note[s] + 12);
    q.cyc(1, pENVMOD, {84, 92, 100});
    q.chain("1 2 0:3 1:1");
}
// Talking Acid 126: the vowel filter, moved by a synced LFO, so the line says something different every beat.
static void pTalkingAcid(PB& b)
{
    PB2 q(b);
    b.key(9, 1);
    b.set({{pCUTOFF, 30}, {pRESO, 90}, {pENVMOD, 100}, {pDECAY, 60}, {pACCENT, 90}, {pVOWEL, 100}, {pDLYTIME, dtv(dt8D)}, {pDLYFB, 40}, {pDLYMIX, 20}, {pTEMPO, 126}});
    q.line(0, bars({"A1 A1! C2 A1~ E2 A1 G1! A1 A1~ D2 A1 C2! A1 E2~ G2 A1", "A1 A1! C2 A1~ E2 A1 G1! A1 A1~ D2 A1 C2! G1 E1~ G1 A1",
                    "A1 A1! C2 A1~ E2 A1 G1! A1 A1~ D2 A1 C2! A1 E2~ G2 A1", "A1 - C2! - E2 - G2! - A2~ G2 E2 D2! C2~ A1 G1 E1"}), 55);
    q.lfo(0, 1, lsd4, 40, pCUTOFF); q.lfo(1, 0, lsd2Bars, 24, pVOWEL);
    q.cyc(0, pVOWEL, {100, 100, 80, 110});
}
// Evolving Line 132, G phrygian: EVOLVE mutates the line every loop and comes back every 4 loops.
static void pEvolvingLine(PB& b)
{
    PB2 q(b);
    b.key(7, 2);
    b.set({{pCUTOFF, 34}, {pRESO, 96}, {pENVMOD, 80}, {pDECAY, 46}, {pACCENT, 85}, {pEVOLVE, 50}, {pEVORESET, 75}, {pDLYMIX, 30}, {pDLYTIME, dtv(dt8D)},
           {pREVERB, 24}, {pTEMPO, 132}});
    q.line(0, bars({"G1 G1 G#1 G1! G1 A#1~ G1 G1 D2! G1 F1 G1~ G#1 G1 G2! G1", "G1 G1 G#1 G1! G1 A#1~ G1 G1 D2! G1 F1 G1~ D#2 D2 C2! A#1"}), 54);
    q.lfo(0, 0, lsd2Bars, 12, pCUTOFF);
}
// Pumping Acid 128, D minor: ducked on every beat, sub and drive, a resonant LOW CUT that an LFO sweeps.
static void pPumpingAcid(PB& b)
{
    PB2 q(b);
    b.key(2, 1);
    b.set({{pCUTOFF, 40}, {pRESO, 100}, {pENVMOD, 70}, {pDECAY, 40}, {pACCENT, 80}, {pPUMP, 100}, {pSUB, 60}, {pDRIVE, 50}, {pLOWCUT, 30}, {pLCRESO, 60}, {pTEMPO, 128}});
    q.line(0, bars({"D1 D1 D2! D1 D1 F1~ D1 D1! C2 D1 D1 A1~ D1 D1 D2! D1", "D1 D1 D2! D1 D1 F1~ D1 D1! C2 D1 D1 A1~ G1 F1 E1! D1",
                    "D1 D1 D2! D1 D1 F1~ D1 D1! C2 D1 D1 A1~ D1 D1 D2! D1", "A#0 A#0 A#1! A#0 A#0 D1~ A#0 A#0! A1 A#0 C1 C1~ C2 C1 E1! F1"}), 54);
    q.lfo(0, 0, lsd4Bars, 30, pLOWCUT);
}
// Polymeter 134: 64 notes, accents turn every 5 steps, slides every 7, chance and repeats.
static void pPolymeter(PB& b)
{
    PB2 q(b);
    b.key(4, 2);
    b.set({{pCUTOFF, 32}, {pRESO, 104}, {pENVMOD, 90}, {pDECAY, 40}, {pACCENT, 110}, {pSLIDE, 50}, {pGROWL, 30}, {pDLYTIME, dtv(dt16D)}, {pDLYMIX, 20}, {pTEMPO, 134}});
    q.line(0, bars({"E1 E1 F1 E1 G1 E1 E2 E1 F1 E1 B1 E1 E1 C2 E1 F1", "E1 E1 F1 E1 G1 E1 E2 E1 F1 E1 B1 E1 D2 C2 B1 G1",
                    "E1 E1 F1 E1 G1 E1 E2 E1 F1 E1 B1 E1 E1 C2 E1 F1", "E1 E1 F1 E1 G1 A1 B1 C2 D2 E2 D2 C2 B1 A1 G1 F1"}), 52);
    Pattern& P = q.P(0);
    for (int s = 0; s < 64; ++s) { P.accent[s] = 0; P.slide[s] = 0; }
    P.accent[0] = 1; P.accent[3] = 1; P.accLen = 5;
    P.slide[2] = 1; P.slideLen = 7;
    q.rat(0, 15, 2); q.rat(0, 47, 2); q.rat(0, 63, 3);
    q.prob(0, {6, 13, 22, 29, 38, 45}, 55);
}
// Harmony Lead 124: a second voice a fifth up, echoes of two lengths, chorus-like LFO on the tune.
static void pHarmonyLead(PB& b)
{
    PB2 q(b);
    b.key(9, 1);
    b.set({{pCUTOFF, 52}, {pRESO, 80}, {pENVMOD, 70}, {pDECAY, 56}, {pACCENT, 80}, {pHARMONY, 75}, {pDLYMIX, 40}, {pDLYTIME, dtv(dt8D)}, {pDLYTIMER, 88},
           {pTUNE, 64}, {pREVERB, 40}, {pREVSIZE, 70}, {pTEMPO, 124}});
    q.line(0, bars({"A2 - C3! A2~ E3 - D3 C3~ A2 - G2! A2 - E3~ D3 C3", "A2 - C3! A2~ E3 - D3 C3~ A2 - G2! E2 - G2~ A2 -",
                    "F2 - A2! F2~ C3 - G2 A2~ F2 - E2! F2 - C3~ B2 A2", "G2 - B2! G2~ D3 - C3 B2~ G2 - D3! E3 - - - -"}), 54);
    q.tie(0, {61, 62, 63});
    q.lfo(0, 1, lsd2Bars, 18, pCUTOFF);
}
// Wobble Acid 128, F minor: a 60-step line turning against the bar, two synced LFOs on cutoff and resonance.
static void pWobbleAcid(PB& b)
{
    PB2 q(b);
    b.key(5, 1);
    b.set({{pCUTOFF, 36}, {pRESO, 100}, {pENVMOD, 50}, {pDECAY, 60}, {pACCENT, 80}, {pLOWCUT, 30}, {pLCRESO, 60}, {pTEMPO, 128}});
    q.line(0, bars({"F1 F1~ F1 G#1 F1 F1! C2~ F1 F1 D#2 F1~ F1 F1! G#1 F1", "F1 F1~ F1 G#1 F1 F1! C2~ F1 F1 D#2 F1~ C2 A#1! G#1 G1",
                    "F1 F1~ F1 G#1 F1 F1! C2~ F1 F1 D#2 F1~ F1 F1! G#1 F1", "C#1 C#1~ C#1 F1 C#1 C#1! G#1~ C#1 D#1 G1 D#1~ D#1 G1! A#1 C2"}), 54);
    q.lfo(0, 0, lsd8, 46, pCUTOFF); q.lfo(1, 4, lsd16, 20, pRESO);
}
// Shimmer Chaos 124, C phrygian: a square line in a shimmer reverb; CHAOS repeats and pitches it up.
static void pShimmerChaos(PB& b)
{
    PB2 q(b);
    b.key(0, 2);
    b.set({{pWAVE, 127}, {pCUTOFF, 46}, {pRESO, 92}, {pENVMOD, 70}, {pDECAY, 50}, {pACCENT, 85}, {pREVERB, 70},
           {pREVMODEL, (float)reverbModelValue(rmShimmer)}, {pREVSIZE, 80}, {pGLCHANCE, 70}, {pGLSIZE, 60}, {pGLPITCH, 90}, {pGLMIX, 110}, {pTEMPO, 124}});
    q.line(0, bars({"C2 - C#2 C2! - G1~ C2 - D#2 - C2 C#2! - G#1~ C2 -", "C2 - C#2 C2! - G1~ C2 - F2 - D#2 C#2! - C2~ A#1 -"}), 54);
    q.lfo(0, 1, lsd4Bars, 16, pCUTOFF);
}
// Deep Sub 122, G dorian: long tied notes low in the filter, sub, reverb; accents only at the phrase ends.
static void pDeepSub(PB& b)
{
    PB2 q(b);
    b.key(7, 3);
    b.set({{pCUTOFF, 30}, {pRESO, 70}, {pENVMOD, 40}, {pDECAY, 90}, {pACCENT, 60}, {pSLIDE, 80}, {pSUB, 80}, {pAMPDEC, 127}, {pREVERB, 30}, {pREVSIZE, 70},
           {pDLYTIME, dtv(dt4)}, {pDLYFB, 50}, {pDLYMIX, 18}, {pTEMPO, 122}});
    q.line(0, bars({"G1 - - G1 - - A#1~ C2 - - G1 - D2~ C2 - -", "G1 - - G1 - - A#1~ C2 - - F1 - - E1~ F1 -",
                    "G1 - - G1 - - A#1~ C2 - - G1 - D2~ C2 - -", "F1 - - F1 - - G1~ A1 - - C2! - - A#1~ A1 -"}), 58);
    q.tie(0, {1, 2, 9, 10, 17, 18, 25, 26, 33, 34, 41, 42, 49, 50, 57, 58});
    q.lfo(0, 1, lsd2Bars, 12, pCUTOFF);
}
// Acid Breaks 170, A minor penta: fast, broken, ratchets and chance, a FUZZ saturation, short echo.
static void pAcidBreaks(PB& b)
{
    PB2 q(b);
    b.key(9, 4);
    b.set({{pCUTOFF, 40}, {pRESO, 94}, {pENVMOD, 80}, {pDECAY, 36}, {pACCENT, 100}, {pSAT, 40}, {pSATTYPE, satv(3)}, {pDLYTIME, dtv(dt16)}, {pDLYFB, 30},
           {pDLYMIX, 18}, {pTEMPO, 170}});
    q.line(0, bars({"A1! - A1 C2 - A1 - E2~ D2 - A1! - G1 A1 - -", "A1! - A1 C2 - A1 A2! - G2 E2 - D2~ C2 - A1 -",
                    "A1! - A1 C2 - A1 - E2~ D2 - A1! - G1 A1 - -", "A1! A1 - C2 D2 - E2! - G2~ A2 - G2 E2! D2 C2 A1"}), 52);
    q.rat(0, 3, 2); q.rat(0, 19, 3); q.rat(0, 51, 2); q.rat(0, 63, 4);
    q.prob(0, {7, 13, 29, 45}, 60);
    q.lfo(0, 5, lsd8, 12, pCUTOFF);
}
// Fold Lead 135, D minor: the FOLD saturator, GROWL and a high line; B = the same line with the filter wide open.
static void pFoldLead(PB& b)
{
    PB2 q(b);
    b.key(2, 1);
    b.set({{pCUTOFF, 50}, {pRESO, 84}, {pENVMOD, 76}, {pDECAY, 56}, {pACCENT, 90}, {pSAT, 70}, {pSATTYPE, satv(4)}, {pGROWL, 40}, {pTUNE, 64},
           {pDLYTIME, dtv(dt8D)}, {pDLYFB, 50}, {pDLYMIX, 30}, {pREVERB, 24}, {pTEMPO, 135}});
    q.line(0, bars({"D2 D2 F2! D2~ C2 D2 - A2! G2~ F2 D2 - C2~ D2 F2! -", "D2 D2 F2! D2~ C2 D2 - A2! G2~ F2 E2 - C2~ A1 C2! -",
                    "D2 D2 F2! D2~ C2 D2 - A2! G2~ F2 D2 - C2~ D2 F2! -", "A#1 A#1 D2! A#1~ C2 C2 - E2! F2~ G2 A2 - G2~ F2 E2! -"}), 54);
    q.copy(0, 1); q.cyc(1, pCUTOFF, {64, 70, 76, 70});
    q.lfo(0, 2, lsd4, 10, pSAT);
    q.chain("1 2 0:3 1:1");
}

struct PresetDef { const char* name; const char* desc; void (*build)(PB&); float trimDb = 0.f; };   // trimDb: level so every preset plays at about -15 LUFS
static const PresetDef kPresets[] = {
    {"Init",           "Base sound, empty pattern",                                         pInit},
    {"Acid House",     "124 BPM, 4 bars that open up; B with octave jumps and repeats",     pAcidHouse, -2.3f},
    {"Squelch",        "Closed filter, high resonance, accents, ratchets shaped by DECAY",  pSquelch, -8.9f},
    {"Acid Techno",    "138 BPM, TUBE drive, GROWL in the last bar, pump",                   pAcidTechno, -10.9f},
    {"Rubber Dub",     "Square wave, slow slides and ties, dotted echo, hall",              pRubber, -4.5f},
    {"Phrygian Run",   "Dark sixteenths with chance; B climbs an octave",                   pPhrygianRun, -2.7f},
    {"Talking Acid",   "The vowel filter moved by an LFO: the line seems to speak",         pTalkingAcid, -16.8f},
    {"Evolving Line",  "EVOLVE changes it each loop, back every 4 loops",                   pEvolvingLine, -3.3f},
    {"Pumping Acid",   "Ducked on every beat, sub, drive, a sweeping resonant LOW CUT",     pPumpingAcid, -9.5f},
    {"Polymeter",      "64 notes, accents every 5, slides every 7, chance and repeats",     pPolymeter, -3.9f},
    {"Harmony Lead",   "A second voice a fifth up, echoes of two lengths",                  pHarmonyLead, -2.4f},
    {"Wobble Acid",    "Two synced LFOs on cutoff and resonance",                           pWobbleAcid, -5.7f},
    {"Shimmer Chaos",  "Shimmer reverb and CHAOS repeats rising",                           pShimmerChaos, -3.6f},
    {"Deep Sub",       "Long tied notes, sub and reverb, accents at the phrase ends",       pDeepSub, -3.8f},
    {"Acid Breaks",    "170 BPM pentatonic, ratchets and chance, FUZZ",                     pAcidBreaks, -2.7f},
    {"Fold Lead",      "FOLD saturation and GROWL on a high line; B opens the filter",      pFoldLead, -4.1f},
};
static constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

inline void buildPreset(int i, float* values, Bank& bank)
{
    PB b(values, bank);
    if (i >= 0 && i < kNumPresets) {
        kPresets[i].build(b);
        values[pVOLUME] = std::min(127.f, std::round(values[pVOLUME] * std::pow(10.f, kPresets[i].trimDb / 40.f)));
    }
}

} // namespace kn
