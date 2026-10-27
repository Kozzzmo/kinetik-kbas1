// Kinetik — metronome click while the sequencer records (shared: identical in kinetik-kdsp16 and kinetik-kmsp6)
#pragma once
#include <cmath>
#include <cstdint>
#include <climits>

namespace kn {

// A short sine "tick" on every quarter note, higher and louder on the first beat of each 4/4 bar.
// Added to the main output after the master effects; silent (and nearly free) when not active.
struct Click {
    bool enabled = true;          // user setting (CLICK button)
    float sr = 48000.f;
    int64_t lastBeat = INT64_MIN;
    float env = 0.f, ph = 0.f, inc = 0.f, decay = 0.999f;

    void init(float s)
    {
        sr = s > 1.f ? s : 48000.f;
        decay = std::exp(-1.f / (0.014f * sr));
        reset();
    }
    void reset() { lastBeat = INT64_MIN; env = 0.f; ph = 0.f; }

    // n frames from song position b0 (in quarter notes), bpf = quarter notes per frame;
    // active = recording while the sequencer plays
    void process(float* l, float* r, uint32_t n, double b0, double bpf, bool active)
    {
        const bool on = active && enabled;
        if (!on) { lastBeat = INT64_MIN; if (env < 1e-4f) return; }
        constexpr float kTwoPi = 6.28318530718f;
        for (uint32_t i = 0; i < n; ++i) {
            if (on) {
                const double pos = b0 + i * bpf;
                const int64_t b = (int64_t)std::floor(pos + 1e-9);
                if (b != lastBeat) {
                    // only on the frame that crosses the beat (not when recording starts mid-beat)
                    if (pos - (double)b < bpf * 1.5 && b >= 0) {
                        const bool bar = (b % 4) == 0;
                        inc = kTwoPi * (bar ? 1760.f : 1320.f) / sr;
                        env = bar ? 0.42f : 0.26f;
                        ph = 0.f;
                    }
                    lastBeat = b;
                }
            }
            if (env > 1e-4f) {
                const float s = std::sin(ph) * env;
                ph += inc; if (ph > kTwoPi) ph -= kTwoPi;
                env *= decay;
                l[i] += s; r[i] += s;
            }
        }
    }
};

} // namespace kn
