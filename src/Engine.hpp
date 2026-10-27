// Kinetik KBAS-1 — monophonic acid bass: oscillator, 4-pole resonant filter, accent / slide, sequencer, drive, delay
#pragma once
#include "Bank.hpp"
#include "Saturator.hpp"
#include "Click.hpp"
#include <atomic>
#include <vector>
#include <algorithm>
#include <climits>
#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace kn {

inline void knDenormalsOff()
{
#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
    _mm_setcsr(_mm_getcsr() | 0x8040);
#elif defined(__aarch64__)
    uint64_t fpcr; __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr)); __asm__ __volatile__("msr fpcr, %0" :: "r"(fpcr | (1ull << 24)));
#endif
}

struct Transport { bool hostValid = false, hostPlaying = false; double beat = 0, bpm = 120; };
struct NoteIn { uint32_t frame; int note; int vel; bool on; };
// live recording (REC + playing): what the UI writes into the pattern
//   kind 0 = note on step a (note, accent on accent index b), kind 1 = slide on index a, kind 2 = TIE on step a
struct RecEvent { uint8_t kind; int8_t pattern; int16_t a, b; int16_t note; uint8_t accent; };

// zero-delay-feedback 4-pole low pass (cascade of TPT one-poles) with a saturated feedback path
struct Ladder {
    float s[4] = {0, 0, 0, 0};
    float g = 0.1f, G = 0.09f, k = 0.f;
    void set(float fc, float sr, float reso01)
    {
        fc = clampf(fc, 20.f, sr * 0.42f);
        g = std::tan(kPi * fc / sr); G = g / (1.f + g);
        k = reso01 * 4.1f;
    }
    inline float tick(float x)
    {
        const float S = G * G * G * s[0] + G * G * s[1] + G * s[2] + s[3];
        const float G4 = G * G * G * G;
        float u = (x * (1.f + k * 0.35f) - k * S / (1.f + g)) / (1.f + k * G4);   // bass kept at high resonance
        u = std::tanh(u * 0.9f) / 0.9f;
        float y = u;
        for (int i = 0; i < 4; ++i) {
            const float v = (y - s[i]) * G;
            const float o = v + s[i];
            s[i] = o + v;
            y = o;
        }
        return y;
    }
    void reset() { s[0] = s[1] = s[2] = s[3] = 0.f; }
};

class Engine {
public:
    float param[kTotalParams];
    Bank bank;
    // outputs for the UI
    int outStep = -1;               // 1/24 of a step since the song start (-1 = stopped)
    int outGate = 0, outPattern = 0, outNote = 0;
    static constexpr int kFine = 24;
    int midiTranspose = 0;          // semitones (transpose mode)
    uint32_t trigCount = 0;         // tests

    Engine()
    {
        for (int i = 0; i < kTotalParams; ++i) param[i] = kParamDefaults[i];
        bank.initDefault();
    }
    bool recording = false;           // REC armed in the UI: live notes are written into the playing pattern
    Click click;                      // metronome while recording
    static constexpr int kRecQ = 256;
    bool popRec(RecEvent& e)
    {
        const int r = recR.load(std::memory_order_relaxed);
        if (r == recW.load(std::memory_order_acquire)) return false;
        e = recQ[r]; recR.store((r + 1) % kRecQ, std::memory_order_release);
        return true;
    }

    void init(double s)
    {
        sr = (float)s;
        sat.init(sr); click.init(sr);
        dlyL.assign((size_t)(sr * 4.5f), 0.f); dlyR.assign(dlyL.size(), 0.f);
        reset();
    }
    void reset()
    {
        lad.reset(); gate = false; amp = 0.f; fenv = 0.f; accCap = 0.f; phase = 0.0;
        std::fill(dlyL.begin(), dlyL.end(), 0.f); std::fill(dlyR.begin(), dlyR.end(), 0.f); dlyQuiet = 1 << 30;
        wasPlaying = false; usingInternal = false; prevPlay = false; internalBeat = 0; held = 0;
        lastK = INT64_MIN; gateOffBeat = -1.0; lockStep = -1; chainPl.reset(); ratN = 1; phase2 = subPh = 0.0; for (auto& f : form) f.reset();
        sat.reset(); click.reset(); recSkipK = INT64_MIN; recPrevK = INT64_MIN; recNote = -1;
    }

    void process(float* outL, float* outR, uint32_t frames, const Transport& tp, const NoteIn* notes, uint32_t nNotes)
    {
        knDenormalsOff();
        if (frames == 0) return;
        const bool playParam = param[pPLAY] > 0.5f;
        double bpm = tp.hostValid && tp.bpm > 1.0 ? tp.bpm : (double)param[pTEMPO];
        bpm = std::max(20.0, std::min(400.0, bpm));
        curBpm = bpm;
        bool playing = false, restarted = false; double b0 = 0;
        if (tp.hostValid && tp.hostPlaying) {
            playing = true; b0 = tp.beat;
            if (!wasPlaying || usingInternal || std::fabs(b0 - prevB1) > 0.01) restarted = true;
            usingInternal = false;
        } else if (playParam) {
            if (!prevPlay || !wasPlaying || !usingInternal) { internalBeat = 0; restarted = true; }
            playing = true; b0 = internalBeat; usingInternal = true;
        }
        prevPlay = playParam;
        if (!playing && wasPlaying && gate && heldCount() == 0) gate = false;   // stop: release the sequenced note
        const int want = clampi((int)param[pPATTERN], 0, kPatterns - 1);
        if (!playing || restarted) {
            const int c = playing ? chainPl.start(bank.chain) : -1;
            if (!playing) { chainPl.reset(); lockStep = -1; ratN = 1; }
            curPattern = c >= 0 ? c : want;
        }
        if (restarted) { lastK = INT64_MIN; gateOffBeat = -1.0; ratN = 1; }
        if (edits != seenEdits || evoFor != curPattern) resetEvolve();
        const double bpf = bpm / (60.0 * sr);
        if (!playing) b0 = freeBeat;
        updateControls();
        const float pumpDepth = param[pPUMP] / 127.f; const double pumpLen = kPumpBeats[pumpBin(param[pPUMPRATE])];
        uint32_t ni = 0;
        if (!playing) { recPrevK = INT64_MIN; recNote = -1; recSkipK = INT64_MIN; }
        for (uint32_t i = 0; i < frames; ++i) {
            const double beat = b0 + i * bpf;
            while (ni < nNotes && notes[ni].frame <= i) { liveNote(notes[ni], playing, beat); ++ni; }
            if (playing) {
                sequence(beat);
                if (ratN > 1 && ratI < ratN && beat >= ratStart + ratDur * ratI / ratN) {      // ratchet repeats
                    noteOn(ratNote, ratAcc, false);
                    gateOffBeat = beat + ratDur / ratN * 0.55;
                    ++ratI;
                }
                if (gateOffBeat >= 0.0 && beat >= gateOffBeat) { gateOffBeat = -1.0; if (heldCount() == 0) gate = false; }
            }
            if (pumpDepth > 0.001f) {          // PUMP: a ducking curve on every beat division (like a kick sidechain)
                double ph = beat / pumpLen; ph -= std::floor(ph);
                const float u = (float)std::min(1.0, ph / 0.55);
                pumpG = 1.f - pumpDepth * (1.f - u * u * (3.f - 2.f * u));
            } else pumpG = 1.f;
            if ((i & 15) == 0) updateControls();
            float l, r; renderSample(l, r);
            outL[i] = l; outR[i] = r;
        }
        while (ni < nNotes) liveNote(notes[ni++], playing, b0 + bpf * frames);
        click.process(outL, outR, frames, b0, bpf, recording && playing);
        const double b1 = b0 + bpf * frames;
        outStep = playing ? (int)((int64_t)std::floor(b1 / 0.25 * kFine) % (32768LL * kFine)) : -1;
        outGate = gate ? 1 : 0; outPattern = curPattern; outNote = curNote;
        wasPlaying = playing; prevB1 = b1;
        if (playing && usingInternal) internalBeat = b1;
        freeBeat = b1;
    }
    // the pattern that plays (EVOLVE works on a copy)
    const Pattern& playingPattern() const { return param[pEVOLVE] > 0.5f ? evoPat : bank.patterns[curPattern]; }
    uint32_t edits = 0;               // the plugin bumps it when a pattern changed (EVOLVE starts again from the written line)

private:
    float sr = 48000.f; double curBpm = 120.0;
    // voice
    double phase = 0.0, phase2 = 0.0, subPh = 0.0; float logNote = 36.f, targetNote = 36.f; int curNote = 36, harmOff = 0;
    bool gate = false, accentNow = false, sliding = false;
    float amp = 0.f, ampEnv = 0.f, fenv = 0.f, accCap = 0.f, accTarget = 0.f;
    Ladder lad; DriveAA drv; OnePole tone, dcL, subLp;
    Svf form[3]; float vowMix = 0.f;
    // cached controls
    float slideCoef = 0.99f, fenvCoef = 0.999f, accEnvCoef = 0.999f, ampDecCoef = 1.f, atkStep = 0.01f, relCoef = 0.99f;
    float baseCut = 500.f, envOct = 3.f, accOct = 1.f, accGain = 1.f, wave = 0.f, tune = 0.f, drive = 0.f, vol = 1.f, reso = 0.f, curCut = 500.f;
    float sub = 0.f, growl = 0.f, pumpG = 1.f; int harmDeg = 0;
    double dTime = 1000, dTimeR = 1000; float dFb = 0.5f, dMix = 0.f; OnePole dlpL, dlpR; std::vector<float> dlyL, dlyR; int dw = 0, dlyQuiet = 1 << 30;
    // sequencer
    int curPattern = 0; int64_t lastK = INT64_MIN; double gateOffBeat = -1.0, freeBeat = 0.0;
    int lockStep = -1;                    // the playing step whose locks apply (-1 = the knobs)
    ChainPlayer chainPl;
    int ratN = 1, ratI = 1, ratNote = 36; bool ratAcc = false; double ratStart = 0, ratDur = 0;
    Pattern evoPat; int evoFor = -1, loops = 0; uint32_t seenEdits = 0, rng = 0x2545F491u;
    float urand() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (rng & 0xFFFFFF) / 16777216.f; }
    float ep(int i) const { if (lockStep >= 0) { const uint8_t v = playingPattern().locks[lockStep][i]; if (v != kNoLock) return (float)v; } return param[i]; }
    bool wasPlaying = false, usingInternal = false, prevPlay = false; double internalBeat = 0, prevB1 = 0;
    Saturator sat;
    // live recording
    RecEvent recQ[kRecQ]; std::atomic<int> recW{0}, recR{0};
    int64_t recSkipK = INT64_MIN, recPrevK = INT64_MIN; int recNote = -1;
    void pushRec(const RecEvent& e)
    {
        const int w = recW.load(std::memory_order_relaxed), nw = (w + 1) % kRecQ;
        if (nw == recR.load(std::memory_order_acquire)) return;   // full: dropped
        recQ[w] = e; recW.store(nw, std::memory_order_release);
    }
    // the step nearest to a beat (swing included)
    int64_t nearestStep(double beat) const
    {
        const Pattern& P = bank.patterns[curPattern];
        const double sb = 0.25, off = (P.swing / 100.0 * 2.0 - 1.0) * sb;
        const int64_t k0 = (int64_t)std::floor(beat / sb);
        int64_t best = k0; double bd = 1e9;
        for (int64_t k = k0 - 1; k <= k0 + 1; ++k) {
            const double st = k * sb + ((k & 1) ? off : 0.0), d = std::fabs(st - beat);
            if (d < bd) { bd = d; best = k; }
        }
        return best;
    }
    // REC: a note played while the sequencer runs goes to the nearest step of the written pattern; a note played
    // while another is held slides into it (the steps between are tied), a note held for more than a step is tied
    void recordOn(int note, int vel, double beat, bool legato)
    {
        const Pattern& P = bank.patterns[curPattern];
        const int L = std::max<int>(1, P.length), AL = std::max(1, P.accRow()), SL = std::max(1, P.slideRow());
        const int64_t k = std::max<int64_t>(0, nearestStep(beat));
        if (k > lastK) recSkipK = k;            // the step has not started yet: the live note stands in for it
        const int s = (int)(k % L);
        pushRec({0, (int8_t)curPattern, (int16_t)s, (int16_t)(k % AL), (int16_t)clampi(note, 0, 127), (uint8_t)(vel >= 100 ? 1 : 0)});
        if (legato && recPrevK != INT64_MIN && k > recPrevK && k - recPrevK <= L) {
            for (int64_t j = recPrevK + 1; j < k; ++j) pushRec({2, (int8_t)curPattern, (int16_t)(j % L), 0, 0, 0});
            pushRec({1, (int8_t)curPattern, (int16_t)((k - 1) % SL), 0, 0, 0});
        }
        recPrevK = k; recNote = note;
    }
    void recordOff(int note, double beat)
    {
        if (note != recNote || recPrevK == INT64_MIN) return;
        const Pattern& P = bank.patterns[curPattern];
        const int L = std::max<int>(1, P.length);
        const int64_t e = nearestStep(beat);
        for (int64_t j = recPrevK + 1; j < e && j - recPrevK < L; ++j) pushRec({2, (int8_t)curPattern, (int16_t)(j % L), 0, 0, 0});
        recNote = -1;
    }
    // live notes (last-note priority)
    int heldNotes[16] = {}; int held = 0;
    int heldCount() const { return held; }

    void resetEvolve() { evoPat = bank.patterns[curPattern]; evoFor = curPattern; loops = 0; seenEdits = edits; }
    // EVOLVE: small changes each loop, always in the scale
    void mutate(Pattern& P, float a)
    {
        const int root = bank.glob.root, scale = bank.glob.scale, lo = 36 + root - 5, hi = 36 + root + 19;
        auto snapIn = [&](int n, int dir) { for (int k = 0; k < 12 && !inScale(n, root, scale); ++k) n += dir; return n; };
        for (int s = 1; s < P.length; ++s) {
            if (P.on[s] && urand() < a * 0.3f) {
                const int dir = urand() < 0.5f ? -1 : 1;
                int n = P.note[s] + dir * (1 + (int)(urand() * 3.f));
                if (urand() < a * 0.2f) n = P.note[s] + (n > 48 ? -12 : 12);
                n = snapIn(n, dir);
                while (n > hi + 12) n -= 12;
                while (n < lo) n += 12;
                P.note[s] = (uint8_t)clampi(n, 12, 108);
            }
            if (P.on[s] && urand() < a * 0.15f) P.accent[s] ^= 1;
            if (P.on[s] && urand() < a * 0.1f) P.slide[s] ^= 1;
            if (urand() < a * 0.06f) P.on[s] ^= 1;
        }
    }
    int harmonyNote(int note) const
    {
        if (harmDeg == 0) return note;
        if (harmDeg == 7 || harmDeg == -7) return note + (harmDeg > 0 ? 12 : -12);
        int n = note, left = harmDeg;
        while (left > 0) { ++n; if (inScale(n, bank.glob.root, bank.glob.scale)) --left; }
        return n;
    }

    void updateControls()
    {
        slideCoef = std::exp(-1.f / (slideSec(ep(pSLIDE)) * 0.35f * sr));
        const float dec = accentNow ? std::min(decaySec(ep(pDECAY)), 0.2f) : decaySec(ep(pDECAY));   // accents: short and snappy
        fenvCoef = std::exp(-1.f / (dec * 0.3f * sr));
        accEnvCoef = std::exp(-1.f / (0.09f * sr));
        ampDecCoef = std::exp(-1.f / (ampDecaySec(ep(pAMPDEC)) * 0.3f * sr));
        atkStep = 1.f / (0.0015f * sr);
        relCoef = std::exp(-1.f / (0.006f * sr));
        baseCut = cutoffHz(ep(pCUTOFF));
        envOct = ep(pENVMOD) / 127.f * 5.5f;
        accOct = ep(pACCENT) / 127.f * 2.5f;
        accGain = 1.f + ep(pACCENT) / 127.f * 0.9f;
        wave = ep(pWAVE) / 127.f;
        tune = (ep(pTUNE) - 64.f) / 64.f * 12.f;
        drive = ep(pDRIVE) / 127.f;
        tone.setLP(900.f * std::pow(20.f, ep(pTONE) / 127.f), sr);
        dcL.setLP(18.f, sr);
        vol = volumeGain(ep(pVOLUME));
        const double spb = 60.0 * sr / std::max(20.0, curBpm);
        dTime = std::min((double)dlyL.size() - 4.0, std::max(16.0, kDelTime[bin8(ep(pDLYTIME))] * spb));
        const int rb = delayRBin(param[pDLYTIMER]);
        dTimeR = rb < 0 ? dTime : std::min((double)dlyL.size() - 4.0, std::max(16.0, kDelTime[rb] * spb));
        dFb = ep(pDLYFB) / 127.f * 0.92f; dMix = ep(pDLYMIX) / 127.f;
        dlpL.setLP(3500.f, sr); dlpR.setLP(3500.f, sr);
        const float oct = envOct * fenv + accOct * accCap;
        reso = ep(pRESO) / 127.f;
        curCut = baseCut * std::pow(2.f, oct);
        lad.set(curCut, sr, reso);
        sat.amount = param[pSAT] / 127.f; { const int t = satBin(param[pSATTYPE]); if (t != sat.type) { sat.type = t; } } sat.setTapeTone();
        sub = param[pSUB] / 127.f; growl = param[pGROWL] / 127.f; harmDeg = kHarmDegrees[harmBin(param[pHARMONY])];
        subLp.setLP(180.f, sr);
        // VOWEL: three formants whose vowel follows the filter (closed = "u", open = "i")
        vowMix = param[pVOWEL] / 127.f;
        if (vowMix > 0.001f) {
            static const float F[5][3] = {{300, 870, 2240}, {570, 840, 2410}, {730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}};   // u o a e i
            const float pos = clampf(std::log2(curCut / 120.f) / 7.f, 0.f, 0.9999f) * 4.f;
            const int a = (int)pos; const float f = pos - a;
            const float q = 6.f + reso * 14.f;
            for (int k = 0; k < 3; ++k) form[k].setup(F[a][k] + (F[a + 1][k] - F[a][k]) * f, q, sr);
        }
    }

    void noteOn(int note, bool accent, bool glide)
    {
        targetNote = (float)note; curNote = note;
        harmOff = harmonyNote(note) - note;
        accentNow = accent;
        if (glide && gate) { sliding = true; }
        else {
            sliding = false; logNote = targetNote;
            fenv = 1.f;
            if (accent) accTarget = 1.f;
            ampEnv = 1.f;
        }
        if (accent && glide && gate) accTarget = 1.f;
        gate = true;
        ++trigCount;
    }

    void liveNote(const NoteIn& n, bool playing, double beat)
    {
        if (playing && recording) {             // REC: play it and write it into the pattern
            if (n.on) recordOn(n.note, n.vel, beat, held > 0);
            else recordOff(n.note, beat);
        } else if (playing && bank.glob.transpose) {   // transpose mode: the keys move the pattern, they do not play
            if (n.on) midiTranspose = n.note - 60;
            return;
        }
        if (n.on) {
            const bool legato = held > 0;
            if (held < 16) heldNotes[held++] = n.note;
            noteOn(n.note + 0, n.vel >= 100, legato);
        } else {
            int j = 0;
            for (int i = 0; i < held; ++i) if (heldNotes[i] != n.note) heldNotes[j++] = heldNotes[i];
            held = j;
            if (held > 0) noteOn(heldNotes[held - 1], false, true);
            else if (gateOffBeat < 0.0) gate = false;
        }
    }

    // step boundaries (16ths, swing on the odd ones)
    void sequence(double beat)
    {
        const Pattern& P0 = playingPattern();
        const double sb = 0.25, off = (P0.swing / 100.0 * 2.0 - 1.0) * sb;
        int64_t k = (int64_t)std::floor(beat / sb + 1e-9);
        if ((k & 1) && beat < k * sb + off) --k;
        if (k == lastK || k < 0) return;
        const bool first = lastK == INT64_MIN;
        lastK = k;
        // end of the playing pattern: the chain (or the chosen pattern), and EVOLVE's next generation
        const int want = clampi((int)param[pPATTERN], 0, kPatterns - 1);
        if (!first && k % std::max<int>(1, P0.length) == 0) {
            const int c = chainPl.next(bank.chain);
            const int np = c >= 0 ? c : want;
            if (np != curPattern) { curPattern = np; resetEvolve(); }
            else {
                ++loops;
                const int n = evoResetLoops(param[pEVORESET]);
                if (n && loops % n == 0) evoPat = bank.patterns[curPattern];
                else if (param[pEVOLVE] > 0.5f) mutate(evoPat, param[pEVOLVE] / 127.f);
            }
        }
        if (edits != seenEdits) resetEvolve();
        const Pattern& P = playingPattern();
        const int L = std::max<int>(1, P.length), AL = std::max(1, P.accRow()), SL = std::max(1, P.slideRow());
        const int s = (int)(k % L), nxt = (int)((k + 1) % L);
        const double start = k * sb + ((k & 1) ? off : 0.0), dur = sb + ((k & 1) ? -off : off);
        const bool slideHere = P.slide[(int)(k % SL)] != 0, slidePrev = P.slide[(int)(((k - 1) % SL + SL) % SL)] != 0;
        const bool accHere = P.accent[(int)(k % AL)] != 0;
        ratN = 1;
        // TIE: the previous note goes on through this step
        if (P.tie[s]) {
            if (gate) gateOffBeat = (P.tie[nxt] || (slideHere && P.on[nxt])) ? -1.0 : start + dur * 0.55;
            return;
        }
        if (k == recSkipK) {                    // just recorded live: the held key plays it
            recSkipK = INT64_MIN; lockStep = -1; gateOffBeat = held > 0 ? -1.0 : start + dur * 0.55; return;
        }
        lockStep = P.on[s] ? s : -1;
        updateControls();
        const bool plays = P.on[s] && (P.prob[s] >= 100 || urand() * 100.f < P.prob[s]);
        if (!plays) { if (gate && held == 0) gate = false; gateOffBeat = -1.0; lockStep = -1; return; }
        const int prev = (int)((k - 1 + L) % L);
        const bool glide = (P.on[prev] || P.tie[prev]) && slidePrev && gate && k > 0;
        noteOn(P.note[s] + midiTranspose, accHere, glide);
        const int r = clampi(P.ratchet[s], 1, 4);
        if (r > 1) { ratN = r; ratI = 1; ratStart = start; ratDur = dur; ratNote = P.note[s] + midiTranspose; ratAcc = accHere; gateOffBeat = start + dur / r * 0.55; return; }
        gateOffBeat = ((slideHere && P.on[nxt]) || P.tie[nxt]) ? -1.0 : start + dur * 0.55;   // sliding / tied steps hold the gate
    }

    inline float osc(double& ph, float f)
    {
        const double dt = std::min(0.45, (double)f / sr);
        ph += dt; if (ph >= 1.0) ph -= 1.0;
        const float p = (float)ph, d = (float)dt;
        const float saw = 2.f * p - 1.f - polyBlep(p, d);
        const float sq = blepSquare(p, d);
        return saw * (1.f - wave) + sq * wave * 0.8f;
    }
    inline void renderSample(float& outL, float& outR)
    {
        // pitch (glide while sliding)
        if (sliding) { logNote = targetNote + (logNote - targetNote) * slideCoef; if (std::fabs(logNote - targetNote) < 0.001f) { logNote = targetNote; sliding = false; } }
        else logNote = targetNote;
        const float f = 440.f * std::pow(2.f, (logNote + tune - 69.f) / 12.f);
        float x = osc(phase, f);
        if (harmDeg != 0) x = (x + 0.7f * osc(phase2, f * std::pow(2.f, harmOff / 12.f))) * 0.75f;   // HARMONY: second voice in the scale
        // envelopes
        fenv *= fenvCoef;
        accCap += (accTarget - accCap) * (1.f - accEnvCoef); accTarget *= accEnvCoef;   // accent "charge": repeated accents rise
        if (gate) { ampEnv *= ampDecCoef; amp = std::min(1.f, amp + atkStep); }
        else amp *= relCoef;
        const float a = amp * (0.25f + 0.75f * ampEnv) * (accentNow ? accGain : 1.f);
        if (a < 1e-6f && !gate) { lad.tick(0.f); x = 0.f; }
        // GROWL: the oscillator moves the cutoff at audio rate
        if (growl > 0.001f) lad.set(curCut * std::pow(2.f, growl * 1.6f * x), sr, reso);
        float y = lad.tick(x * 0.7f);
        if (vowMix > 0.001f) {              // VOWEL: formant filter mixed with the ladder
            float v = 0.f; static const float fg[3] = {1.f, 0.7f, 0.35f};
            for (int k = 0; k < 3; ++k) { form[k].tick(x * 0.7f); v += form[k].bp * fg[k]; }
            y += (v * 1.6f - y) * vowMix;
        }
        y *= a;
        // SUB: a clean square one octave down, after the filter
        if (sub > 0.001f) { subPh += f * 0.5 / sr; if (subPh >= 1.0) subPh -= 1.0; y += subLp.lp(subPh < 0.5 ? 1.f : -1.f) * sub * a * 0.6f; }
        // SATURATE (2x oversampled models)
        y = sat.process(y);
        // drive + tone
        if (drive > 0.001f) y = drv.process(y * (1.f + drive * 2.f), drive) * 0.8f;
        y = tone.lp(y);
        y -= dcL.lp(y);   // DC blocker
        y *= vol;
        // ping-pong delay (left and right times can differ)
        float l = y, r = y;
        if (dMix > 0.001f || dlyQuiet < (int)dlyL.size()) {          // keeps running until the echo has died away
            auto rd = [&](const std::vector<float>& b, double d) {
                double pos = (double)dw - d; if (pos < 0) pos += b.size();
                const size_t i0 = (size_t)pos % b.size(), i1 = (i0 + 1) % b.size(); const float fr = (float)(pos - std::floor(pos));
                return b[i0] + (b[i1] - b[i0]) * fr;
            };
            const float el = rd(dlyL, dTime), er = rd(dlyR, dTimeR);
            dlyL[(size_t)dw] = y * dMix + dlpR.lp(er) * dFb;     // cross-feedback = ping-pong
            dlyR[(size_t)dw] = dlpL.lp(el) * dFb;
            l += el; r += er;
            dlyQuiet = (std::fabs(el) + std::fabs(er) > 1e-6f || dMix > 0.001f) ? 0 : dlyQuiet + 1;
        }
        if (++dw >= (int)dlyL.size()) dw = 0;
        l *= pumpG; r *= pumpG;
        // safety
        outL = std::tanh(l * 0.9f) / 0.9f; outR = std::tanh(r * 0.9f) / 0.9f;
    }
};

} // namespace kn
