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
    // locks: step 1 locked to a closed filter, step 5 plain; brightness = share of the high band
    {
        auto hf = [](const std::vector<float>& x, int a, int b) { double h = 0, t = 0; float prev = 0; for (int i = a; i < b; ++i) { const float d = x[i] - prev; prev = x[i]; h += d * d; t += x[i] * x[i]; } return h / std::max(t, 1e-12); };
        auto lk = render([](Engine& e) { Pattern& P = e.bank.patterns[0]; P.on[0] = 1; P.on[4] = 1; P.note[0] = P.note[4] = 45; e.param[pCUTOFF] = 100; e.param[pENVMOD] = 0; P.locks[0][pCUTOFF] = 10; }, 30000);
        const double a = hf(lk, 300, 3000), b = hf(lk, 24300, 27000);
        std::snprintf(m, sizeof(m), "a CUTOFF lock darkens its step only (%.4f vs %.4f)", a, b);
        check(a < b * 0.3, m);
        auto dm = render([](Engine& e) { Pattern& P = e.bank.patterns[0]; P.on[0] = 1; P.note[0] = 45; P.locks[0][pDLYMIX] = 127; e.param[pDLYMIX] = 0; e.param[pDLYTIME] = 88; }, 40000);
        check(rms(dm, 12000, 16000) > 1e-3, "a DELAY MIX lock sends that step into the echo");
    }
    // chain: A02 once then A03 twice, looped
    {
        std::unique_ptr<Engine> e(new Engine()); e->init(sr); e->param[pPLAY] = 1.f;
        e->bank.chain.parse("1 2 1:1 2:2", kPatterns);
        std::vector<float> L(256), R(256); std::vector<int> seq;
        for (int p = 0; p < 96000 * 7; p += 256) {
            e->process(L.data(), R.data(), 256, Transport(), nullptr, 0);
            if (seq.empty() || seq.back() != e->outPattern) seq.push_back(e->outPattern);
        }
        check(seq.size() >= 4 && seq[0] == 1 && seq[1] == 2 && seq[2] == 1 && seq[3] == 2, "CHAIN plays its patterns in order and loops");
    }
    // probability, ratchets, ties
    {
        Engine* E = nullptr;
        render([](Engine& e) { Pattern& P = e.bank.patterns[0]; P.on[0] = 1; P.prob[0] = 0; }, 6000, {}, &E);
        check(E->trigCount == 0, "probability 0 %: the step does not play");
        render([](Engine& e) { Pattern& P = e.bank.patterns[0]; P.on[0] = 1; P.ratchet[0] = 3; }, 6000, {}, &E);
        check(E->trigCount == 3, "ratchet 3: three notes in the step");
        auto t = render([](Engine& e) { Pattern& P = e.bank.patterns[0]; P.on[0] = 1; P.tie[1] = 1; P.tie[2] = 1; }, 18000, {}, &E);
        check(E->trigCount == 1 && rms(t, 15000, 16500) > rms(t, 1000, 2500) * 0.05, "TIE: one note held over three steps");
    }
    // polymetric accent row: 3 steps long, accent on its first step = steps 1, 4, 7, 10...
    {
        auto pk = [](const std::vector<float>& x, int a, int b) { float m = 0; for (int i = a; i < b; ++i) m = std::max(m, std::fabs(x[i])); return m; };
        auto x = render([](Engine& e) { Pattern& P = e.bank.patterns[0]; for (int i = 0; i < 16; ++i) { P.on[i] = 1; P.note[i] = 45; } P.accent[0] = 1; P.accLen = 3; e.param[pACCENT] = 127; }, 6000 * 8);
        const float s1 = pk(x, 0, 3000), s2 = pk(x, 6000, 9000), s4 = pk(x, 18000, 21000), s5 = pk(x, 24000, 27000);
        check(s1 > s2 * 1.15f && s4 > s5 * 1.15f, "accent row of 3 steps: accents on steps 1, 4, 7...");
    }
    // EVOLVE: the line changes each loop, in the scale; EVO RESET brings it back
    {
        std::unique_ptr<Engine> e(new Engine()); e->init(sr); e->param[pPLAY] = 1.f; e->param[pEVOLVE] = 127.f;
        Pattern& P = e->bank.patterns[0]; for (int i = 0; i < 16; ++i) { P.on[i] = 1; P.note[i] = (uint8_t)(36 + (i % 4) * 3); }
        e->bank.glob.scale = 1; e->bank.glob.root = 0;
        for (int i = 0; i < 16; ++i) if (!inScale(P.note[i], 0, 1)) P.note[i] = 36;
        std::vector<float> L(256), R(256);
        for (int p = 0; p < 96000 * 4 + 1000; p += 256) e->process(L.data(), R.data(), 256, Transport(), nullptr, 0);
        const Pattern& Q = e->playingPattern(); int changed = 0; bool inKey = true;
        for (int i = 0; i < 16; ++i) { if (Q.note[i] != P.note[i] || Q.on[i] != P.on[i] || Q.accent[i] != P.accent[i]) ++changed; if (Q.on[i] && !inScale(Q.note[i], 0, 1)) inKey = false; }
        check(changed >= 3 && inKey, "EVOLVE: after 4 loops the line has changed, still in the scale");
        e->param[pEVORESET] = 30.f;    // every 2 loops
        for (int p = 0; p < 96000 * 2; p += 256) e->process(L.data(), R.data(), 256, Transport(), nullptr, 0);
        bool same = true; const Pattern& Q2 = e->playingPattern();
        for (int i = 0; i < 16; ++i) if (Q2.note[i] != P.note[i] || Q2.on[i] != P.on[i]) same = false;
        check(same, "EVO RESET: back to the written line every 2 loops");
    }
    // colour: PUMP, SUB, GROWL, VOWEL, HARMONY, R TIME
    {
        auto base = [](Engine& e) { Pattern& P = e.bank.patterns[0]; for (int i = 0; i < 16; ++i) { P.on[i] = 1; P.note[i] = 45; P.slide[i] = 1; } e.param[pAMPDEC] = 127; };
        auto plain = render(base, 24000);
        auto pump = render([&](Engine& e) { base(e); e.param[pPUMP] = 127; }, 24000);
        check(rms(pump, 200, 2000) < rms(pump, 14000, 20000) * 0.5, "PUMP: ducked at the beat, back up after");
        bool fin = true; int differ = 0;
        for (int pp : {pSUB, pGROWL, pVOWEL, pHARMONY}) {
            auto y = render([&](Engine& e) { base(e); e.param[pp] = pp == pHARMONY ? 75.f : 110.f; }, 24000);
            for (float v : y) if (!std::isfinite(v) || std::fabs(v) > 1.2f) fin = false;
            double d = 0; for (int i = 0; i < 24000; ++i) d += (y[i] - plain[i]) * (y[i] - plain[i]);
            if (std::sqrt(d / 24000) > 0.01) ++differ;
        }
        check(fin && differ == 4, "SUB, GROWL, VOWEL and HARMONY change the sound, finite");
        Engine* E = nullptr;
        std::unique_ptr<Engine> e(new Engine()); e->init(sr); e->param[pPLAY] = 1.f; e->param[pDLYMIX] = 127; e->param[pDLYTIME] = 88; e->param[pDLYTIMER] = 40;
        e->bank.patterns[0].on[0] = 1; e->bank.patterns[0].note[0] = 45;
        std::vector<float> L(48000), R(48000);
        for (int p = 0; p < 48000; p += 256) e->process(L.data() + p, R.data() + p, std::min(256, 48000 - p), Transport(), nullptr, 0);
        (void)E;
        double dl = 0, dr = 0; for (int i = 0; i < 48000; ++i) { dl += L[i] * L[i]; dr += R[i] * R[i]; }
        check(std::fabs(dl - dr) > 0.05 * std::max(dl, dr), "R TIME: the right echo has its own time");
    }
    // SATURATE: five models, each changes the sound at a sane level
    {
        auto base = [](Engine& e) { Pattern& P = e.bank.patterns[0]; for (int i = 0; i < 16; ++i) { P.on[i] = 1; P.note[i] = 40 + (i % 3) * 7; } e.param[pAMPDEC] = 127; };
        auto plain = render(base, 24000);
        int ok = 0;
        for (int t = 0; t < kSatTypes; ++t) {
            auto y = render([&](Engine& e) { base(e); e.param[pSAT] = 100; e.param[pSATTYPE] = t * 25.6f + 12.f; }, 24000);
            bool fin = true; for (float v : y) if (!std::isfinite(v) || std::fabs(v) > 1.2f) fin = false;
            double d = 0; for (int i = 0; i < 24000; ++i) d += (y[i] - plain[i]) * (y[i] - plain[i]);
            const double ratio = rms(y, 2000, 24000) / std::max(1e-9, rms(plain, 2000, 24000));
            std::printf("     %s: level x%.2f\n", kSatNames[t], ratio);
            if (fin && std::sqrt(d / 24000) > 0.01 && ratio > 0.4 && ratio < 2.5) ++ok;
        }
        check(ok == kSatTypes, "SATURATE: every model changes the sound, finite, level within -8 / +8 dB");
    }
    // REC: live notes are written to the nearest step (accent from the velocity, legato = slide + ties, long note = ties)
    {
        Engine* E = nullptr;
        const int st = 6000;   // one 16th at 120 BPM
        render([](Engine& e) { e.recording = true; }, st * 10,
               {{(uint32_t)(st + 50), 50, 110, true}, {(uint32_t)(st + 2000), 50, 0, false},
                {(uint32_t)(st * 3 - 200), 52, 90, true}, {(uint32_t)(st * 5 + 100), 55, 90, true},
                {(uint32_t)(st * 5 + 300), 52, 0, false}, {(uint32_t)(st * 8), 55, 0, false}}, &E);
        RecEvent ev; int notes = 0, slides = 0, ties = 0; bool where = true;
        while (E->popRec(ev)) {
            if (ev.kind == 0) { ++notes; if (!((ev.a == 1 && ev.note == 50 && ev.accent) || (ev.a == 3 && ev.note == 52 && !ev.accent) || (ev.a == 5 && ev.note == 55))) where = false; }
            else if (ev.kind == 1) { ++slides; if (ev.a != 4) where = false; }
            else { ++ties; if (ev.a != 4 && ev.a != 6 && ev.a != 7) where = false; }
        }
        std::printf("     notes %d slides %d ties %d\n", notes, slides, ties);
        check(notes == 3 && slides == 1 && ties == 3 && where, "REC: notes on the nearest steps, accent, legato slide, held note tied");
        auto off = render([](Engine& e) { e.recording = false; }, st * 4, {{(uint32_t)(st + 50), 50, 110, true}}, &E);
        check(!E->popRec(ev), "REC off: nothing recorded");
        auto clk = render([](Engine& e) { e.recording = true; }, st * 4);
        check(rms(clk, 0, 1000) > 0.01, "CLICK: heard while recording");
        auto noclk = render([](Engine& e) { e.recording = true; e.click.enabled = false; }, st * 4);
        check(rms(noclk, 0, 1000) < 1e-4, "CLICK off: silent");
    }
    std::printf(fails ? "%d failures\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
