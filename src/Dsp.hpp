// Kinetik KMSP-6 — small DSP building blocks (shared with KDSP-16)
#pragma once
#include <cmath>
#include <cstdint>

namespace kn {

static constexpr float kPi = 3.14159265358979f;
static constexpr float kTwoPi = 6.28318530717959f;

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float expmap(float x, float lo, float hi) { return lo * std::pow(hi / lo, clampf(x, 0.f, 1.f)); }
// time (s) to fall 60 dB -> per-sample multiplier
inline float decayCoef(float seconds, float sr) { return std::exp(-6.9077553f / (seconds * sr)); }

struct Rng {
    uint32_t s = 0x9E3779B9u;
    inline float next() // -1..1
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float)(int32_t)s * (1.0f / 2147483648.0f);
    }
    inline float uni() { return next() * 0.5f + 0.5f; }
};

// Topology-preserving state variable filter
struct Svf {
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1.4f;
    float lp = 0, bp = 0, hp = 0;
    void setup(float fc, float q, float sr)
    {
        fc = clampf(fc, 10.f, sr * 0.45f);
        const float g = std::tan(kPi * fc / sr);
        k = 1.f / clampf(q, 0.3f, 40.f);
        a1 = 1.f / (1.f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline void tick(float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float tick(float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setPeak(float fc, float q, float db, float sr)
    {
        const float A = std::pow(10.f, db / 40.f);
        const float w = kTwoPi * clampf(fc, 10.f, sr * 0.45f) / sr;
        const float al = std::sin(w) / (2.f * q);
        const float c = std::cos(w);
        const float a0 = 1.f + al / A;
        b0 = (1.f + al * A) / a0; b1 = -2.f * c / a0; b2 = (1.f - al * A) / a0;
        a1 = -2.f * c / a0; a2 = (1.f - al / A) / a0;
    }
    void setShelf(bool high, float fc, float db, float sr)
    {
        const float A = std::pow(10.f, db / 40.f);
        const float w = kTwoPi * clampf(fc, 10.f, sr * 0.45f) / sr;
        const float c = std::cos(w), s = std::sin(w);
        const float al = s / 2.f * std::sqrt(2.f);
        const float sa = 2.f * std::sqrt(A) * al;
        float a0;
        if (!high) {
            a0 = (A + 1) + (A - 1) * c + sa;
            b0 = A * ((A + 1) - (A - 1) * c + sa) / a0;
            b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0;
            b2 = A * ((A + 1) - (A - 1) * c - sa) / a0;
            a1 = -2 * ((A - 1) + (A + 1) * c) / a0;
            a2 = ((A + 1) + (A - 1) * c - sa) / a0;
        } else {
            a0 = (A + 1) - (A - 1) * c + sa;
            b0 = A * ((A + 1) + (A - 1) * c + sa) / a0;
            b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
            b2 = A * ((A + 1) + (A - 1) * c - sa) / a0;
            a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
            a2 = ((A + 1) - (A - 1) * c - sa) / a0;
        }
    }
    void reset() { z1 = z2 = 0; }
};

struct OnePole {
    float a = 0, y = 0;
    void setLP(float fc, float sr) { a = std::exp(-kTwoPi * clampf(fc, 1.f, sr * 0.49f) / sr); }
    inline float lp(float x) { y = x + a * (y - x); return y; }
    inline float hp(float x) { return x - lp(x); }
};

// RESONANCE knob (0..127) to filter Q: exponential (equal steps in dB of peak) with a slow start, so the
// first half of the knob stays musical and the strong peak is at the top. qMin at 0, qMax at 127.
inline float resoQ(float v127, float qMin, float qMax)
{
    const float x = clampf(v127 / 127.f, 0.f, 1.f);
    return qMin * std::pow(qMax / qMin, std::pow(x, 1.25f));
}

// soft clip normalised so that |x|=1 stays ~1
inline float drive(float x, float amount) // amount 0..1
{
    if (amount <= 0.001f) return x;
    const float g = 1.f + amount * 9.f;
    return std::tanh(x * g) / std::tanh(g);
}

// the same soft clip with first-order antiderivative anti-aliasing (ADAA): the output is the average of tanh over
// the segment between two input samples, which removes most of the aliasing of a plain tanh at high drive.
// ADAA alone behaves like a two-sample average (-6 dB at 16 kHz), so the input is first lifted by a short
// symmetric FIR that keeps the result flat within 1 dB up to ~14 kHz. One instance per channel; adds 1.5 samples
// of delay while active.
struct DriveAA {
    double x1 = 0.0, x2 = 0.0, p1 = 0.0;
    static constexpr double kLift = 0.16;
    static inline double logCosh(double u) { const double a = std::fabs(u); return a + std::log1p(std::exp(-2.0 * a)) - 0.6931471805599453; }
    inline float process(float x, float amount)
    {
        const double p = (1.0 + 2.0 * kLift) * x1 - kLift * ((double)x + x2);   // lifted input, centred on x1
        x2 = x1; x1 = x;
        const double p0 = p1; p1 = p;
        if (amount <= 0.001f) return x;
        const double g = 1.0 + amount * 9.0;
        const double u0 = p0 * g, u1 = p * g, du = u1 - u0;
        const double y = std::fabs(du) < 1e-5 ? std::tanh(0.5 * (u0 + u1)) : (logCosh(u1) - logCosh(u0)) / du;
        return (float)(y / std::tanh(g));
    }
    void reset() { x1 = x2 = p1 = 0.0; }
};

inline float polyBlep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}
inline float blepSquare(float ph, float dt)
{
    float s = ph < 0.5f ? 1.f : -1.f;
    s += polyBlep(ph, dt);
    float t2 = ph + 0.5f; if (t2 >= 1.f) t2 -= 1.f;
    s -= polyBlep(t2, dt);
    return s;
}

} // namespace kn
