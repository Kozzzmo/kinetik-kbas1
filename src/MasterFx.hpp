// Kinetik — master effects: echo (5 models), reverb (5 models), chorus (3 models), CHAOS (glitch / beat repeat), EQ,
// compressor, drive, safety limiter.
// Shared by KDSP-16 and KMSP-6: keep this file identical in both repositories.
// Uses the global parameter layout of Params.hpp (gECHO_TIME ... gREV_MOD), identical in both products.
//
// Each model has its own sound AND its own use of the MOD knob (label in kEchoModLabel / kReverbModLabel):
//   echo   DIGITAL   clean, exact repeats                         MOD = SPREAD (right repeats later: stereo)
//          TAPE      wow + flutter + drift, saturation, darker    MOD = WOW (pitch wobble, tape age)
//                    every pass, hiss
//          BBD       dark bucket-brigade, bandwidth falls with    MOD = CHORUS (stereo chorus in the repeats)
//                    the time, soft grit
//          PING-PONG left / right bounces                         MOD = OFFSET (right side later: swung bounce)
//          LO-FI     telephone band, low sample rate, few bits    MOD = CRUSH (sample rate and bits)
//          MULTI-TAP four taps before each repeat, scattered L/R  MOD = SPACING (how far apart the taps are)
//          REVERSE   every repeat plays backwards                 MOD = BLEND (forward repeats mixed in)
//          DIFFUSE   repeats smear into a wash (allpass loop)     MOD = DIFFUSION
//   + every model: R TIME (right side shorter / longer), DRIVE (saturation in the loop), DUCK (the echo backs off
//     while the sent sound plays), FREEZE (the loop holds what it has, no new input)
//   reverb ROOM      short, strong early reflections               MOD = EARLY (reflections vs tail)
//          PLATE     instant dense build-up, bright, no echoes     MOD = BRIGHT
//          HALL      long, wide, big early field, lush motion      MOD = MOTION (modulation depth)
//          SPRING    dispersive "boing" loop, band-limited         MOD = DRIP (chirp of the spring)
//          SHIMMER   long hall with octave-up feedback             MOD = SHIMMER (octave amount)
#pragma once
#include "Params.hpp"
#include "Dsp.hpp"
#include <vector>
#include <algorithm>
#include <cstring>
#include <atomic>

namespace kn {

inline int clampMI(int v, int n) { return v < 0 ? 0 : (v >= n ? n - 1 : v); }

enum EchoModel { emDigital, emTape, emBbd, emPingPong, emLofi, emMulti, emReverse, emDiffuse, kEchoModels };
static const char* const kEchoModelNames[kEchoModels] = {"DIGITAL", "TAPE", "BBD", "PING-PONG", "LO-FI", "MULTI-TAP", "REVERSE", "DIFFUSE"};
static const char* const kEchoModLabel[kEchoModels] = {"SPREAD", "WOW", "CHORUS", "OFFSET", "CRUSH", "SPACING", "BLEND", "DIFFUSION"};
// MODEL knob: 8 bins of 16 values; the values saved before 0.13 (5 models: 0, 26, 52, 77, 103) keep their model
static const int8_t kEchoModelOfBin[8] = {emDigital, emTape, emMulti, emBbd, emPingPong, emReverse, emLofi, emDiffuse};
static const int8_t kEchoModelOrder[kEchoModels] = {emDigital, emTape, emMulti, emBbd, emPingPong, emReverse, emLofi, emDiffuse};   // knob order
enum ReverbModel { rmRoom, rmPlate, rmHall, rmSpring, rmShimmer, kReverbModels };
static const char* const kReverbModelNames[kReverbModels] = {"ROOM", "PLATE", "HALL", "SPRING", "SHIMMER"};
static const char* const kReverbModLabel[kReverbModels] = {"EARLY", "BRIGHT", "MOTION", "DRIP", "SHIMMER"};
inline int echoModelIndex(int v) { return kEchoModelOfBin[clampMI(v / 16, 8)]; }
inline int reverbModelIndex(int v) { return clampMI(v * kReverbModels / 128, kReverbModels); }
inline int echoModelValue(int m) { for (int b = 0; b < 8; ++b) if (kEchoModelOfBin[b] == m) return b * 16; return 0; }
inline int reverbModelValue(int i) { return (i * 128 + kReverbModels - 1) / kReverbModels; }

// return level of the echo / reverb: 0 .. about +10 dB above the old maximum (square law, 64 = unity-ish)
inline float fxReturnGain(float v127) { const float x = clampf(v127 / 127.f, 0.f, 1.f); return 3.2f * x * x; }

// chorus (send effect, its own return like echo and reverb)
enum ChorusModel { cmChorus, cmEnsemble, cmFlanger, kChorusModels };
static const char* const kChorusModelNames[kChorusModels] = {"CHORUS", "ENSEMBLE", "FLANGER"};
static const char* const kChorusModelInfo[kChorusModels] = {
    "classic stereo chorus: one moving voice per side",
    "three voices per side with a fast shimmer: lush strings",
    "short sweeping delay with feedback: jet sweep",
};
inline int chorusModelIndex(int v) { return clampMI(v * kChorusModels / 128, kChorusModels); }
inline int chorusModelValue(int i) { return (i * 128 + kChorusModels - 1) / kChorusModels; }

// CHAOS (glitch / beat repeat): grabs a slice of the tracks sent to it (CHAOS send) and repeats it, tempo-synced
static constexpr int kGlSizes = 7, kGlLens = 5;
static const float kGlSizeBeats[kGlSizes] = {1.f, 0.5f, 1.f / 3.f, 0.25f, 1.f / 6.f, 0.125f, 0.0625f};
static const char* const kGlSizeNames[kGlSizes] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64"};
static const float kGlLenBeats[kGlLens] = {0.5f, 1.f, 2.f, 4.f, 8.f};
static const char* const kGlLenNames[kGlLens] = {"1/8 BAR", "1/4 BAR", "1/2 BAR", "1 BAR", "2 BARS"};
inline int glSizeIndex(int v) { return clampMI(v * kGlSizes / 128, kGlSizes); }
inline int glLenIndex(int v) { return clampMI(v * kGlLens / 128, kGlLens); }
inline float glPitchSemis(float v) { return std::round((v - 64.f) / 63.f * 12.f); }   // -12 .. +12 over one repeat event

// one-line description of each model (help texts, LCD)
static const char* const kEchoModelInfo[kEchoModels] = {
    "clean, exact repeats - MOD spreads them in stereo",
    "tape machine: wow, flutter, saturation, hiss - MOD = wow",
    "bucket-brigade: dark and gritty - MOD = chorus",
    "repeats bounce left / right - MOD delays the right side",
    "telephone band, low sample rate - MOD = crush",
    "four taps before each repeat, scattered left / right - MOD = spacing",
    "every repeat plays backwards - MOD blends forward repeats in",
    "repeats smear into a wash - MOD = diffusion",
};
static const char* const kReverbModelInfo[kReverbModels] = {
    "small room, close reflections - MOD = early reflections",
    "metal plate: instant, dense, bright - MOD = brightness",
    "large hall, long and wide - MOD = motion",
    "spring tank: boingy drips - MOD = drip",
    "long hall rising an octave - MOD = shimmer",
};

class MasterFx {
public:
    void init(float sampleRate)
    {
        sr = sampleRate;
        const size_t echoLen = (size_t)(sr * 6.6f) + 8;     // 2 bars at 40 BPM... plus OFFSET / SPREAD headroom
        echoL.assign(echoLen, 0.f); echoR.assign(echoLen, 0.f);
        for (int i = 0; i < 8; ++i) {
            const int maxLen = (int)(kRevMs[i] * 0.001f * sr * 1.6f) + 64;
            revLine[i].assign((size_t)maxLen, 0.f);
        }
        for (int i = 0; i < kDiff; ++i) diff[i].assign((size_t)(kDiffMs[i] * 0.001f * sr * 1.2f) + 4, 0.f);
        for (int i = 0; i < kSpring; ++i) spring[i].assign((size_t)(kSpringMs[i] * 0.001f * sr) + 4, 0.f);
        springLoop.assign((size_t)(sr * 0.12f) + 8, 0.f);
        shim.assign(4096, 0.f);
        preBuf.assign((size_t)(sr * 0.36f) + 4, 0.f);     // pre-delay (0.2 s) + early reflections (0.12 s)
        choL.assign((size_t)(sr * 0.07f) + 8, 0.f); choR.assign(choL.size(), 0.f);
        for (int k = 0; k < kEchoDiff; ++k) { edL[k].assign((size_t)(kEchoDiffMs[k] * 0.001f * sr) + 4, 0.f); edR[k].assign((size_t)(kEchoDiffMs[k] * 1.13f * 0.001f * sr) + 4, 0.f); }
        glL.assign((size_t)(sr * 4.f) + 8, 0.f); glR.assign(glL.size(), 0.f);   // one slice: up to a quarter note at 20 BPM
        revModel = -1;
        reset();
    }

    void reset()
    {
        std::fill(echoL.begin(), echoL.end(), 0.f); std::fill(echoR.begin(), echoR.end(), 0.f);
        clearReverb();
        std::fill(preBuf.begin(), preBuf.end(), 0.f);
        echoW = 0; preW = 0; cmpEnv = 0.f; gateEnv = 0.f; gateGain = 1.f; gateHold = 0;
        echoLpL = echoLpR = echoHpL = echoHpR = echoLp2L = echoLp2R = 0.f;
        holdL = holdR = 0.f; holdCnt = 0; wowPh = flutPh = modPh = chorPh = 0.f; drift = driftT = 0.f; driftCnt = 0;
        echoDelay = 1000.f;
        clearEchoExtras();
        std::fill(choL.begin(), choL.end(), 0.f); std::fill(choR.begin(), choR.end(), 0.f);
        choW = 0; choPh = choVib = choFbL = choFbR = choLpL = choLpR = choHpL = choHpR = 0.f; choQuiet = 0.0; choAwake = true;
        glW = glStart = glPos = 0; glActive = false; glPrevBeat = glFreeBeat = 0.0; glLpL = glLpR = 0.f;
        for (int k = 0; k < 8; ++k) revModCur[k] = revModInc[k] = 0.f;
        revModCnt = 0; echoQuiet = revQuiet = 0.0; echoAwake = revAwake = true;
    }

    // g: global parameters; main/send buffers are mixed into out (main bus + echo + reverb, then master chain)
    // beat0 = song position (beats) of the first frame, playing = transport running (the glitch follows the bars)
    void process(const float* g, float bpm, const float* mainL, const float* mainR,
                 const float* sendDL, const float* sendDR, const float* sendRL, const float* sendRR,
                 const float* sendCL, const float* sendCR, const float* sendGL, const float* sendGR, double beat0, bool playing,
                 float* outL, float* outR, uint32_t n)
    {
        // ================= echo: per-model settings
        const int em = echoModelIndex((int)g[gECHO_MODEL]);
        const int div = echoDivIndex((int)g[gECHO_TIME]);
        const float emod = g[gECHO_MOD] / 127.f;
        const uint32_t esz = (uint32_t)echoL.size();
        float targetDelay = (float)(kEchoBeats[div] * 60.0 / std::max(20.f, bpm) * sr);
        targetDelay = clampf(targetDelay, 8.f, (float)esz / 1.6f - 64.f);
        if (em == emReverse) targetDelay = std::min(targetDelay, (float)esz / 2.4f - 64.f);   // reverse reads two windows back
        const float fb = std::min(em == emTape ? 1.08f : 1.0f, g[gECHO_FB] / 127.f * 1.05f);
        float lpHz = expmap(g[gECHO_LPF] / 127.f, 500.f, 20000.f);
        float hpHz = expmap(g[gECHO_HPF] / 127.f, 20.f, 3000.f);
        float rRatio = 1.f;                 // right-side delay / left-side delay
        float sat = 1.f;                    // saturation in the loop (1 = none)
        bool twoPole = false;
        float wowDepth = 0.f, flutDepth = 0.f, driftDepth = 0.f, chorDepth = 0.f, hiss = 0.f;
        int holdN = 1; float bits = 0.f;
        switch (em) {
        case emDigital:
            rRatio = 1.f + 0.5f * emod;     // SPREAD: the right repeats come later
            break;
        case emTape:
            lpHz = std::min(lpHz, 7500.f - 3500.f * emod);   // old tape = darker
            hpHz = std::max(hpHz, 70.f);
            twoPole = true; sat = 2.6f;
            wowDepth = (8.f + 70.f * emod) * sr / 48000.f;    // ~0.05..0.5 % pitch wobble at 0.6 Hz
            flutDepth = (0.6f + 2.2f * emod) * sr / 48000.f;  // 7 Hz flutter
            driftDepth = 25.f * emod * sr / 48000.f;          // slow random drift
            hiss = 0.0004f + 0.0012f * emod;
            break;
        case emBbd: {
            // a 4096-stage delay: the longer the time, the lower the clock and the darker the sound
            const float bw = clampf(4096.f / (2.f * echoDelay / sr) / 2.2f, 900.f, 9000.f);
            lpHz = std::min(lpHz, bw);
            hpHz = std::max(hpHz, 90.f);
            twoPole = true; sat = 1.5f;
            chorDepth = (0.0004f + 0.0045f * emod) * sr;      // up to 4.5 ms of stereo chorus
            hiss = 0.0003f;
            break;
        }
        case emPingPong:
            rRatio = 1.f + 0.5f * emod;     // OFFSET: swung bounces
            break;
        case emLofi:
            lpHz = std::min(lpHz, 3800.f - 1600.f * emod);   // telephone band
            hpHz = std::max(hpHz, 320.f);
            sat = 1.6f;
            holdN = 2 + (int)(emod * 10.f);                   // 24 kHz .. 4 kHz
            bits = std::exp2(9.f - 4.f * emod);                // 512 .. 32 levels per unit (quiet sends stay audible)
            break;
        case emMulti: case emReverse: break;
        case emDiffuse:
            lpHz = std::min(lpHz, 12000.f);
            break;
        }
        // extras of every model
        const float rTime = 1.f + (g[gECHO_RTIME] - 64.f) / 128.f;          // right side x0.5 .. x1.5
        const float eDrive = g[gECHO_DRIVE] / 127.f;
        if (eDrive > 0.001f) sat = std::max(sat, 1.f + 5.f * eDrive);
        const bool freeze = g[gECHO_FREEZE] > 0.5f;
        const float duckAmt = g[gECHO_DUCK] / 127.f;
        const float duckAtk = std::exp(-1.f / (0.002f * sr)), duckRel = std::exp(-1.f / (0.25f * sr));
        const float tapSpace = 0.12f + 0.2f * emod;                           // MULTI-TAP
        const float diffG = 0.45f + 0.3f * emod;                              // DIFFUSE
        const float revBlend = em == emReverse ? 0.8f * emod : 0.f;           // REVERSE: forward repeats mixed in
        const float lpA = std::exp(-kTwoPi * lpHz / sr);
        const float hpA = std::exp(-kTwoPi * hpHz / sr);
        const float w = em == emPingPong ? 1.f : g[gECHO_WIDTH] / 127.f;
        const float eLev = fxReturnGain(g[gECHO_LEV]);
        const float wowInc = 0.6f / sr, flutInc = 7.1f / sr, chorInc = 0.83f / sr;

        // ================= reverb: per-model settings
        const int rm = reverbModelIndex((int)g[gREV_MODEL]);
        if (rm != revModel) setReverbModel(rm);
        const float rmod = g[gREV_MOD] / 127.f;
        const float sizeX = g[gREV_SIZE] / 127.f;
        const float rt = expmap(sizeX, kRevRtMin[rm], kRevRtMax[rm]);
        float damp = 0.05f + g[gREV_DAMP] / 127.f * 0.85f;
        if (rm == rmPlate) damp *= 0.6f * (1.f - 0.85f * rmod);          // BRIGHT
        if (rm == rmHall) damp *= 0.8f;
        if (rm == rmRoom) damp = 0.2f + damp * 0.75f;
        const float rHpA = std::exp(-kTwoPi * expmap(g[gREV_HPF] / 127.f, 20.f, 2000.f) / sr);
        const uint32_t psz = (uint32_t)preBuf.size();
        const uint32_t erMax = (uint32_t)(0.12f * sr);
        const uint32_t pre = std::min<uint32_t>(psz - erMax - 2, (uint32_t)(g[gREV_PRE] / 127.f * 0.2f * sr));
        const float rLev = fxReturnGain(g[gREV_LEV]) * kRevOut[rm];
        const bool gate = g[gREV_GATE] > 0.5f;
        const uint32_t gateHoldS = (uint32_t)(expmap(g[gREV_GATE] / 127.f, 0.03f, 1.0f) * sr);
        float revG[8];
        for (int i = 0; i < 8; ++i) revG[i] = std::pow(10.f, -3.f * (float)revLen[i] / (rt * sr));
        const float lineMod = (rm == rmHall ? 4.f + 40.f * rmod : rm == rmShimmer ? 12.f : rm == rmPlate ? 3.f : 1.5f) * sr / 48000.f;
        const float shimMix = rm == rmShimmer ? 0.15f + 0.55f * rmod : 0.f;
        // early reflections (ROOM: strong and close, HALL: wide and late)
        float erGain = 0.f; float erScale = 1.f;
        if (rm == rmRoom) { erGain = 0.35f + 0.9f * rmod; erScale = 0.5f + 0.9f * sizeX; }
        if (rm == rmHall) { erGain = 0.25f; erScale = 1.f + 0.6f * sizeX; }
        const float tailGain = rm == rmRoom ? 1.f - 0.6f * rmod : 1.f;
        uint32_t erTap[kEr]; for (int k = 0; k < kEr; ++k) erTap[k] = std::min<uint32_t>(erMax, (uint32_t)(kErMs[k] * erScale * 0.001f * sr));
        // spring tank loop
        const uint32_t ssz = (uint32_t)springLoop.size();
        const uint32_t sLen = std::min<uint32_t>(ssz - 4, (uint32_t)((0.032f + 0.05f * sizeX) * sr));
        const float sFb = std::pow(10.f, -3.f * (float)sLen / (rt * sr));
        const float sAp = 0.45f + 0.4f * rmod;                                 // DRIP: more dispersion = more chirp
        const uint32_t springTapR = std::min<uint32_t>(sLen - 1, (uint32_t)(0.0073f * sr));
        const float springLp = std::exp(-kTwoPi * 4500.f / sr), springHp = std::exp(-kTwoPi * 160.f / sr);

        // ================= master
        eqLowL.setShelf(false, 120.f, (g[gEQ_LOW] - 64.f) / 64.f * 15.f, sr); copyCoef(eqLowR, eqLowL);
        eqHighL.setShelf(true, 7000.f, (g[gEQ_HIGH] - 64.f) / 64.f * 15.f, sr); copyCoef(eqHighR, eqHighL);
        const float thrDb = -40.f + g[gCMP_THR] / 127.f * 40.f;
        const float ratio = 1.f + g[gCMP_RAT] / 127.f * 19.f;
        const float atk = std::exp(-1.f / (expmap(g[gCMP_ATK] / 127.f, 0.0001f, 0.08f) * sr));
        const float rel = std::exp(-1.f / (expmap(g[gCMP_REL] / 127.f, 0.02f, 1.0f) * sr));
        const float makeup = std::pow(10.f, (-thrDb * (1.f - 1.f / ratio) * 0.4f) / 20.f);
        const float drv = g[gDRIVE] / 127.f;
        const float mv = g[gVOLUME] / 127.f; const float vol = mv * mv * 1.2f;

        // ================= chorus: per-model settings
        const int cm = chorusModelIndex((int)g[gCHO_MODEL]);
        const float cRate = expmap(g[gCHO_RATE] / 127.f, 0.05f, 8.f) * (cm == cmEnsemble ? 0.6f : 1.f);
        const float cBaseMs = cm == cmFlanger ? expmap(g[gCHO_TIME] / 127.f, 0.3f, 6.f) : expmap(g[gCHO_TIME] / 127.f, 4.f, 30.f);
        const float cDepthMs = g[gCHO_DEPTH] / 127.f * (cm == cmFlanger ? cBaseMs * 0.9f : std::min(cBaseMs * 0.8f, 9.f));
        const float cBase = cBaseMs * 0.001f * sr, cDepth = cDepthMs * 0.001f * sr;
        const float cFb = g[gCHO_FB] / 127.f * (cm == cmFlanger ? 0.92f : 0.6f);
        const float cWidth = g[gCHO_WIDTH] / 127.f * 0.5f;          // phase offset of the right side: 0 .. half a cycle
        const float cToneA = g[gCHO_TONE] >= 126.5f ? 0.f : std::exp(-kTwoPi * expmap(g[gCHO_TONE] / 127.f, 1200.f, 20000.f) / sr);
        const float cLev = fxReturnGain(g[gCHO_LEV]);
        const float cInc = cRate / sr, cVibInc = 6.3f / sr, cVib = cm == cmEnsemble ? 0.00012f * sr : 0.f;
        const int cTaps = cm == cmEnsemble ? 3 : 1;
        const float cHpA = std::exp(-kTwoPi * 150.f / sr);   // no lows into the chorus: a delayed bass cancels the dry one
        const uint32_t csz = (uint32_t)choL.size();

        // ================= glitch / beat repeat
        const float glChance = g[gGL_CHANCE] / 127.f;
        const bool hold = glHold.load(std::memory_order_relaxed) != 0;
        const bool glOn = glChance > 0.f || hold || glActive;
        const double bpf = (double)bpm / (60.0 * sr);
        const double sliceBeats = kGlSizeBeats[glSizeIndex((int)g[gGL_SIZE])];
        const double lenBeats = kGlLenBeats[glLenIndex((int)g[gGL_LEN])];
        const float glSemis = glPitchSemis(g[gGL_PITCH]), glFade = g[gGL_FADE] / 127.f;
        const float glRev = g[gGL_REV] / 127.f, glMix = g[gGL_MIX] / 127.f;
        const double pBeat = glChance >= 1.f ? 1.0 : 1.0 - std::pow(1.0 - (double)glChance, 0.25);   // CHANCE is per bar
        const uint32_t gsz = (uint32_t)glL.size();

        // ================= idle detection: echo and reverb sleep once their sends are silent and the tail has died
        // away below -120 dB (estimated from feedback / decay time). Their state is cleared when they fall asleep,
        // so a time or size change made while asleep never replays old audio.
        float pkD = 0.f, pkR = 0.f;
        for (uint32_t i = 0; i < n; ++i) {
            pkD = std::max(pkD, std::max(std::fabs(sendDL[i]), std::fabs(sendDR[i])));
            pkR = std::max(pkR, std::max(std::fabs(sendRL[i]), std::fabs(sendRR[i])));
        }
        constexpr float kQuiet = 1e-6f;
        echoQuiet = pkD > kQuiet ? 0.0 : echoQuiet + n;
        revQuiet = pkR > kQuiet ? 0.0 : revQuiet + n;
        double echoTail = 1e300;
        if (fb < 0.97f) {
            const double dMax = std::max(echoDelay, targetDelay) * std::max(1.f, rRatio) + wowDepth + flutDepth + 2.f * driftDepth + chorDepth + 64.0;
            const double repeats = fb < 1e-3f ? 1.0 : std::ceil(std::log(1e-6) / std::log((double)fb));
            echoTail = dMax * (repeats + 2.0);
        }
        const double revTail = (double)psz + (double)ssz + rt * sr * 2.0 * (rm == rmShimmer ? 3.0 : 1.0) + 8192.0;
        const bool echoOn = freeze || echoQuiet <= echoTail, revOn = revQuiet <= revTail;
        float pkC = 0.f;
        for (uint32_t i = 0; i < n; ++i) pkC = std::max(pkC, std::max(std::fabs(sendCL[i]), std::fabs(sendCR[i])));
        choQuiet = pkC > kQuiet ? 0.0 : choQuiet + n;
        const double choRepeats = cFb < 1e-3f ? 1.0 : std::ceil(std::log(1e-6) / std::log((double)cFb));
        const bool choOn = choQuiet <= (double)(cBase + cDepth + 64.f + cVib) * (choRepeats + 2.0);
        if (!choOn && choAwake) {
            std::fill(choL.begin(), choL.end(), 0.f); std::fill(choR.begin(), choR.end(), 0.f);
            choFbL = choFbR = choLpL = choLpR = choHpL = choHpR = 0.f;
        }
        choAwake = choOn;
        if (!echoOn && echoAwake) {
            std::fill(echoL.begin(), echoL.end(), 0.f); std::fill(echoR.begin(), echoR.end(), 0.f);
            echoLpL = echoLpR = echoHpL = echoHpR = echoLp2L = echoLp2R = 0.f; holdL = holdR = 0.f;
            clearEchoExtras();
        }
        if (!revOn && revAwake) {
            clearReverb(); std::fill(preBuf.begin(), preBuf.end(), 0.f);
            revHpZ = 0.f; gateEnv = 0.f; gateHold = 0;
        }
        echoAwake = echoOn; revAwake = revOn;
        if (!echoOn) echoDelay = targetDelay;

        for (uint32_t i = 0; i < n; ++i) {
            float dl = 0.f, dr = 0.f;
            if (echoOn) {
            // ---------------- echo
            echoDelay += (targetDelay - echoDelay) * 0.0005f;
            float d = echoDelay, dR;
            if (em == emTape) {
                wowPh += wowInc; if (wowPh >= 1.f) wowPh -= 1.f;
                flutPh += flutInc; if (flutPh >= 1.f) flutPh -= 1.f;
                if (--driftCnt <= 0) { driftCnt = (int)(sr * 0.25f); driftT = rng.next(); }
                drift += (driftT - drift) * 0.00004f;
                d += wowDepth * (0.5f + 0.5f * std::sin(kTwoPi * wowPh)) + flutDepth * std::sin(kTwoPi * flutPh) + driftDepth * (drift + 1.f);
                dR = d;
            } else if (em == emBbd) {
                chorPh += chorInc; if (chorPh >= 1.f) chorPh -= 1.f;
                const float tri = 2.f * std::fabs(2.f * chorPh - 1.f) - 1.f;   // -1..1
                d += chorDepth * (0.5f + 0.5f * tri);
                dR = echoDelay + chorDepth * (0.5f - 0.5f * tri);              // opposite phase on the right
            } else {
                dR = echoDelay * rRatio;
            }
            dR *= rTime;
            dl = readFrac(echoL, esz, (float)echoW - d);
            dr = readFrac(echoR, esz, (float)echoW - dR);
            float outEL = dl, outER = dr;                      // what is heard (the loop is fed from dl / dr)
            if (em == emMulti) {
                // taps before the main repeat, alternately left and right, quieter and quieter
                const float t1L = readFrac(echoL, esz, (float)echoW - d * (1.f - tapSpace));
                const float t2R = readFrac(echoR, esz, (float)echoW - dR * (1.f - 2.f * tapSpace));
                const float t3L = readFrac(echoL, esz, (float)echoW - d * (1.f - 3.f * tapSpace));
                const float t1R = readFrac(echoR, esz, (float)echoW - dR * (1.f - tapSpace));
                outEL = 0.7f * dl + 0.45f * t1L + 0.3f * t3L + 0.2f * t2R;
                outER = 0.7f * dr + 0.55f * t1R + 0.35f * t2R + 0.15f * t1L;
            } else if (em == emReverse) {
                // two grains, half a window apart, each reading the previous window backwards (triangle windows sum to 1)
                const uint32_t D = (uint32_t)std::max(64.f, d), DR = (uint32_t)std::max(64.f, dR);
                if (++revCnt >= D) revCnt = 0;
                if (++revCntR >= DR) revCntR = 0;
                auto grainRead = [&](const std::vector<float>& b, uint32_t c, uint32_t W) {
                    const uint32_t c2 = (c + W / 2) % W;
                    const float w1 = 1.f - std::fabs(2.f * c / W - 1.f), w2 = 1.f - std::fabs(2.f * c2 / W - 1.f);
                    return b[(echoW + 2 * esz - 1 - 2 * c) % esz] * w1 + b[(echoW + 2 * esz - 1 - 2 * c2) % esz] * w2;
                };
                const float rl = grainRead(echoL, revCnt, D), rr = grainRead(echoR, revCntR, DR);
                outEL = rl * (1.f - revBlend) + dl * revBlend; outER = rr * (1.f - revBlend) + dr * revBlend;
                dl = outEL; dr = outER;                          // the reversed repeats feed the loop
            }
            if (freeze) {   // the loop keeps what it holds: no input, no filtering, full feedback
                // whole-sample reads: an interpolated read would dull the loop a little on every pass
                const uint32_t fL = (uint32_t)std::lround(echoDelay), fR = (uint32_t)std::lround(echoDelay * (em == emBbd || em == emTape ? 1.f : rRatio) * rTime);
                echoL[echoW] = echoL[(echoW + esz - fL) % esz];
                echoR[echoW] = echoR[(echoW + esz - fR) % esz];
                echoW = (echoW + 1) % esz;
                dl = outEL; dr = outER;
            } else {
            float inL, inR;
            if (em == emPingPong) {
                inL = 0.5f * (sendDL[i] + sendDR[i]) + fb * dr;   // the input enters on the left only
                inR = fb * dl;
            } else {
                const float sm = 0.5f * (sendDL[i] + sendDR[i]);
                inL = sendDL[i] * (1.f - w) + sm * w + fb * (w * dr + (1.f - w) * dl);
                inR = sendDR[i] * (1.f - w) + fb * (w * dl + (1.f - w) * dr);
            }
            if (hiss > 0.f) { inL += hiss * rng.next(); inR += hiss * rng.next(); }
            echoLpL = inL + lpA * (echoLpL - inL); echoLpR = inR + lpA * (echoLpR - inR);
            float yl = echoLpL, yr = echoLpR;
            if (twoPole) {   // second pole: steeper roll-off
                echoLp2L = yl + lpA * (echoLp2L - yl); echoLp2R = yr + lpA * (echoLp2R - yr);
                yl = echoLp2L; yr = echoLp2R;
            }
            echoHpL = yl + hpA * (echoHpL - yl); echoHpR = yr + hpA * (echoHpR - yr);
            yl -= echoHpL; yr -= echoHpR;
            if (holdN > 1) {
                if (--holdCnt <= 0) { holdCnt = holdN; holdL = std::round(yl * bits) / bits; holdR = std::round(yr * bits) / bits; }
                yl = holdL; yr = holdR;
            }
            if (em == emDiffuse)   // each pass through the loop gets smeared a little more
                for (int k = 0; k < kEchoDiff; ++k) { yl = allpass(edL[k], edPosL[k], (uint32_t)edL[k].size(), yl, diffG); yr = allpass(edR[k], edPosR[k], (uint32_t)edR[k].size(), yr, diffG); }
            if (sat > 1.f) {   // tape: a little asymmetry (even harmonics)
                const float as = em == emTape ? 0.12f : 0.f;
                yl = std::tanh((yl + as * yl * yl) * sat) / sat; yr = std::tanh((yr + as * yr * yr) * sat) / sat;
            }
            echoL[echoW] = yl; echoR[echoW] = yr;
            echoW = (echoW + 1) % esz;
            dl = outEL; dr = outER;
            }
            if (duckAmt > 0.f) {   // DUCK: the echo backs off while the sent sound plays
                const float a = 0.5f * (std::fabs(sendDL[i]) + std::fabs(sendDR[i]));
                duckEnv = a > duckEnv ? a + duckAtk * (duckEnv - a) : a + duckRel * (duckEnv - a);
                const float gd = 1.f / (1.f + duckAmt * 14.f * duckEnv);
                dl *= gd; dr *= gd;
            }
            }

            float outRL = 0.f, outRR = 0.f;
            if (revOn) {
            // ---------------- reverb
            float rin = 0.5f * (sendRL[i] + sendRR[i]);
            revHpZ = rin + rHpA * (revHpZ - rin); rin = rin - revHpZ;
            preBuf[preW] = rin;
            float x = preBuf[(preW + psz - pre) % psz];
            float erL = 0.f, erR = 0.f;
            if (erGain > 0.f)
                for (int k = 0; k < kEr; ++k) {
                    const float v = preBuf[(preW + psz - pre - erTap[k]) % psz] * kErGain[k];
                    if (k & 1) erR += v; else erL += v;
                }
            preW = (preW + 1) % psz;
            if (rm == rmShimmer) x += shimOut * shimMix;
            float springOut = 0.f;
            if (rm == rmSpring) {
                // spring tank: a loop of dispersive allpasses -> each pass returns as a falling "drip"
                float s = x + springLoop[(springW + ssz - sLen) % ssz] * sFb;
                for (int k = 0; k < kSpring; ++k) s = allpass(spring[k], springPos[k], springLen[k], s, sAp);
                springLpZ = s + springLp * (springLpZ - s); s = springLpZ;
                springHpZ = s + springHp * (springHpZ - s); s -= springHpZ;
                s = std::tanh(s * 1.5f) / 1.5f;
                springLoop[springW] = s; springW = (springW + 1) % ssz;
                springOut = s;
                x = s * 0.35f;                   // a little of it into the tank FDN for density
            }
            // input diffusion (plate: 4 strong allpasses, hall / shimmer 2, room 1, spring none)
            for (int k = 0; k < diffCount; ++k) x = allpass(diff[k], diffPos[k], diffLen[k], x, diffG);
            // line modulation: the sines are evaluated every kModStep samples and interpolated in between
            if (--revModCnt <= 0) {
                revModCnt = kModStep;
                const float ph = modPh + (float)kModStep * 0.37f / sr;
                for (int k = 0; k < 8; ++k)
                    revModInc[k] = (lineMod * std::sin(kTwoPi * (ph + k * 0.125f)) - revModCur[k]) / (float)kModStep;
            }
            modPh += 0.37f / sr; if (modPh >= 1.f) modPh -= 1.f;
            float o[8], sum = 0.f;
            for (int k = 0; k < 8; ++k) {
                revModCur[k] += revModInc[k];
                o[k] = readFrac(revLine[k], (uint32_t)revLine[k].size(), (float)revW[k] - (float)revLen[k] - revModCur[k]);
                sum += o[k];
            }
            const float hh = sum * 0.25f;
            for (int k = 0; k < 8; ++k) {
                float v = (o[k] - hh) * revG[k];
                revLp[k] = v + damp * (revLp[k] - v);
                v = revLp[k] + x * ((k & 1) ? 0.5f : -0.5f);
                revLine[k][revW[k]] = v;
                if (++revW[k] >= (uint32_t)revLine[k].size()) revW[k] = 0;
                if (k < 4) outRL += o[k] * ((k & 1) ? -1.f : 1.f); else outRR += o[k] * ((k & 1) ? 1.f : -1.f);
            }
            if (rm == rmShimmer) shimOut = pitchUp(0.5f * (outRL + outRR));
            outRL *= tailGain; outRR *= tailGain;
            if (rm == rmSpring) {   // the right side hears the tank a few ms later (decorrelated, mono-safe)
                const float sR = springLoop[(springW + ssz - 1 - springTapR) % ssz];
                outRL = outRL * 0.5f + springOut * 2.4f; outRR = outRR * 0.5f + sR * 2.4f;
            }
            outRL += erL * erGain * 2.f; outRR += erR * erGain * 2.f;
            if (gate) {
                const float a = std::fabs(rin);
                gateEnv = std::max(a, gateEnv * 0.9995f);
                if (gateEnv > 0.003f) gateHold = gateHoldS; else if (gateHold > 0) --gateHold;
                const float tgt = gateHold > 0 ? 1.f : 0.f;
                gateGain += (tgt - gateGain) * (tgt > gateGain ? 0.02f : 0.004f);
            } else gateGain = 1.f;
            outRL *= gateGain; outRR *= gateGain;
            }

            // ---------------- chorus
            float cl = 0.f, cr = 0.f;
            if (choOn) {
                choPh += cInc; if (choPh >= 1.f) choPh -= 1.f;
                choVib += cVibInc; if (choVib >= 1.f) choVib -= 1.f;
                choHpL = sendCL[i] + cHpA * (choHpL - sendCL[i]); choHpR = sendCR[i] + cHpA * (choHpR - sendCR[i]);
                choL[choW] = (sendCL[i] - choHpL) + cFb * choFbL; choR[choW] = (sendCR[i] - choHpR) + cFb * choFbR;
                const float vib = cVib > 0.f ? cVib * std::sin(kTwoPi * choVib) : 0.f;
                float yl = 0.f, yr = 0.f;
                for (int k = 0; k < cTaps; ++k) {
                    float pl = choPh + (float)k / (float)cTaps; if (pl >= 1.f) pl -= 1.f;
                    float pr = pl + cWidth; if (pr >= 1.f) pr -= 1.f;
                    const float ml = cm == cmFlanger ? 2.f * std::fabs(2.f * pl - 1.f) - 1.f : std::sin(kTwoPi * pl);
                    const float mr = cm == cmFlanger ? 2.f * std::fabs(2.f * pr - 1.f) - 1.f : std::sin(kTwoPi * pr);
                    yl += readFrac(choL, csz, (float)choW - (cBase + cDepth * (0.5f + 0.5f * ml)) - vib);
                    yr += readFrac(choR, csz, (float)choW - (cBase + cDepth * (0.5f + 0.5f * mr)) + vib);
                }
                yl /= (float)cTaps; yr /= (float)cTaps;
                choFbL = std::tanh(yl); choFbR = std::tanh(yr);           // bounded feedback
                if (cTaps > 1) { yl *= 1.8f; yr *= 1.8f; }
                if (cToneA > 0.f) { choLpL = yl + cToneA * (choLpL - yl); choLpR = yr + cToneA * (choLpR - yr); yl = choLpL; yr = choLpR; }
                if (++choW >= csz) choW = 0;
                cl = yl * cLev; cr = yr * cLev;
            }

            float l = mainL[i] + dl * eLev + outRL * 0.35f * rLev + cl;
            float r = mainR[i] + dr * eLev + outRR * 0.35f * rLev + cr;

            // ---------------- CHAOS: repeats a slice of the sent tracks (CHANCE per bar on the beats, or HOLD); the sent
            // tracks' dry sound is replaced by the repeats, the other tracks and the effect returns keep playing
            if (glOn) {
                double beat;
                if (playing) beat = beat0 + (double)i * bpf; else { glFreeBeat += bpf; beat = glFreeBeat; }
                if (!glActive) {
                    bool start = false;
                    if (hold) start = std::floor(beat / sliceBeats) != std::floor(glPrevBeat / sliceBeats);
                    else if (playing && glChance > 0.f && beat >= glPrevBeat && std::floor(beat) != std::floor(glPrevBeat))
                        start = rng.uni() < pBeat;
                    if (start) {
                        glActive = true; glHoldEvt = hold; glStart = glW; glPos = 0; glRepeat = -1;
                        glSlice = (uint32_t)std::max(64.0, std::min((double)gsz - 16.0, sliceBeats / bpf));
                        glLen = hold ? 0xFFFFFFFFu : (uint32_t)std::max<double>(glSlice, lenBeats / bpf);
                    }
                }
                if (!glActive || glPos < glSlice) { glL[glW] = sendGL[i]; glR[glW] = sendGR[i]; if (++glW >= gsz) glW = 0; }
                if (glActive) {
                    const uint32_t W = std::min<uint32_t>(96, glSlice / 4);
                    if (glHoldEvt && !hold && glLen == 0xFFFFFFFFu) glLen = (glPos / glSlice + 1) * glSlice;   // released: end of this slice
                    if (!glHoldEvt && !playing && glLen > glPos + W) glLen = glPos + W;                       // transport stopped
                    const uint32_t nrep = glPos / glSlice, v = glPos - nrep * glSlice;
                    if ((int)nrep != glRepeat) {
                        glRepeat = (int)nrep;
                        glRevNow = nrep > 0 && rng.uni() < glRev;
                        const double nref = glHoldEvt ? 8.0 : std::max(1.0, (double)glLen / glSlice - 1.0);
                        glRate = nrep == 0 ? 1.f : std::exp2(clampf((float)(glSemis * nrep / nref), -24.f, 24.f) / 12.f);
                        glGain = std::pow(1.f - 0.6f * glFade, (float)nrep);
                        const float fc = std::max(250.f, 18000.f * std::pow(1.f - 0.5f * glFade, (float)nrep));
                        glLpA = nrep == 0 ? 0.f : std::exp(-kTwoPi * fc / sr);
                    }
                    float off = std::fmod((float)v * glRate, (float)glSlice);
                    if (glRevNow) off = (float)glSlice - 1.f - off;
                    float wl = readFrac(glL, gsz, (float)glStart + off), wr = readFrac(glR, gsz, (float)glStart + off);
                    if (glLpA > 0.f) { glLpL = wl + glLpA * (glLpL - wl); glLpR = wr + glLpA * (glLpR - wr); wl = glLpL; wr = glLpR; }
                    else { glLpL = wl; glLpR = wr; }
                    float wnd = std::min(1.f, (float)(glSlice - v) / (float)W);
                    if (nrep > 0) wnd = std::min(wnd, (float)v / (float)W);
                    const float act = glLen == 0xFFFFFFFFu ? 1.f : std::min(1.f, (float)(glLen - glPos) / (float)W);
                    const float m = glMix * act, wg = glGain * wnd;
                    l += (wl * wg - sendGL[i]) * m; r += (wr * wg - sendGR[i]) * m;
                    if (++glPos >= glLen) glActive = false;
                }
                glPrevBeat = beat;
            } else glPrevBeat = playing ? beat0 + (double)i * bpf : glPrevBeat;
            l = eqHighL.tick(eqLowL.tick(l)); r = eqHighR.tick(eqLowR.tick(r));
            // compressor (linked peak)
            const float pk = std::max(std::fabs(l), std::fabs(r));
            cmpEnv = pk > cmpEnv ? pk + atk * (cmpEnv - pk) : pk + rel * (cmpEnv - pk);
            if (ratio > 1.01f && thrDb < -0.1f) {
                const float lv = 20.f * std::log10(cmpEnv + 1e-9f);
                const float over = lv - thrDb;
                float gain = makeup;
                if (over > 0.f) gain *= std::pow(10.f, -over * (1.f - 1.f / ratio) / 20.f);
                l *= gain; r *= gain;
            }
            l = drvL.process(l, drv) * vol; r = drvR.process(r, drv) * vol;
            outL[i] = safety(l); outR[i] = safety(r);
        }
    }

    // transparent below 0.9, smooth ceiling at 1.0
    static inline float safety(float x)
    {
        const float a = std::fabs(x);
        if (a <= 0.9f) return x;
        const float y = 0.9f + 0.1f * std::tanh((a - 0.9f) * 10.f);
        return x < 0.f ? -y : y;
    }

private:
    static constexpr int kDiff = 4, kSpring = 8, kEr = 8;
    static constexpr float kRevMs[8] = {29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f};
    static constexpr float kDiffMs[kDiff] = {4.77f, 3.59f, 12.73f, 9.31f};
    static constexpr float kSpringMs[kSpring] = {0.37f, 0.53f, 0.71f, 0.89f, 1.13f, 1.37f, 1.61f, 1.97f};
    static constexpr float kErMs[kEr] = {4.3f, 7.9f, 11.7f, 16.1f, 21.9f, 29.3f, 37.9f, 47.3f};
    static constexpr float kErGain[kEr] = {0.9f, 0.85f, 0.72f, 0.66f, 0.55f, 0.47f, 0.38f, 0.3f};
    // per model: FDN line length scale, decay range (seconds), output gain
    static constexpr float kRevScale[kReverbModels] = {0.42f, 0.75f, 1.35f, 0.5f, 1.4f};
    static constexpr float kRevRtMin[kReverbModels] = {0.12f, 0.4f, 1.0f, 0.6f, 2.0f};
    static constexpr float kRevRtMax[kReverbModels] = {1.6f, 6.f, 12.f, 5.f, 16.f};
    static constexpr float kRevOut[kReverbModels] = {1.2f, 1.f, 1.f, 1.0f, 0.9f};

    static inline float readFrac(const std::vector<float>& b, uint32_t size, float pos)
    {
        while (pos < 0.f) pos += (float)size;
        const uint32_t i0 = (uint32_t)pos % size, i1 = (i0 + 1) % size;
        const float fr = pos - std::floor(pos);
        return b[i0] + (b[i1] - b[i0]) * fr;
    }
    static inline float allpass(std::vector<float>& b, uint32_t& pos, uint32_t len, float x, float gcoef)
    {
        const float d = b[pos];
        const float v = x + gcoef * d;
        b[pos] = v;
        if (++pos >= len) pos = 0;
        return d - gcoef * v;
    }
    // octave up: two crossfaded taps sweeping a 2048-sample window
    float pitchUp(float x)
    {
        const uint32_t sz = (uint32_t)shim.size();
        shim[shimW] = x;
        shimW = (shimW + 1) % sz;
        constexpr float D = 2048.f;
        shimD -= 1.f; if (shimD < 0.f) shimD += D;           // ratio 2: the delay shrinks one sample per sample
        float d2 = shimD + D * 0.5f; if (d2 >= D) d2 -= D;
        const float a = readFrac(shim, sz, (float)shimW - 1.f - shimD), b = readFrac(shim, sz, (float)shimW - 1.f - d2);
        const float wa = 1.f - std::fabs(shimD / D * 2.f - 1.f), wb = 1.f - std::fabs(d2 / D * 2.f - 1.f);
        return a * wa + b * wb;
    }
    void setReverbModel(int rm)
    {
        revModel = rm;
        for (int i = 0; i < 8; ++i)
            revLen[i] = std::min<uint32_t>((uint32_t)revLine[i].size() - 64, (uint32_t)(kRevMs[i] * 0.001f * sr * kRevScale[rm]));
        diffCount = rm == rmPlate ? 4 : (rm == rmRoom ? 1 : (rm == rmSpring ? 0 : 2));
        diffG = rm == rmPlate ? 0.75f : 0.6f;
        const float ds = rm == rmRoom ? 0.6f : (rm == rmPlate ? 0.8f : 1.f);
        for (int i = 0; i < kDiff; ++i) diffLen[i] = std::max<uint32_t>(2, (uint32_t)(kDiffMs[i] * 0.001f * sr * ds));
        for (int i = 0; i < kSpring; ++i) springLen[i] = std::max<uint32_t>(2, (uint32_t)(kSpringMs[i] * 0.001f * sr));
        clearReverb();
    }
    void clearReverb()
    {
        for (int i = 0; i < 8; ++i) { std::fill(revLine[i].begin(), revLine[i].end(), 0.f); revW[i] = 0; revLp[i] = 0.f; }
        for (int i = 0; i < kDiff; ++i) { std::fill(diff[i].begin(), diff[i].end(), 0.f); diffPos[i] = 0; }
        for (int i = 0; i < kSpring; ++i) { std::fill(spring[i].begin(), spring[i].end(), 0.f); springPos[i] = 0; }
        std::fill(springLoop.begin(), springLoop.end(), 0.f); springW = 0;
        std::fill(shim.begin(), shim.end(), 0.f); shimW = 0; shimD = 0.f; shimOut = 0.f;
        revHpZ = springLpZ = springHpZ = 0.f;
    }
    static void copyCoef(Biquad& dst, const Biquad& src)
    {
        dst.b0 = src.b0; dst.b1 = src.b1; dst.b2 = src.b2; dst.a1 = src.a1; dst.a2 = src.a2;
    }

    float sr = 48000.f;
    Rng rng;
    std::vector<float> echoL, echoR; uint32_t echoW = 0; float echoDelay = 1000.f;
    float echoLpL = 0, echoLpR = 0, echoHpL = 0, echoHpR = 0, echoLp2L = 0, echoLp2R = 0;
    float holdL = 0, holdR = 0; int holdCnt = 0; float wowPh = 0, flutPh = 0, modPh = 0, chorPh = 0;
    float drift = 0, driftT = 0; int driftCnt = 0;
    std::vector<float> revLine[8]; uint32_t revLen[8] = {}; uint32_t revW[8] = {}; float revLp[8] = {};
    std::vector<float> diff[kDiff]; uint32_t diffLen[kDiff] = {}, diffPos[kDiff] = {}; int diffCount = 2; float diffG = 0.6f;
    std::vector<float> spring[kSpring]; uint32_t springLen[kSpring] = {}, springPos[kSpring] = {};
    std::vector<float> springLoop; uint32_t springW = 0;
    float springLpZ = 0, springHpZ = 0;
    std::vector<float> shim; uint32_t shimW = 0; float shimD = 0, shimOut = 0;
    std::vector<float> preBuf; uint32_t preW = 0; float revHpZ = 0, gateEnv = 0, gateGain = 1; uint32_t gateHold = 0;
    int revModel = -1;
    Biquad eqLowL, eqLowR, eqHighL, eqHighR;
    float cmpEnv = 0;
    static constexpr int kModStep = 32;
    float revModCur[8] = {}, revModInc[8] = {}; int revModCnt = 0;
    double echoQuiet = 0.0, revQuiet = 0.0; bool echoAwake = true, revAwake = true;
    DriveAA drvL, drvR;   // anti-aliased master DRIVE
    static constexpr int kEchoDiff = 3;
    static constexpr float kEchoDiffMs[kEchoDiff] = {7.3f, 11.9f, 17.1f};
    std::vector<float> edL[kEchoDiff], edR[kEchoDiff]; uint32_t edPosL[kEchoDiff] = {}, edPosR[kEchoDiff] = {};
    uint32_t revCnt = 0, revCntR = 0; float duckEnv = 0.f;
    void clearEchoExtras()
    {
        for (int k = 0; k < kEchoDiff; ++k) { std::fill(edL[k].begin(), edL[k].end(), 0.f); std::fill(edR[k].begin(), edR[k].end(), 0.f); edPosL[k] = edPosR[k] = 0; }
        revCnt = revCntR = 0; duckEnv = 0.f;
    }
    std::vector<float> choL, choR; uint32_t choW = 0;
    float choPh = 0, choVib = 0, choFbL = 0, choFbR = 0, choLpL = 0, choLpR = 0, choHpL = 0, choHpR = 0; double choQuiet = 0.0; bool choAwake = true;
    std::vector<float> glL, glR; uint32_t glW = 0, glStart = 0, glPos = 0, glSlice = 64, glLen = 0; int glRepeat = -1;
    bool glActive = false, glHoldEvt = false, glRevNow = false; float glRate = 1.f, glGain = 1.f, glLpA = 0.f, glLpL = 0.f, glLpR = 0.f;
    double glPrevBeat = 0.0, glFreeBeat = 0.0;
public:
    std::atomic<int> glHold{0};   // HOLD button (set by the plug-in from the UI command "gh 0/1")
    bool glitching() const { return glActive; }
};

} // namespace kn
