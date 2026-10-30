// KBAS-1 bank: text round trip, edit commands, factory presets (in their key), ACID GEN stays in the scale.
//   g++ -O2 -std=c++17 tests/bank.cpp -o /tmp/t && /tmp/t
#include "../src/Presets.hpp"
#include <cstdio>
#include <cstring>
using namespace kn;

static int fails = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; }
static bool same(const Bank& a, const Bank& b)
{
    if (std::memcmp(&a.glob, &b.glob, sizeof(Global))) return false;
    for (int p = 0; p < kPatterns; ++p) {
        const Pattern &x = a.patterns[p], &y = b.patterns[p];
        if (x.length != y.length || x.swing != y.swing) return false;
        for (int s = 0; s < kSteps; ++s)
            if (x.on[s] != y.on[s] || x.note[s] != y.note[s] || x.accent[s] != y.accent[s] || x.slide[s] != y.slide[s]) return false;
    }
    return true;
}

int main()
{
    static Bank a, b;
    a.initDefault();
    check(serializeBank(a) == "KBAS1\nG 1 0 0 70\n", "empty bank is short");
    applyCommand(a, "P 3 24 62");
    applyCommand(a, "s 3 5 1 45 1 0");
    applyCommand(a, "l 3 6 1");
    applyCommand(a, "n 3 6 50");
    applyCommand(a, "G 2 4 1");
    check(a.patterns[3].length == 24 && a.patterns[3].swing == 62, "P sets length and swing");
    check(a.patterns[3].on[5] && a.patterns[3].note[5] == 45 && a.patterns[3].accent[5] && !a.patterns[3].slide[5], "s sets a full step");
    check(a.patterns[3].slide[6] && a.patterns[3].note[6] == 50, "l / n set slide and note");
    check(applyCommand(a, "G 2 4 1") == kGlobTouched && a.glob.scale == 2 && a.glob.root == 4 && a.glob.transpose == 1, "G sets the key");
    check(applyCommand(a, "P 99 1 1") == -1 && applyCommand(a, "s 0 70 1 1 1 1") == -1 && applyCommand(a, "zz") == -1, "bad commands are refused");
    check(deserializeBank(b, serializeBank(a).c_str()) && same(a, b), "text round trip");
    applyCommand(a, "cp 3 7"); check(a.patterns[7].note[5] == 45 && a.patterns[7].length == 24, "cp copies");
    applyCommand(a, "rot 7 1"); check(a.patterns[7].note[6] == 45 && a.patterns[7].accent[6], "rot shifts right");
    applyCommand(a, "rot 7 -1"); check(a.patterns[7].note[5] == 45, "rot shifts back");
    applyCommand(a, "cl 7"); check(a.patterns[7].isDefault(), "cl clears");
    check(!deserializeBank(b, "KFX81\n"), "other formats are refused");
    applyCommand(a, "k 3 5 2 20"); applyCommand(a, "k 3 5 13 127"); applyCommand(a, "C 1 2 3:2 0:1");
    check(deserializeBank(b, serializeBank(a).c_str()) && b.patterns[3].locks[5][2] == 20 && b.patterns[3].locks[5][13] == 127 && b.chain.on && b.chain.n == 2, "locks and chain survive the text");
    applyCommand(a, "rot 3 1"); check(a.patterns[3].locks[6][2] == 20, "locks follow rotation");
    applyCommand(a, "kc 3 6"); check(!a.patterns[3].hasLocks(6), "kc clears a step's locks");
    check(applyCommand(a, "k 3 5 15 1") == -1, "only sound parameters can be locked");
    applyCommand(a, "R 3 5 7"); applyCommand(a, "y 3 2 40 3 1");
    check(deserializeBank(b, serializeBank(a).c_str()) && b.patterns[3].accLen == 5 && b.patterns[3].slideLen == 7 && b.patterns[3].prob[2] == 40 && b.patterns[3].ratchet[2] == 3 && b.patterns[3].tie[2] == 1,
          "row lengths, probability, ratchet and tie survive");

    // presets: build, round trip, notes inside the preset key
    bool allOk = true, inKey = true, haveNotes = true;
    for (int i = 0; i < kNumPresets; ++i) {
        float v[kTotalParams] = {}; Bank pb; buildPreset(i, v, pb);
        Bank r; if (!deserializeBank(r, serializeBank(pb).c_str()) || !same(pb, r)) allOk = false;
        if (i == 0) continue;
        bool any = false;
        for (int s = 0; s < pb.patterns[0].length; ++s) if (pb.patterns[0].on[s]) {
            any = true;
            if (!inScale(pb.patterns[0].note[s], pb.glob.root, pb.glob.scale)) { inKey = false; std::printf("  %s step %d %s out of key\n", kPresets[i].name, s + 1, noteName(pb.patterns[0].note[s]).c_str()); }
        }
        if (!any) haveNotes = false;
        for (int p = 0; p < kSavedParams; ++p) if (!(v[p] >= 0.f && v[p] <= 240.f)) allOk = false;
    }
    check(allOk, "presets round trip, values in range");
    check(inKey, "preset notes in the preset key");
    check(haveNotes, "every preset plays notes");
    check(kPresets[12].build && [] { float v[kTotalParams]; Bank pb; buildPreset(12, v, pb); return pb.patterns[0].length == 32; }(), "Long Line is 32 steps");

    // ACID GEN
    bool gIn = true, gVar = false;
    for (int sc = 1; sc < kScales; ++sc) for (int root = 0; root < 12; root += 5) for (uint32_t seed = 1; seed < 20; ++seed) {
        Pattern p; acidGen(p, sc, root, seed * 7919u);
        int ons = 0;
        for (int s = 0; s < p.length; ++s) { if (!inScale(p.note[s], root, sc)) gIn = false; ons += p.on[s]; if (p.slide[s] && !p.on[s]) gIn = false; }
        if (ons > 4 && ons < p.length) gVar = true;
    }
    check(gIn, "ACID GEN stays in the scale");
    check(gVar, "ACID GEN makes lines with rests");
    Pattern x, y; acidGen(x, 1, 0, 5); acidGen(y, 1, 0, 6);
    check(std::memcmp(x.note, y.note, sizeof(x.note)) != 0, "different seeds, different lines");
    // 0.4: DENSITY
    {
        Pattern p; p.length = 16; GenLine g;
        acidGenDense(p, g, 0, 1, 0, 1234, 50);
        int n50 = 0; for (int s = 0; s < 16; ++s) n50 += p.on[s];
        Pattern q = p; applyDensity(q, g, 80);
        int n80 = 0; bool subset = true; for (int s = 0; s < 16; ++s) { n80 += q.on[s]; if (p.on[s] && !q.on[s]) subset = false; }
        check(n50 == 8 && n80 == 13 && subset, "DENSITY: 50 % = 8 of 16 steps, 80 % = 13, the 50 % steps are kept");
        bool accOk = true; for (int s = 0; s < 16; ++s) if ((p.accent[s] || p.slide[s]) && !p.on[s]) accOk = false;
        check(accOk, "DENSITY: accents and slides only on the steps that play");
        int down = 0, tries = 0;
        for (uint32_t sd = 1; sd < 200; ++sd) { Pattern r; r.length = 16; GenLine h; acidGenDense(r, h, 0, 1, 0, sd, 25); for (int s = 0; s < 16; s += 4) down += r.on[s]; tries += 4; }
        std::printf("     downbeats %.2f\n", down / (double)tries); check(down > tries * 0.55, "DENSITY 25 %: the downbeats are mostly what is left");
        Pattern z = p; applyDensity(z, g, 0); check(z.empty(), "DENSITY 0 %: no notes");
        Bank d; d.initDefault(); applyCommand(d, "G 2 3 0 35"); Bank e; deserializeBank(e, serializeBank(d).c_str());
        check(e.glob.density == 35 && e.glob.scale == 2, "DENSITY saved with the bank");
        Bank o; deserializeBank(o, "KBAS1\nG 1 0 0\n"); check(o.glob.density == 70, "0.3 banks: DENSITY 70 %");
    }
    std::printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails ? 1 : 0;
}
