// Kinetik KBAS-1 — SATURATE: five saturation models after the filter, run at twice the sample rate
#pragma once
#include "Dsp.hpp"
#include <cmath>

namespace kn {

// 2-pole Butterworth low pass (direct form 2 transposed)
struct LowBiquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set(float fc, float sr, float q = 0.7071f)
    {
        const float w = kTwoPi * clampf(fc, 10.f, sr * 0.49f) / sr;
        const float al = std::sin(w) / (2.f * q), c = std::cos(w), a0 = 1.f + al;
        b0 = (1.f - c) * 0.5f / a0; b1 = (1.f - c) / a0; b2 = b0; a1 = -2.f * c / a0; a2 = (1.f - al) / a0;
    }
    inline float tick(float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0.f; }
};

struct Saturator {
    enum { kTube, kTape, kDiode, kFuzz, kFold };
    int type = kTube; float amount = 0.f;     // 0..1
    float sr = 48000.f;
    LowBiquad up[2], dn[2];                    // 4-pole anti-image / anti-alias filters at the 2x rate
    OnePole tapeIn, tapeOut, dc;
    float eIn = 0.f, eOut = 0.f, envC = 0.999f;   // level match: the louder models do not jump out
    void init(float s)
    {
        sr = s;
        for (int i = 0; i < 2; ++i) { up[i].set(sr * 0.45f, sr * 2.f, i ? 1.3066f : 0.5412f); dn[i] = up[i]; }
        tapeIn.setLP(2500.f, sr * 2.f); dc.setLP(12.f, sr); envC = std::exp(-1.f / (0.08f * sr));
        reset();
    }
    void setTapeTone() { tapeOut.setLP(13000.f - amount * 6000.f, sr * 2.f); }
    void reset() { for (auto& f : up) f.reset(); for (auto& f : dn) f.reset(); tapeIn.y = tapeOut.y = dc.y = 0.f; eIn = eOut = 0.f; }

    inline float shape(float x) const
    {
        const float g = 1.f + amount * 9.f;
        switch (type) {
        case kTube: {        // asymmetric soft clip: even harmonics, warm
            const float bias = 0.18f + amount * 0.25f;
            return (std::tanh(x * g + bias) - std::tanh(bias)) / std::sqrt(g);
        }
        case kTape: {        // gentle symmetric curve that also squashes the peaks
            const float u = x * g * 0.8f;
            return u / std::sqrt(1.f + u * u) * 1.3f / std::sqrt(g);
        }
        case kDiode: {       // sharper, one side clips earlier
            const float u = x * g;
            const float y = u > 0.f ? 1.f - std::exp(-u) : -0.55f * (1.f - std::exp(u / 0.55f));
            return y * 1.4f / std::sqrt(g);
        }
        case kFuzz: {        // very high gain, hard edges
            const float u = x * g * 3.f;
            return clampf(u, -1.f, 1.f) * 0.55f + std::tanh(u) * 0.25f;
        }
        default: {           // FOLD: the wave folds back over itself (hollow, vocal)
            const float u = x * (1.f + amount * 5.f);
            return std::sin(u * 1.5707963f) * 0.7f;
        }
        }
    }
    inline float stage(float x)
    {
        if (type == kTape) {                           // pre-emphasis / de-emphasis around the curve: highs saturate more
            const float hi = x - tapeIn.lp(x);
            x = shape(x + hi * amount * 0.8f);
            return tapeOut.lp(x);
        }
        return shape(x);
    }
    inline float process(float x)
    {
        if (amount < 0.001f) return x;
        // 2x: zero-stuffed, filtered, shaped, filtered, decimated
        float a = up[1].tick(up[0].tick(x * 2.f));
        float b = up[1].tick(up[0].tick(0.f));
        a = dn[1].tick(dn[0].tick(stage(a)));
        dn[1].tick(dn[0].tick(stage(b)));
        a -= dc.lp(a);                                              // the asymmetric curves add DC
        eIn = x * x + (eIn - x * x) * envC; eOut = a * a + (eOut - a * a) * envC;
        if (eOut > 1e-9f) a *= clampf(std::pow(eIn / eOut, 0.4f), 0.3f, 2.f);   // most of the level difference made up
        const float wet = amount < 0.25f ? amount * 4.f : 1.f;     // small amounts blend in
        return x + (a - x) * wet;
    }
};

} // namespace kn
