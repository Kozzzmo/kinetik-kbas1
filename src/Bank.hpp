// Kinetik KBAS-1 — patterns (steps with note, accent, slide), text format and edit commands
#pragma once
#include "Params.hpp"
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
    Pattern() { clear(); }
    void clear()
    {
        length = 16; swing = 50;
        std::memset(on, 0, sizeof(on)); std::memset(accent, 0, sizeof(accent)); std::memset(slide, 0, sizeof(slide));
        std::memset(note, 36, sizeof(note));   // C1
    }
    bool empty() const { for (int s = 0; s < kSteps; ++s) if (on[s]) return false; return true; }
    bool isDefault() const
    {
        if (length != 16 || swing != 50) return false;
        for (int s = 0; s < kSteps; ++s) if (on[s] || accent[s] || slide[s] || note[s] != 36) return false;
        return true;
    }
    void rotate(int n)
    {
        const int L = length; Pattern t = *this;
        for (int s = 0; s < L; ++s) {
            const int d = ((s + n) % L + L) % L;
            on[d] = t.on[s]; note[d] = t.note[s]; accent[d] = t.accent[s]; slide[d] = t.slide[s];
        }
    }
};

struct Global {
    uint8_t scale = 1, root = 0;         // ACID GEN / note snap (root 0 = C)
    uint8_t transpose = 0;               // 1 = MIDI keys transpose the playing pattern (C3 = no change)
};

struct Bank {
    Pattern patterns[kPatterns];
    Global glob;
    void initDefault() { for (auto& p : patterns) p.clear(); glob = Global(); }
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
    std::snprintf(buf, sizeof(buf), "G %d %d %d\n", b.glob.scale, b.glob.root, b.glob.transpose); s += buf;
    for (int p = 0; p < kPatterns; ++p) {
        const Pattern& P = b.patterns[p];
        if (P.isDefault()) continue;
        std::snprintf(buf, sizeof(buf), "P %d %d %d\n", p, P.length, P.swing); s += buf;
        std::snprintf(buf, sizeof(buf), "Q %d", p); s += buf;
        for (int st = 0; st < kSteps; ++st) { s += ' '; s += stepToken(P, st); }
        s += "\n";
    }
    return s;
}

// one edit line; returns the pattern touched, kGlobTouched, or -1
//   P p length swing     s p step on note accent slide     o / n / a / l p step value (on / note / accent / slide)
//   Q p <32 tokens "<note><x|.><A|-><S|->">               G scale root transpose     cl p     cp src dst     rot p n
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
    if (!std::strcmp(op, "G") && std::sscanf(a, "%d %d %d", &v[0], &v[1], &v[2]) == 3) {
        b.glob.scale = (uint8_t)clampi(v[0], 0, kScales - 1); b.glob.root = (uint8_t)clampi(v[1], 0, 11); b.glob.transpose = v[2] ? 1 : 0;
        return kGlobTouched;
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
