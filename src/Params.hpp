// Kinetik KBAS-1 — constants, parameters and value texts (monophonic acid bass)
#pragma once
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include "Dsp.hpp"

namespace kn {

static constexpr int kSteps = 32;         // steps per pattern
static constexpr int kPatterns = 16;

// ---- host parameters (saved ones never move: new ones go just before gOUT_STEP)
enum Param {
    pTUNE = 0, pWAVE, pCUTOFF, pRESO, pENVMOD, pDECAY, pACCENT, pSLIDE,    // voice
    pAMPDEC, pDRIVE, pTONE, pDLYTIME, pDLYFB, pDLYMIX, pVOLUME,             // amp + effects
    pTEMPO, pPLAY, pPATTERN,
    pOUT_STEP, pOUT_GATE, pOUT_PATTERN, pOUT_NOTE,
    kTotalParams
};
static constexpr int kSavedParams = pOUT_STEP;
static const char* const kParamNames[kTotalParams] = {
    "TUNE", "WAVE", "CUTOFF", "RESO", "ENV MOD", "DECAY", "ACCENT", "SLIDE",
    "AMP DECAY", "DRIVE", "TONE", "DELAY TIME", "FEEDBACK", "DELAY MIX", "VOLUME",
    "TEMPO", "PLAY", "PATTERN", "STEP", "GATE", "PLAYING", "NOTE"};
static const float kParamDefaults[kTotalParams] = {
    64, 0, 40, 80, 70, 50, 70, 40,
    127, 0, 80, 64, 50, 0, 100,
    120, 0, 0, -1, 0, 0, 0};

static const double kDelTime[8] = {0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 0.75, 1.0};
static const char* const kDelTimeNames[8] = {"1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/8D", "1/4"};
inline int bin8(float v) { int b = (int)(v / 16.f); return b < 0 ? 0 : (b > 7 ? 7 : b); }

// mappings (shared by the engine and the UI)
inline float cutoffHz(float v) { return 30.f * std::pow(400.f, v / 127.f); }               // 30 Hz .. 12 kHz
inline float decaySec(float v) { return 0.08f * std::pow(30.f, v / 127.f); }                // 80 ms .. 2.4 s
inline float slideSec(float v) { return 0.008f + v / 127.f * 0.25f; }                       // 8 .. 258 ms
inline float ampDecaySec(float v) { return v >= 126.5f ? 1e9f : 0.05f * std::pow(80.f, v / 127.f); }
inline float volumeGain(float v) { return 1.6f * (v / 127.f) * (v / 127.f); }

inline std::string noteName(int n)
{
    static const char* nm[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    char b[16]; std::snprintf(b, sizeof(b), "%s%d", nm[((n % 12) + 12) % 12], n / 12 - 2);   // 60 = C3 (Ableton naming)
    return b;
}

inline std::string paramText(int p, float v)
{
    char b[32];
    switch (p) {
    case pTUNE: { const int st = (int)std::lround((v - 64.f) / 64.f * 12.f); std::snprintf(b, sizeof(b), st ? "%+d ST" : "0 ST", st); return b; }
    case pWAVE: if (v < 1.f) return "SAW"; if (v > 126.f) return "SQUARE"; std::snprintf(b, sizeof(b), "%d%% SQ", (int)std::lround(v / 127.f * 100.f)); return b;
    case pCUTOFF: { const float hz = cutoffHz(v); if (hz >= 1000.f) std::snprintf(b, sizeof(b), "%.1fK", hz / 1000.f); else std::snprintf(b, sizeof(b), "%d HZ", (int)hz); return b; }
    case pDECAY: std::snprintf(b, sizeof(b), "%d MS", (int)std::lround(decaySec(v) * 1000.f)); return b;
    case pSLIDE: std::snprintf(b, sizeof(b), "%d MS", (int)std::lround(slideSec(v) * 1000.f)); return b;
    case pAMPDEC: if (v >= 126.5f) return "HOLD"; std::snprintf(b, sizeof(b), "%d MS", (int)std::lround(ampDecaySec(v) * 1000.f)); return b;
    case pDRIVE: if (v < 0.5f) return "OFF"; std::snprintf(b, sizeof(b), "%d%%", (int)std::lround(v / 127.f * 100.f)); return b;
    case pDLYTIME: return kDelTimeNames[bin8(v)];
    case pDLYMIX: if (v < 0.5f) return "OFF"; std::snprintf(b, sizeof(b), "%d%%", (int)std::lround(v / 127.f * 100.f)); return b;
    case pVOLUME: { const float g = volumeGain(v); if (g < 0.001f) return "-INF"; std::snprintf(b, sizeof(b), "%+.1fdB", 20.f * std::log10(g)); return b; }
    case pTEMPO: std::snprintf(b, sizeof(b), "%.1f", v); return b;
    default: std::snprintf(b, sizeof(b), "%d", (int)std::lround(v)); return b;
    }
}

// scales for the ACID GEN and the note snap
static constexpr int kScales = 8;
static const char* const kScaleNames[kScales] = {"CHROMATIC", "MINOR", "PHRYGIAN", "DORIAN", "MINOR PENTA", "BLUES", "MAJOR", "HARMONIC MIN"};
static const uint16_t kScaleMask[kScales] = {0xFFF, 0x5AD, 0x5AB, 0x6AD, 0x4A9, 0x4E9, 0xAB5, 0x9AD};   // bit n = semitone n from the root
inline bool inScale(int note, int root, int scale) { return (kScaleMask[scale % kScales] >> (((note - root) % 12 + 12) % 12)) & 1; }

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace kn
