// Kinetik — pattern CHAIN (shared by KDSP-16, KMSP-6, KFX-8, KBAS-1, KGRN-4; keep identical): a list of patterns, each
// played a number of times, looped while CHAIN is on. Bank line "C <on> <n> <pattern>:<repeats> ...".
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace kn {

struct Chain {
    static constexpr int kMax = 16;
    uint8_t on = 0, n = 0;
    uint8_t pat[kMax] = {}, rep[kMax] = {};
    bool active() const { return on && n > 0; }
    bool isDefault() const { return !on && n == 0; }
    void clear() { on = 0; n = 0; }
    void add(int p, int r = 1) { if (n < kMax) { pat[n] = (uint8_t)p; rep[n] = (uint8_t)(r < 1 ? 1 : (r > 16 ? 16 : r)); ++n; } }
    void remove(int i) { if (i < 0 || i >= n) return; for (int k = i; k + 1 < n; ++k) { pat[k] = pat[k + 1]; rep[k] = rep[k + 1]; } --n; if (!n) on = 0; }
    std::string line() const
    {
        std::string s = "C " + std::to_string(on) + " " + std::to_string(n);
        for (int i = 0; i < n; ++i) s += " " + std::to_string(pat[i]) + ":" + std::to_string(rep[i]);
        return s;
    }
    // parses the arguments after "C"; patterns are clamped to 0..patterns-1
    bool parse(const char* a, int patterns)
    {
        char* e = nullptr;
        const long o = std::strtol(a, &e, 10); if (e == a) return false; a = e;
        const long cnt = std::strtol(a, &e, 10); if (e == a) return false; a = e;
        Chain c; c.on = o ? 1 : 0;
        for (long i = 0; i < cnt && i < kMax; ++i) {
            const long p = std::strtol(a, &e, 10); if (e == a) break; a = e;
            long r = 1; if (*a == ':') { ++a; r = std::strtol(a, &e, 10); a = e; }
            if (p < 0 || p >= patterns) continue;
            c.add((int)p, (int)r);
        }
        if (!c.n) c.on = 0;
        *this = c;
        return true;
    }
};

// the engine side: call start() when the transport (re)starts, next() at every end of the playing pattern
// (engines that also see a boundary at the very first step pass skipFirst = true to start())
struct ChainPlayer {
    int pos = 0, reps = 0; bool running = false, fresh = false;
    void reset() { pos = 0; reps = 0; running = false; fresh = false; }
    int start(const Chain& c, bool skipFirst = false) { reset(); if (!c.active()) return -1; running = true; fresh = skipFirst; return c.pat[0]; }
    // the pattern to play next, or -1 when the chain is off
    int next(const Chain& c)
    {
        if (!c.active()) { running = false; return -1; }
        if (!running) { running = true; pos = 0; reps = 0; fresh = false; return c.pat[0]; }
        if (pos >= c.n) { pos = 0; reps = 0; fresh = false; return c.pat[0]; }
        if (fresh) { fresh = false; return c.pat[pos]; }
        if (++reps >= c.rep[pos]) { reps = 0; pos = (pos + 1) % c.n; }
        return c.pat[pos];
    }
    // what next() would return, without moving
    int peek(const Chain& c) const
    {
        if (!c.active()) return -1;
        if (!running || pos >= c.n) return c.pat[0];
        if (fresh) return c.pat[pos];
        return reps + 1 >= c.rep[pos] ? c.pat[(pos + 1) % c.n] : c.pat[pos];
    }
};

} // namespace kn
