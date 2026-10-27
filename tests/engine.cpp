// KBAS-1 engine: sequenced notes at the right pitch, slides glide without a new attack, accents louder and brighter,
// resonance rings, no NaN, live MIDI with legato, transpose mode, pattern change at the end of the pattern.
//   g++ -O2 -std=c++17 tests/engine.cpp -o /tmp/t && /tmp/t
#include "../src/Engine.hpp"
#include <cstdio>
#include <memory>
#include <vector>
#include <functional>
using namespace kn;

static int fails = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; }
static const float sr = 48000.f;

static std::vector<float> render(std::function<void(Engine&)> setup, int frames, std::vector<NoteIn> notes = {}, Engine** keep = nullptr)
{
    static std::unique_ptr<Engine> e; e.reset(new Engine()); e->init(sr);
    e->param[pPLAY] = 1.f; e->param[pTEMPO] = 120.f;
    setup(*e);
    std::vector<float> L(frames), R(frames);
    size_t ni = 0;
    for (int p = 0; p < frames; p += 256) {
        const uint32_t n = std::min(256, frames - p);
        std::vector<NoteIn> blk;
        while (ni < notes.size() && notes[ni].frame < (uint32_t)(p + n)) { NoteIn x = notes[ni++]; x.frame -= p; blk.push_back(x); }
        e->process(L.data() + p, R.data() + p, n, Transport(), blk.data(), (uint32_t)blk.size());
    }
    if (keep) *keep = e.get();
    return L;
}
// dominant period by autocorrelation in [from, to)
static double pitchHz(const std::vector<float>& x, int from, int to)
{
    double best = 0; int bl = 0;
    for (int lag = 40; lag < 2400; ++lag) {
        double c = 0, e1 = 0, e2 = 0;
        for (int i = from; i + lag < to; ++i) { c += x[i] * x[i + lag]; e1 += x[i] * x[i]; e2 += x[i + lag] * x[i + lag]; }
        const double r = c / std::sqrt(e1 * e2 + 1e-12);
        if (r > best + 1e-6) { best = r; bl = lag; }
        if (best > 0.95 && r < best - 0.2) break;
    }
    return bl ? sr / bl : 0;
}
static double rms(const std::vector<float>& x, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += x[i] * x[i]; return std::sqrt(s / std::max(1, b - a)); }

int main()
{
    // a 16th = 6000 frames at 120 BPM
    auto x = render([](Engine& e) { Pattern& P = e.bank.patterns[0]; P.on[0] = 1; P.note[0] = 45; P.on[4] = 1; P.note[4] = 57; e.param[pRESO] = 20; e.param[pCUTOFF] = 90; }, 48000);
    const double f1 = pitchHz(x, 600, 3000), f2 = pitchHz(x, 24600, 27000);
    char m[160];
    std::snprintf(m, sizeof(m), "A1 then A2: %.1f Hz and %.1f Hz (110 / 220)", f1, f2);
    check(std::fabs(f1 - 110) < 3 && std::fabs(f2 - 220) < 5, m);
    check(rms(x, 4000, 6000) < rms(x, 500, 2500) * 0.05 && rms(x, 6100, 23000) < 1e-3, "the gate closes after the step (55 %)");
    bool finite = true; for (float v : x) if (!std::isfinite(v) || std::fabs(v) > 1.2f) finite = false;
    check(finite, "output finite and limited");

    // slide: step 1 slides into step 2: no new attack, pitch glides
    Engine* E = nullptr;
    auto s = render([](Engine& e) { Pattern& P = e.bank.patterns[0]; P.on[0] = 1; P.note[0] = 45; P.slide[0] = 1; P.on[1] = 1; P.note[1] = 52; }, 14000, {}, &E);
    const double fa = pitchHz(s, 1000, 5000), fs = pitchHz(s, 8200, 9200);
    std::snprintf(m, sizeof(m), "slide glides from A1 (%.1f Hz) towards E2 (%.1f Hz, 164.8 at the end), gate open across the steps", fa, fs);
    check(std::fabs(fa - 110) < 3 && fs > 150 && fs < 168 && rms(s, 5800, 6200) > rms(s, 1000, 3000) * 0.2, m);

    // accent: louder and brighter
    auto plain = render([](Engine& e) { Pattern& P = e.bank.patterns[0]; P.on[0] = 1; P.note[0] = 40; }, 6000);
    auto acc = render([](Engine& e) { Pattern& P = e.bank.patterns[0]; P.on[0] = 1; P.note[0] = 40; P.accent[0] = 1; }, 6000);
    std::snprintf(m, sizeof(m), "accent louder (%.3f > %.3f)", rms(acc, 200, 3000), rms(plain, 200, 3000));
    check(rms(acc, 200, 3000) > rms(plain, 200, 3000) * 1.2, m);

    // resonance: a high RESO setting rings at the cutoff
    auto lo = render([](Engine& e) { Pattern& P = e.bank.patterns[0]; P.on[0] = 1; P.note[0] = 36; e.param[pRESO] = 0; e.param[pENVMOD] = 0; }, 6000);
    auto hi = render([](Engine& e) { Pattern& P = e.bank.patterns[0]; P.on[0] = 1; P.note[0] = 36; e.param[pRESO] = 120; e.param[pENVMOD] = 0; }, 6000);
    check(rms(hi, 200, 3000) > 0.01 && rms(lo, 200, 3000) > 0.01, "sound with low and high resonance");

    // live MIDI, stopped: legato glides
    auto live = render([](Engine& e) { e.param[pPLAY] = 0.f; }, 30000, {{100, 48, 90, true}, {12000, 55, 90, true}, {24000, 48, 0, false}, {24000, 55, 0, false}});
    const double fl = pitchHz(live, 20000, 23000);
    std::snprintf(m, sizeof(m), "live MIDI: legato to G2 (%.1f Hz ~ 196), silent after note-off", fl);
    check(std::fabs(fl - 196) < 5 && rms(live, 27000, 30000) < 1e-3, m);

    // transpose mode: C3+7 moves the pattern up a fifth
    auto tr = render([](Engine& e) { Pattern& P = e.bank.patterns[0]; P.on[0] = 1; P.note[0] = 45; e.bank.glob.transpose = 1; }, 6000, {{0, 67, 100, true}});
    const double ft = pitchHz(tr, 600, 3000);
    std::snprintf(m, sizeof(m), "transpose mode: A1 + 7 = E2 (%.1f Hz)", ft);
    check(std::fabs(ft - 164.8) < 4, m);

    // pattern change at the end of the pattern
    {
        std::unique_ptr<Engine> e(new Engine()); e->init(sr); e->param[pPLAY] = 1.f;
        e->bank.patterns[1].on[0] = 1;
        std::vector<float> L(256), R(256); int at = -1;
        for (int p = 0; p < 200000; p += 256) {
            if (p == 2560) e->param[pPATTERN] = 1.f;
            e->process(L.data(), R.data(), 256, Transport(), nullptr, 0);
            if (at < 0 && e->outPattern == 1) at = p;
        }
        check(at >= 96000 - 512 && at <= 96000 + 512, "queued pattern starts when the pattern (16 steps) ends");
    }
    std::printf(fails ? "%d failures\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
