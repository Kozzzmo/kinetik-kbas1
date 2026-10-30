// Kinetik KBAS-1 — patterns (steps with note, accent, slide), text format and edit commands
#pragma once
#include "Params.hpp"
#include "Chain.hpp"
#include <cstring>
#include <cstdlib>
#include <string>

namespace kn {

struct Pattern {
    uint8_t length = 16, swing = 50;
    uint8_t on[kSteps];          // gate on this step
    uint8_t note[kSteps];        // MIDI note (60 = C3)
    uint8_t accent[kSteps];
    uint8_t slide[kSteps];       // glide from this step into the next one (no new attack, gate held)
    uint8_t locks[kSteps][kLockParams];   // per-step sound values (kNoLock = the knob)
    uint8_t prob[kSteps];        // chance the step plays, 0..100 %
    uint8_t ratchet[kSteps];     // 1..4 notes inside the step
    uint8_t tie[kSteps];         // 1 = hold the previous note through this step (no new attack)
    uint8_t accLen = 0, slideLen = 0;     // own lengths of the ACCENT / SLIDE rows (0 = the pattern length): polymetric acid
    Pattern() { clear(); }
    void clear()
    {
        length = 16; swing = 50;
        std::memset(on, 0, sizeof(on)); std::memset(accent, 0, sizeof(accent)); std::memset(slide, 0, sizeof(slide));
        std::memset(note, 36, sizeof(note));   // C1
        std::memset(locks, kNoLock, sizeof(locks));
        std::memset(prob, 100, sizeof(prob)); std::memset(ratchet, 1, sizeof(ratchet)); std::memset(tie, 0, sizeof(tie));
        accLen = slideLen = 0;
    }
    int accRow() const { return accLen ? accLen : length; }
    int slideRow() const { return slideLen ? slideLen : length; }
    bool hasLocks(int s) const { for (int k = 0; k < kLockParams; ++k) if (locks[s][k] != kNoLock) return true; return false; }
    bool empty() const { for (int s = 0; s < kSteps; ++s) if (on[s]) return false; return true; }
    bool isDefault() const
    {
        if (length != 16 || swing != 50 || accLen || slideLen) return false;
        for (int s = 0; s < kSteps; ++s) if (prob[s] != 100 || ratchet[s] != 1 || tie[s]) return false;
        for (int s = 0; s < kSteps; ++s) if (on[s] || accent[s] || slide[s] || note[s] != 36 || hasLocks(s)) return false;
        return true;
    }
    void rotate(int n)
    {
        const int L = length; Pattern t = *this;
        for (int s = 0; s < L; ++s) {
            const int d = ((s + n) % L + L) % L;
            on[d] = t.on[s]; note[d] = t.note[s]; accent[d] = t.accent[s]; slide[d] = t.slide[s];
            std::memcpy(locks[d], t.locks[s], kLockParams);
            prob[d] = t.prob[s]; ratchet[d] = t.ratchet[s]; tie[d] = t.tie[s];
        }
    }
};

struct Global {
    uint8_t scale = 1, root = 0;         // ACID GEN / note snap (root 0 = C)
    uint8_t transpose = 0;               // 1 = MIDI keys transpose the playing pattern (C3 = no change)
    uint8_t density = 70;                // 0.4: DENSITY of ACID GEN / RANDOM (% of the steps that play)
};

struct Bank {
    Pattern patterns[kPatterns];
    Global glob;
    Chain chain;
    void initDefault() { for (auto& p : patterns) p.clear(); glob = Global(); chain = Chain(); }
};

static constexpr int kGlobTouched = 1000;

inline std::string stepToken(const Pattern& p, int s)
{
    char b[16];
    std::snprintf(b, sizeof(b), "%d%c%c%c", p.note[s], p.on[s] ? 'x' : '.', p.accent[s] ? 'A' : '-', p.slide[s] ? 'S' : '-');
    return b;
}

inline std::string serializeBank(const Bank& b)
{
    std::string s = "KBAS1\n";
    char buf[96];
    std::snprintf(buf, sizeof(buf), "G %d %d %d %d\n", b.glob.scale, b.glob.root, b.glob.transpose, b.glob.density); s += buf;
    if (!b.chain.isDefault()) s += b.chain.line() + "\n";
    for (int p = 0; p < kPatterns; ++p) {
        const Pattern& P = b.patterns[p];
        if (P.isDefault()) continue;
        std::snprintf(buf, sizeof(buf), "P %d %d %d\n", p, P.length, P.swing); s += buf;
        std::snprintf(buf, sizeof(buf), "Q %d", p); s += buf;
        for (int st = 0; st < kSteps; ++st) { s += ' '; s += stepToken(P, st); }
        s += "\n";
        if (P.accLen || P.slideLen) { std::snprintf(buf, sizeof(buf), "R %d %d %d\n", p, P.accLen, P.slideLen); s += buf; }
        for (int st = 0; st < kSteps; ++st)
            if (P.prob[st] != 100 || P.ratchet[st] != 1 || P.tie[st]) { std::snprintf(buf, sizeof(buf), "y %d %d %d %d %d\n", p, st, P.prob[st], P.ratchet[st], P.tie[st]); s += buf; }
        for (int st = 0; st < kSteps; ++st)
            for (int k = 0; k < kLockParams; ++k)
                if (P.locks[st][k] != kNoLock) { std::snprintf(buf, sizeof(buf), "k %d %d %d %d\n", p, st, k, P.locks[st][k]); s += buf; }
    }
    return s;
}

// one edit line; returns the pattern touched, kGlobTouched, or -1
//   P p length swing     s p step on note accent slide     o / n / a / l p step value (on / note / accent / slide)
//   R p accentLength slideLength (0 = pattern length)     y p step probability ratchet tie
//   k p step param value (255 = clear)     kc p step     C on n p:r ... (chain)
//   Q p <64 tokens "<note><x|.><A|-><S|->">               G scale root transpose [density]     cl p     cp src dst     rot p n
inline int applyCommand(Bank& b, const char* line)
{
    char op[8] = {0};
    if (std::sscanf(line, "%7s", op) != 1) return -1;
    const char* a = line + std::strlen(op);
    int v[6] = {0};
    auto okP = [](int p) { return p >= 0 && p < kPatterns; };
    auto okS = [](int s) { return s >= 0 && s < kSteps; };
    if (!std::strcmp(op, "P") && std::sscanf(a, "%d %d %d", &v[0], &v[1], &v[2]) == 3 && okP(v[0])) {
        b.patterns[v[0]].length = (uint8_t)clampi(v[1], 1, kSteps); b.patterns[v[0]].swing = (uint8_t)clampi(v[2], 50, 80); return v[0];
    }
    if (!std::strcmp(op, "s") && std::sscanf(a, "%d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6 && okP(v[0]) && okS(v[1])) {
        Pattern& P = b.patterns[v[0]];
        P.on[v[1]] = v[2] ? 1 : 0; P.note[v[1]] = (uint8_t)clampi(v[3], 0, 127); P.accent[v[1]] = v[4] ? 1 : 0; P.slide[v[1]] = v[5] ? 1 : 0;
        return v[0];
    }
    if ((!std::strcmp(op, "o") || !std::strcmp(op, "n") || !std::strcmp(op, "a") || !std::strcmp(op, "l"))
        && std::sscanf(a, "%d %d %d", &v[0], &v[1], &v[2]) == 3 && okP(v[0]) && okS(v[1])) {
        Pattern& P = b.patterns[v[0]];
        switch (op[0]) {
        case 'o': P.on[v[1]] = v[2] ? 1 : 0; break;
        case 'n': P.note[v[1]] = (uint8_t)clampi(v[2], 0, 127); break;
        case 'a': P.accent[v[1]] = v[2] ? 1 : 0; break;
        default: P.slide[v[1]] = v[2] ? 1 : 0; break;
        }
        return v[0];
    }
    if (!std::strcmp(op, "Q") && std::sscanf(a, "%d", &v[0]) == 1 && okP(v[0])) {
        Pattern& P = b.patterns[v[0]];
        const char* c = a; while (*c == ' ') ++c; while (*c && *c != ' ') ++c;   // skip the pattern number
        for (int st = 0; st < kSteps; ++st) {
            while (*c == ' ') ++c;
            if (!*c) break;
            char* end = nullptr; const long n = std::strtol(c, &end, 10);
            if (end == c) break;
            c = end;
            P.note[st] = (uint8_t)clampi((int)n, 0, 127);
            P.on[st] = *c == 'x'; if (*c) ++c;
            P.accent[st] = *c == 'A'; if (*c) ++c;
            P.slide[st] = *c == 'S'; if (*c) ++c;
        }
        return v[0];
    }
    if (!std::strcmp(op, "G") && std::sscanf(a, "%d %d %d %d", &v[0], &v[1], &v[2], &v[3]) >= 3) {
        if (std::sscanf(a, "%d %d %d %d", &v[0], &v[1], &v[2], &v[3]) < 4) v[3] = 70;
        b.glob.scale = (uint8_t)clampi(v[0], 0, kScales - 1); b.glob.root = (uint8_t)clampi(v[1], 0, 11); b.glob.transpose = v[2] ? 1 : 0;
        b.glob.density = (uint8_t)clampi(v[3], 0, 100);
        return kGlobTouched;
    }
    if (!std::strcmp(op, "k") && std::sscanf(a, "%d %d %d %d", &v[0], &v[1], &v[2], &v[3]) == 4 && okP(v[0]) && okS(v[1]) && v[2] >= 0 && v[2] < kLockParams) {
        b.patterns[v[0]].locks[v[1]][v[2]] = v[3] >= 255 ? kNoLock : (uint8_t)clampi(v[3], 0, 127); return v[0];
    }
    if (!std::strcmp(op, "kc") && std::sscanf(a, "%d %d", &v[0], &v[1]) == 2 && okP(v[0]) && okS(v[1])) { std::memset(b.patterns[v[0]].locks[v[1]], kNoLock, kLockParams); return v[0]; }
    if (!std::strcmp(op, "C")) return b.chain.parse(a, kPatterns) ? kGlobTouched : -1;
    if (!std::strcmp(op, "R") && std::sscanf(a, "%d %d %d", &v[0], &v[1], &v[2]) == 3 && okP(v[0])) {
        b.patterns[v[0]].accLen = (uint8_t)clampi(v[1], 0, kSteps); b.patterns[v[0]].slideLen = (uint8_t)clampi(v[2], 0, kSteps); return v[0];
    }
    if (!std::strcmp(op, "y")) {
        v[4] = 0;
        if (std::sscanf(a, "%d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4]) >= 4 && okP(v[0]) && okS(v[1])) {
            b.patterns[v[0]].prob[v[1]] = (uint8_t)clampi(v[2], 0, 100); b.patterns[v[0]].ratchet[v[1]] = (uint8_t)clampi(v[3], 1, 4); b.patterns[v[0]].tie[v[1]] = v[4] ? 1 : 0;
            return v[0];
        }
        return -1;
    }
    if (!std::strcmp(op, "cl") && std::sscanf(a, "%d", &v[0]) == 1 && okP(v[0])) { b.patterns[v[0]].clear(); return v[0]; }
    if (!std::strcmp(op, "cp") && std::sscanf(a, "%d %d", &v[0], &v[1]) == 2 && okP(v[0]) && okP(v[1])) { b.patterns[v[1]] = b.patterns[v[0]]; return v[1]; }
    if (!std::strcmp(op, "rot") && std::sscanf(a, "%d %d", &v[0], &v[1]) == 2 && okP(v[0])) { b.patterns[v[0]].rotate(v[1]); return v[0]; }
    return -1;
}

inline bool deserializeBank(Bank& b, const char* text)
{
    if (!text || std::strncmp(text, "KBAS1", 5) != 0) return false;
    b.initDefault();
    const char* s = text; std::string line;
    while (*s) {
        const char* e = std::strchr(s, '\n');
        const size_t n = e ? (size_t)(e - s) : std::strlen(s);
        line.assign(s, n);
        if (!line.empty() && line.rfind("KBAS", 0) != 0) applyCommand(b, line.c_str());
        s += n; if (*s == '\n') ++s;
    }
    return true;
}

} // namespace kn
