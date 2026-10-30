// Kinetik KBAS-1 — constants, parameters and value texts (monophonic acid bass)
#pragma once
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include "Dsp.hpp"

namespace kn {

static constexpr int kSteps = 64;         // steps per pattern (32 before 0.4)
static constexpr int kPatterns = 16;

// ---- host parameters (saved ones never move: new ones go just before gOUT_STEP)
enum Param {
    pTUNE = 0, pWAVE, pCUTOFF, pRESO, pENVMOD, pDECAY, pACCENT, pSLIDE,    // voice
    pAMPDEC, pDRIVE, pTONE, pDLYTIME, pDLYFB, pDLYMIX, pVOLUME,             // amp + effects
    pTEMPO, pPLAY, pPATTERN,
    pVOWEL, pEVOLVE, pEVORESET, pPUMP, pPUMPRATE,                          // 0.2: vowel filter, line mutation, pump
    pSUB, pGROWL, pHARMONY, pDLYTIMER,                                     // 0.2: sub, filter FM, second voice, right echo time
    pSAT, pSATTYPE,                                                        // 0.3: saturator after the filter
    pLOWCUT, pLCRESO,                                                      // 0.4: resonant low cut
    pREVERB, pREVMODEL, pREVSIZE, pREVDAMP, pREVPRE, pREVMOD,              // 0.4: reverb (KDSP-16 engine)
    pGLCHANCE, pGLSIZE, pGLLEN, pGLPITCH, pGLFADE, pGLREV, pGLMIX,         // 0.4: CHAOS (KDSP-16 glitch)
    pL1WAVE, pL1RATE, pL1SYNC, pL1DEPTH, pL1DEST, pL1TRIG,                 // 0.4: two LFOs
    pL2WAVE, pL2RATE, pL2SYNC, pL2DEPTH, pL2DEST, pL2TRIG,
    pOUT_STEP, pOUT_GATE, pOUT_PATTERN, pOUT_NOTE,
    kTotalParams
};
static constexpr int kSavedParams = pOUT_STEP;
static constexpr int kLockParams = pVOLUME + 1;    // sound parameters that steps can lock (TUNE .. VOLUME)
static constexpr uint8_t kNoLock = 255;
static const char* const kParamNames[kTotalParams] = {
    "TUNE", "WAVE", "CUTOFF", "RESO", "ENV MOD", "DECAY", "ACCENT", "SLIDE",
    "AMP DECAY", "DRIVE", "TONE", "DELAY TIME", "FEEDBACK", "DELAY MIX", "VOLUME",
    "TEMPO", "PLAY", "PATTERN", "VOWEL", "EVOLVE", "EVO RESET", "PUMP", "PUMP RATE", "SUB", "GROWL", "HARMONY", "R TIME", "SATURATE", "SAT TYPE",
    "LOW CUT", "LC RESO", "REVERB", "MODEL", "SIZE", "DAMPING", "PRE-DELAY", "MOD",
    "CHANCE", "SIZE", "LENGTH", "PITCH", "FADE", "REVERSE", "MIX",
    "WAVE", "RATE", "SYNC", "DEPTH", "DEST", "TRIG", "WAVE", "RATE", "SYNC", "DEPTH", "DEST", "TRIG",
    "STEP", "GATE", "PLAYING", "NOTE"};
static const float kParamDefaults[kTotalParams] = {
    64, 0, 40, 80, 70, 50, 70, 40,
    127, 0, 80, 64, 50, 0, 100,
    120, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 52, 70, 64, 10, 30,                   // low cut, reverb (HALL)
    0, 70, 50, 64, 30, 0, 127,                     // CHAOS
    0, 64, 0, 64, 0, 0, 0, 64, 0, 64, 0, 0,        // LFOs (depth 64 = none)
    -1, 0, 0, 0};
static constexpr int kLfos = 2, kLfoParams = 6;
inline int lfoParam(int i, int k) { return pL1WAVE + i * kLfoParams + k; }

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

static const int kEvoReset[5] = {0, 2, 4, 8, 16};          // loops before the line returns to the written one (0 = never)
inline int evoResetLoops(float v) { const int b = (int)(v / 25.6f); return kEvoReset[b < 0 ? 0 : (b > 4 ? 4 : b)]; }
// HARMONY: a second oscillator this many scale degrees away (through the same filter); 0 = off
static constexpr int kHarmonies = 7;
static const int kHarmDegrees[kHarmonies] = {0, -7, 2, 3, 4, 5, 7};
static const char* const kHarmNames[kHarmonies] = {"OFF", "-OCT", "3RD", "4TH", "5TH", "6TH", "+OCT"};
inline int harmBin(float v) { const int b = (int)(v / 128.f * kHarmonies); return b < 0 ? 0 : (b >= kHarmonies ? kHarmonies - 1 : b); }
// R TIME: 0 = same as the left echo, otherwise its own time (bins)
inline int delayRBin(float v) { return v < 8.f ? -1 : bin8((v - 8.f) * 128.f / 120.f); }
static const double kPumpBeats[4] = {1.0, 0.5, 2.0, 4.0};
static const char* const kPumpNames[4] = {"1/4", "1/8", "1/2", "1 BAR"};
inline int pumpBin(float v) { const int b = (int)(v / 32.f); return b < 0 ? 0 : (b > 3 ? 3 : b); }

// SATURATE models (after the filter, 2x oversampled)
static constexpr int kSatTypes = 5;
static const char* const kSatNames[kSatTypes] = {"TUBE", "TAPE", "DIODE", "FUZZ", "FOLD"};
inline int satBin(float v) { const int b = (int)(v / 128.f * kSatTypes); return b < 0 ? 0 : (b >= kSatTypes ? kSatTypes - 1 : b); }

// LOW CUT: 0 = off, then 20 Hz .. 2 kHz (12 dB / oct, resonant)
inline float lowCutHz(float v) { return 20.f * std::pow(100.f, clampf((v - 1.f) / 126.f, 0.f, 1.f)); }

// ---- LFOs: what they can move (DEST 0 = none, 1 + index in this list)
static constexpr int kLfoTargets = 21;
static const uint8_t kLfoTarget[kLfoTargets] = {pCUTOFF, pRESO, pENVMOD, pDECAY, pTUNE, pWAVE, pACCENT, pAMPDEC, pDRIVE, pTONE,
    pDLYFB, pDLYMIX, pVOLUME, pVOWEL, pGROWL, pSUB, pSAT, pLOWCUT, pLCRESO, pREVERB, pPUMP};
static constexpr int kLfoWaves = 6;
static const char* const kLfoWaveNames[kLfoWaves] = {"TRI", "SINE", "SQUARE", "SAW UP", "SAW DN", "RANDOM"};
inline int lfoWave(float v) { const int b = (int)(v * kLfoWaves / 128.f); return b < 0 ? 0 : (b >= kLfoWaves ? kLfoWaves - 1 : b); }
static constexpr int kLfoSyncDivs = 14;
static const float kLfoSyncBeats[kLfoSyncDivs] = {32.f, 16.f, 8.f, 4.f, 2.f, 1.5f, 1.f, 2.f / 3.f, 0.75f, 0.5f, 1.f / 3.f, 0.25f, 1.f / 6.f, 0.125f};
static const char* const kLfoSyncNames[kLfoSyncDivs] = {"8 BARS", "4 BARS", "2 BARS", "1 BAR", "1/2", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
inline int lfoSyncDiv(float v) { const int b = (int)(v * kLfoSyncDivs / 128.f); return b < 0 ? 0 : (b >= kLfoSyncDivs ? kLfoSyncDivs - 1 : b); }
inline bool lfoSynced(float v) { return v >= 64.f; }
inline bool lfoRetrig(float v) { return v >= 64.f; }
inline float lfoHz(float v) { return 0.03f * std::pow(1000.f, clampf(v, 0.f, 127.f) / 127.f); }   // 0.03 .. 30 Hz
inline int lfoDest(float v) { const int d = (int)(v * (kLfoTargets + 1) / 128.f); return d < 0 ? 0 : (d > kLfoTargets ? kLfoTargets : d); }
inline float lfoDestKnob(int d) { return (d + 0.5f) * 128.f / (kLfoTargets + 1); }
inline int lfoTargetSlot(int p) { for (int i = 0; i < kLfoTargets; ++i) if (kLfoTarget[i] == p) return i; return -1; }

// ---- reverb / CHAOS: the KDSP-16 master effects (MasterFx.hpp, shared). Its own parameter block, the host
// parameters above are copied into it (the rest keeps the KDSP-16 defaults: no echo, no chorus, neutral master)
enum MasterParam {
    gECHO_TIME = 0, gECHO_FB, gECHO_LPF, gECHO_HPF, gECHO_WIDTH, gECHO_LEV,
    gREV_SIZE, gREV_DAMP, gREV_GATE, gREV_HPF, gREV_PRE, gREV_LEV,
    gEQ_LOW, gEQ_HIGH, gCMP_THR, gCMP_RAT, gCMP_ATK, gCMP_REL, gDRIVE, gVOLUME,
    gECHO_MODEL, gECHO_MOD, gREV_MODEL, gREV_MOD,
    gMF_HP, gMF_LP, gMF_RESO,
    gCHO_MODEL, gCHO_RATE, gCHO_DEPTH, gCHO_TIME, gCHO_FB, gCHO_WIDTH, gCHO_TONE, gCHO_LEV,
    gGL_CHANCE, gGL_SIZE, gGL_LEN, gGL_PITCH, gGL_FADE, gGL_REV, gGL_MIX,
    gMF_ON, gECHO_RTIME, gECHO_DRIVE, gECHO_DUCK, gECHO_FREEZE, kMasterParams
};
static const uint8_t kMasterDefaults[gGL_MIX + 1] = {
    48, 50, 100, 10, 64, 0,                        // echo LEVEL 0: KBAS-1 has its own echo
    60, 64, 0, 20, 10, 64,
    64, 64, 127, 40, 20, 50, 0, 116,               // VOLUME 116 = unity gain
    0, 30, 52, 30,
    0, 127, 0,
    0, 30, 50, 40, 0, 100, 100, 0,                 // chorus LEVEL 0
    0, 70, 50, 64, 30, 0, 127
};
static constexpr int kEchoDivs = 12;
static const float kEchoBeats[kEchoDivs] = {1.f / 16, 1.f / 8, 1.f / 6, 1.f / 4, 1.f / 3, 3.f / 8, 1.f / 2, 2.f / 3, 3.f / 4, 1.f, 1.5f, 2.f};
inline int echoDivIndex(int v) { return v * kEchoDivs / 128; }

// scales for the ACID GEN and the note snap
static constexpr int kScales = 8;
static const char* const kScaleNames[kScales] = {"CHROMATIC", "MINOR", "PHRYGIAN", "DORIAN", "MINOR PENTA", "BLUES", "MAJOR", "HARMONIC MIN"};
static const uint16_t kScaleMask[kScales] = {0xFFF, 0x5AD, 0x5AB, 0x6AD, 0x4A9, 0x4E9, 0xAB5, 0x9AD};   // bit n = semitone n from the root
inline bool inScale(int note, int root, int scale) { return (kScaleMask[scale % kScales] >> (((note - root) % 12 + 12) % 12)) & 1; }

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace kn

#include "MasterFx.hpp"     // reverb / CHAOS engines (it includes this file: the definitions above are what it uses)

namespace kn {

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
    case pVOWEL: if (v < 0.5f) return "OFF"; std::snprintf(b, sizeof(b), "%d%%", (int)std::lround(v / 127.f * 100.f)); return b;
    case pEVOLVE: if (v < 0.5f) return "OFF"; std::snprintf(b, sizeof(b), "%d%%", (int)std::lround(v / 127.f * 100.f)); return b;
    case pEVORESET: { const int n = evoResetLoops(v); if (!n) return "NEVER"; std::snprintf(b, sizeof(b), "%d LOOPS", n); return b; }
    case pPUMP: if (v < 0.5f) return "OFF"; std::snprintf(b, sizeof(b), "%d%%", (int)std::lround(v / 127.f * 100.f)); return b;
    case pPUMPRATE: return kPumpNames[pumpBin(v)];
    case pSUB: case pGROWL: if (v < 0.5f) return "OFF"; std::snprintf(b, sizeof(b), "%d%%", (int)std::lround(v / 127.f * 100.f)); return b;
    case pHARMONY: return kHarmNames[harmBin(v)];
    case pSAT: if (v < 0.5f) return "OFF"; std::snprintf(b, sizeof(b), "%d%%", (int)std::lround(v / 127.f * 100.f)); return b;
    case pSATTYPE: return kSatNames[satBin(v)];
    case pDLYTIMER: { const int r = delayRBin(v); return r < 0 ? "= LEFT" : kDelTimeNames[r]; }
    case pLOWCUT: { if (v < 0.5f) return "OFF"; const float hz = lowCutHz(v); if (hz >= 1000.f) std::snprintf(b, sizeof(b), "%.1fK", hz / 1000.f); else std::snprintf(b, sizeof(b), "%d HZ", (int)hz); return b; }
    case pLCRESO: case pREVERB: case pREVSIZE: case pREVDAMP: case pREVMOD: case pGLFADE: case pGLREV: case pGLMIX: case pGLCHANCE:
        if (v < 0.5f && (p == pREVERB || p == pGLCHANCE)) return "OFF";
        std::snprintf(b, sizeof(b), "%d%%", (int)std::lround(v / 127.f * 100.f)); return b;
    case pREVPRE: std::snprintf(b, sizeof(b), "%d MS", (int)std::lround(v / 127.f * 200.f)); return b;
    case pREVMODEL: return kReverbModelNames[reverbModelIndex((int)v)];
    case pGLSIZE: return kGlSizeNames[glSizeIndex((int)v)];
    case pGLLEN: return kGlLenNames[glLenIndex((int)v)];
    case pGLPITCH: { const int st = (int)glPitchSemis(v); std::snprintf(b, sizeof(b), st ? "%+d ST" : "0 ST", st); return b; }
    default: break;
    }
    if (p >= pL1WAVE && p < pL1WAVE + kLfos * kLfoParams) {
        switch ((p - pL1WAVE) % kLfoParams) {
        case 0: return kLfoWaveNames[lfoWave(v)];
        case 1: std::snprintf(b, sizeof(b), "%.2f HZ", lfoHz(v)); return b;     // the UI shows the synced division itself
        case 2: return lfoSynced(v) ? "SYNC" : "FREE";
        case 3: { const int d = (int)std::lround((v - 64.f) / 63.f * 100.f); if (!d) return "0"; std::snprintf(b, sizeof(b), "%+d%%", d); return b; }
        case 4: { const int d = lfoDest(v); return d == 0 ? "NONE" : kParamNames[kLfoTarget[d - 1]]; }
        default: return lfoRetrig(v) ? "RETRIG" : "FREE";
        }
    }
    {
        std::snprintf(b, sizeof(b), "%d", (int)std::lround(v)); return b;
    }
}


} // namespace kn
