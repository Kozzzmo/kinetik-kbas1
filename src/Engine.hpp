// Kinetik KBAS-1 — monophonic acid bass: oscillator, 4-pole resonant filter, accent / slide, sequencer, drive, delay
#pragma once
#include "Bank.hpp"
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
    void init(double s)
    {
        sr = (float)s;
        dlyL.assign((size_t)(sr * 4.5f), 0.f); dlyR.assign(dlyL.size(), 0.f);
        reset();
    }
    void reset()
    {
        lad.reset(); gate = false; amp = 0.f; fenv = 0.f; accCap = 0.f; phase = 0.0;
        std::fill(dlyL.begin(), dlyL.end(), 0.f); std::fill(dlyR.begin(), dlyR.end(), 0.f);
        wasPlaying = false; usingInternal = false; prevPlay = false; internalBeat = 0; held = 0;
        lastK = INT64_MIN; gateOffBeat = -1.0;
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
        if (!playing || restarted) curPattern = want;
        if (restarted) { lastK = INT64_MIN; gateOffBeat = -1.0; }
        const double bpf = bpm / (60.0 * sr);
        updateControls();
        uint32_t ni = 0;
        for (uint32_t i = 0; i < frames; ++i) {
            while (ni < nNotes && notes[ni].frame <= i) { liveNote(notes[ni], playing); ++ni; }
            if (playing) {
                const double beat = b0 + i * bpf;
                sequence(beat);
                if (gateOffBeat >= 0.0 && beat >= gateOffBeat) { gateOffBeat = -1.0; if (heldCount() == 0) gate = false; }
            }
            if ((i & 15) == 0) updateControls();
            float l, r; renderSample(l, r);
            outL[i] = l; outR[i] = r;
        }
        while (ni < nNotes) liveNote(notes[ni++], playing);
        const double b1 = b0 + bpf * frames;
        outStep = playing ? (int)((int64_t)std::floor(b1 / 0.25 * kFine) % (32768LL * kFine)) : -1;
        outGate = gate ? 1 : 0; outPattern = curPattern; outNote = curNote;
        wasPlaying = playing; prevB1 = b1;
        if (playing && usingInternal) internalBeat = b1;
    }

private:
    float sr = 48000.f; double curBpm = 120.0;
    // voice
    double phase = 0.0; float logNote = 36.f, targetNote = 36.f; int curNote = 36;
    bool gate = false, accentNow = false, sliding = false;
    float amp = 0.f, ampEnv = 0.f, fenv = 0.f, accCap = 0.f, accTarget = 0.f;
    Ladder lad; DriveAA drv; OnePole tone, dcL;
    // cached controls
    float slideCoef = 0.99f, fenvCoef = 0.999f, accEnvCoef = 0.999f, ampDecCoef = 1.f, atkStep = 0.01f, relCoef = 0.99f;
    float baseCut = 500.f, envOct = 3.f, accOct = 1.f, accGain = 1.f, wave = 0.f, tune = 0.f, drive = 0.f, vol = 1.f;
    double dTime = 1000; float dFb = 0.5f, dMix = 0.f; OnePole dlpL, dlpR; std::vector<float> dlyL, dlyR; int dw = 0;
    // sequencer
    int curPattern = 0; int64_t lastK = INT64_MIN; double gateOffBeat = -1.0;
    bool wasPlaying = false, usingInternal = false, prevPlay = false; double internalBeat = 0, prevB1 = 0;
    // live notes (last-note priority)
    int heldNotes[16] = {}; int held = 0;
    int heldCount() const { return held; }

    void updateControls()
    {
        slideCoef = std::exp(-1.f / (slideSec(param[pSLIDE]) * 0.35f * sr));
        const float dec = accentNow ? std::min(decaySec(param[pDECAY]), 0.2f) : decaySec(param[pDECAY]);   // accents: short and snappy
        fenvCoef = std::exp(-1.f / (dec * 0.3f * sr));
        accEnvCoef = std::exp(-1.f / (0.09f * sr));
        ampDecCoef = std::exp(-1.f / (ampDecaySec(param[pAMPDEC]) * 0.3f * sr));
        atkStep = 1.f / (0.0015f * sr);
        relCoef = std::exp(-1.f / (0.006f * sr));
        baseCut = cutoffHz(param[pCUTOFF]);
        envOct = param[pENVMOD] / 127.f * 5.5f;
        accOct = param[pACCENT] / 127.f * 2.5f;
        accGain = 1.f + param[pACCENT] / 127.f * 0.9f;
        wave = param[pWAVE] / 127.f;
        tune = (param[pTUNE] - 64.f) / 64.f * 12.f;
        drive = param[pDRIVE] / 127.f;
        tone.setLP(900.f * std::pow(20.f, param[pTONE] / 127.f), sr);
        dcL.setLP(18.f, sr);
        vol = volumeGain(param[pVOLUME]);
        const double spb = 60.0 * sr / std::max(20.0, curBpm);
        dTime = std::min((double)dlyL.size() - 4.0, std::max(16.0, kDelTime[bin8(param[pDLYTIME])] * spb));
        dFb = param[pDLYFB] / 127.f * 0.92f; dMix = param[pDLYMIX] / 127.f;
        dlpL.setLP(3500.f, sr); dlpR.setLP(3500.f, sr);
        const float oct = envOct * fenv + accOct * accCap;
        lad.set(baseCut * std::pow(2.f, oct), sr, param[pRESO] / 127.f);
    }

    void noteOn(int note, bool accent, bool glide)
    {
        targetNote = (float)note; curNote = note;
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

    void liveNote(const NoteIn& n, bool playing)
    {
        if (playing && bank.glob.transpose) {   // transpose mode: the keys move the pattern, they do not play
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
        const Pattern& P0 = bank.patterns[curPattern];
        const double sb = 0.25, off = (P0.swing / 100.0 * 2.0 - 1.0) * sb;
        int64_t k = (int64_t)std::floor(beat / sb + 1e-9);
        if ((k & 1) && beat < k * sb + off) --k;
        if (k == lastK || k < 0) return;
        lastK = k;
        // pattern change at the end of the playing pattern
        const int want = clampi((int)param[pPATTERN], 0, kPatterns - 1);
        if (want != curPattern && k % std::max<int>(1, P0.length) == 0) curPattern = want;
        const Pattern& P = bank.patterns[curPattern];
        const int L = std::max<int>(1, P.length);
        const int s = (int)(k % L), prev = (int)((k - 1 + L) % L);
        const double start = k * sb + ((k & 1) ? off : 0.0), dur = sb + ((k & 1) ? -off : off);
        if (!P.on[s]) { if (gate && held == 0) gate = false; gateOffBeat = -1.0; return; }
        const bool glide = P.on[prev] && P.slide[prev] && gate && k > 0;
        noteOn(P.note[s] + midiTranspose, P.accent[s] != 0, glide);
        const int nxt = (s + 1) % L;
        gateOffBeat = (P.slide[s] && P.on[nxt]) ? -1.0 : start + dur * 0.55;   // sliding steps hold the gate
    }

    inline void renderSample(float& outL, float& outR)
    {
        // pitch (glide while sliding)
        if (sliding) { logNote = targetNote + (logNote - targetNote) * slideCoef; if (std::fabs(logNote - targetNote) < 0.001f) { logNote = targetNote; sliding = false; } }
        else logNote = targetNote;
        const float f = 440.f * std::pow(2.f, (logNote + tune - 69.f) / 12.f);
        const double dt = std::min(0.45, (double)f / sr);
        phase += dt; if (phase >= 1.0) phase -= 1.0;
        const float ph = (float)phase, d = (float)dt;
        const float saw = 2.f * ph - 1.f - polyBlep(ph, d);
        const float sq = blepSquare(ph, d);
        float x = saw * (1.f - wave) + sq * wave * 0.8f;
        // envelopes
        fenv *= fenvCoef;
        accCap += (accTarget - accCap) * (1.f - accEnvCoef); accTarget *= accEnvCoef;   // accent "charge": repeated accents rise
        if (gate) { ampEnv *= ampDecCoef; amp = std::min(1.f, amp + atkStep); }
        else amp *= relCoef;
        const float a = amp * (0.25f + 0.75f * ampEnv) * (accentNow ? accGain : 1.f);
        if (a < 1e-6f && !gate) { lad.tick(0.f); x = 0.f; }
        // filter
        float y = lad.tick(x * 0.7f) * a;
        // drive + tone
        if (drive > 0.001f) y = drv.process(y * (1.f + drive * 2.f), drive) * 0.8f;
        y = tone.lp(y);
        y -= dcL.lp(y);   // DC blocker
        y *= vol;
        // ping-pong delay
        float l = y, r = y;
        if (dMix > 0.001f || std::fabs(dlyL[(size_t)dw]) > 1e-7f) {
            double pos = (double)dw - dTime; if (pos < 0) pos += dlyL.size();
            const size_t i0 = (size_t)pos % dlyL.size(), i1 = (i0 + 1) % dlyL.size(); const float fr = (float)(pos - std::floor(pos));
            const float el = dlyL[i0] + (dlyL[i1] - dlyL[i0]) * fr, er = dlyR[i0] + (dlyR[i1] - dlyR[i0]) * fr;
            dlyL[(size_t)dw] = y * dMix + dlpR.lp(er) * dFb;     // cross-feedback = ping-pong
            dlyR[(size_t)dw] = dlpL.lp(el) * dFb;
            l += el; r += er;
        }
        if (++dw >= (int)dlyL.size()) dw = 0;
        // safety
        outL = std::tanh(l * 0.9f) / 0.9f; outR = std::tanh(r * 0.9f) / 0.9f;
    }
};

} // namespace kn
