// Kinetik KBAS-1 — user interface (NanoVG)
#include "DistrhoUI.hpp"
#include "Bank.hpp"
#include "Presets.hpp"
#include "Skins.hpp"
#include "LogoMark.hpp"
#include "Fonts.hpp"
#include "License.hpp"
#include "Updater.hpp"
#include "FsUtf8.hpp"
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <cmath>
#include <string>
#include <memory>
#include <deque>
#include <chrono>

START_NAMESPACE_DISTRHO

using namespace kn;
using DGL_NAMESPACE::Color;

static constexpr float W = 1180.f, H = 720.f;
static constexpr int kFineSteps = 24;   // out_step is in 1/24 of a 16th

struct R {
    float x, y, w, h;
    bool in(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + h * 0.5f; }
};

enum DragKind { dkNone, dkKnob, dkTempo, dkPatLen, dkSwing, dkNote, dkPaint, dkResize };
enum Overlay { ovNone, ovPresets, ovSkins, ovSave, ovLicense, ovUpdate };

class KbasUI : public UI
{
public:
    KbasUI()
        : UI(W, H)
    {
        loadSharedResources();
        const double s = getScaleFactor();
        if (s > 1.01) setSize((uint)(W * s), (uint)(H * s));
        setGeometryConstraints((uint)(W * 0.6 * (s > 1.01 ? s : 1.0)), (uint)(H * 0.6 * (s > 1.01 ? s : 1.0)), true, false);
        for (int i = 0; i < kTotalParams; ++i) values[i] = kParamDefaults[i];
        bank.reset(new Bank());
        bank->initDefault();
        fonts[fnSerif] = createFontFromMemory("kn-serif", kFontSerif, sizeof(kFontSerif), false);
        fonts[fnSerifItalic] = createFontFromMemory("kn-serif-i", kFontSerifItalic, sizeof(kFontSerifItalic), false);
        fonts[fnMono] = createFontFromMemory("kn-mono", kFontMono, sizeof(kFontMono), false);
        fonts[fnBlack] = createFontFromMemory("kn-black", kFontBlack, sizeof(kFontBlack), false);
        fonts[fnLogo] = createFontFromMemory("kn-logo", kFontUnbounded, sizeof(kFontUnbounded), false);
        int sk, zm;
        readDefaultView(sk, zm);
        setSkin(sk, false);
        if (zm != 100) setSize((uint)std::lround(W * baseScale() * zm / 100.0), (uint)std::lround(H * baseScale() * zm / 100.0));
        viewDirty = false;
        licInfo = lic::findInstalled("KBAS-1");
        updater.reset(new upd::Checker("KBAS-1", KN_VERSION, "https://github.com/Kozzzmo/kinetik-kbas1/releases/latest/download/update.txt"));
        updater->check(false);
    }

    // ------------------------------------------------------------------ skins / view
    static std::string viewFile()
    {
       #ifdef _WIN32
        const std::string base = fs::env("APPDATA");
        return !base.empty() ? base + "\\KinetikKBAS1-view.txt" : std::string();
       #else
        const std::string base = fs::env("HOME");
       #ifdef __APPLE__
        return !base.empty() ? base + "/Library/Preferences/KinetikKBAS1-view.txt" : std::string();
       #else
        return !base.empty() ? base + "/.config/KinetikKBAS1-view.txt" : std::string();
       #endif
       #endif
    }
    static void readDefaultView(int& skinOut, int& zoomOut)
    {
        skinOut = kDefaultSkin; zoomOut = 100;
        const std::string f = viewFile();
        if (f.empty()) return;
        if (FILE* fp = fs::open(f, "r")) {
            int a = 0, b = 100; const int n = std::fscanf(fp, "%d %d", &a, &b); std::fclose(fp);
            if (n >= 1 && a >= 0 && a < kNumThemes) skinOut = a;
            if (n >= 2 && b >= 60 && b <= 300) zoomOut = b;
        }
    }
    static void writeDefaultView(int s, int z)
    {
        const std::string f = viewFile();
        if (f.empty()) return;
        if (FILE* fp = fs::open(f, "w")) { std::fprintf(fp, "%d %d\n", s, z); std::fclose(fp); }
    }
    void setSkin(int i, bool save) { skin = clampi(i, 0, kNumThemes - 1); th = &kThemes[skin]; if (save) saveView(); repaint(); }
    double baseScale() const { const double s = getScaleFactor(); return s > 0.1 ? s : 1.0; }
    int zoomPercent() const { return (int)std::lround(getWidth() / (W * baseScale()) * 100.0); }
    void setZoom(double z)
    {
        z = std::max(0.6, std::min(3.0, z));
        const uint w = (uint)std::lround(W * baseScale() * z), h = (uint)std::lround(H * baseScale() * z);
        if (std::abs((int)w - (int)getWidth()) > 1) setSize(w, h);
        viewDirty = true; repaint();
    }
    void saveView()
    {
        char b[32]; std::snprintf(b, sizeof(b), "%d %d", skin, zoomPercent());
        setState("skin", b);
        writeDefaultView(skin, zoomPercent());
        viewDirty = false;
    }

protected:
    // ------------------------------------------------------------------ DSP -> UI
    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= (uint32_t)kTotalParams || values[index] == value) return;
        values[index] = value;
        repaint();
    }
    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "bank") == 0) { deserializeBank(*bank, value); repaint(); }
        if (std::strcmp(key, "skin") == 0 && value && *value) {
            int sk = kDefaultSkin, zm = 0;
            const int n = std::sscanf(value, "%d %d", &sk, &zm);
            if (n >= 1) setSkin(sk, false);
            if (n >= 2 && zm >= 60 && zm <= 300 && std::abs(zm - zoomPercent()) > 1) setZoom(zm / 100.0);
            viewDirty = false;
        }
    }
    void onResize(const ResizeEvent& ev) override { UI::onResize(ev); viewDirty = true; viewDirtyTicks = 0; }

    // ------------------------------------------------------------------ drawing primitives (same as the other Kinetik plug-ins)
    void useFont(int f)
    {
        if (f >= fnSerif && f < kFontCount) fontFaceId(fonts[f]); else fontFace(NANOVG_DEJAVU_SANS_TTF);
    }
    void box(const R& r, const Color& c, float rad = -1.f) { beginPath(); roundedRect(r.x, r.y, r.w, r.h, rad < 0 ? th->radius : rad); fillColor(c); fill(); }
    void frame(const R& r, const Color& c, float rad = -1.f, float wdt = 1.f)
    {
        beginPath(); roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, rad < 0 ? th->radius : rad); strokeColor(c); strokeWidth(wdt); stroke();
    }
    static float textSize(float size, int font) { const float s = std::max(size, 9.5f); return font == fnMono ? s * 1.13f * 1.08f : s * 1.08f; }
    void label(float x, float y, const char* s, float size, const Color& c, int align = ALIGN_CENTER | ALIGN_MIDDLE, int font = -1)
    {
        if (s == nullptr || *s == '\0') return;
        const int f = font < 0 ? th->fontLabel : font;
        useFont(f); fontSize(textSize(size, f));
        textLetterSpacing(f == fnMono ? th->labelSpacing * (size / 12.f) : 0.f);
        fillColor(c); textAlign(align); text(x, y, s, nullptr);
        textLetterSpacing(0.f);
    }
    void number(float x, float y, const char* s, float size, const Color& c, int align = ALIGN_CENTER | ALIGN_MIDDLE, int font = -1)
    {
        if (s == nullptr || *s == '\0') return;
        const int f = font < 0 ? th->fontNum : font;
        useFont(f);
        float sz = f == fnSerif || f == fnSerifItalic ? size * 1.25f : (f == fnBlack ? size * 0.95f : size);
        if (size <= 16.f) sz *= 1.15f;
        fontSize(sz); textAlign(align);
        if (th->glow) { fontBlur(sz * 0.18f); fillColor(Color(c, Color(0, 0, 0, 0.f), 0.35f)); text(x, y, s, nullptr); fontBlur(0.f); }
        fillColor(c); text(x, y, s, nullptr);
    }
    enum Icon { icPlay, icStop };
    void iconLabel(float cx, float cy, Icon ic, const char* s, float size, const Color& c)
    {
        const int f = th->fontButton;
        useFont(f); fontSize(textSize(size, f));
        textLetterSpacing(f == fnMono ? th->labelSpacing * (size / 12.f) : 0.f);
        DGL_NAMESPACE::Rectangle<float> bnd; textBounds(0, 0, s, nullptr, bnd);
        textLetterSpacing(0.f);
        const float isz = size * 0.62f, gap = size * 0.55f, total = isz + gap + bnd.getWidth();
        const float x = cx - total * 0.5f;
        beginPath();
        if (ic == icPlay) { moveTo(x, cy - isz * 0.55f); lineTo(x + isz * 0.9f, cy); lineTo(x, cy + isz * 0.55f); closePath(); }
        else rect(x, cy - isz * 0.45f, isz * 0.9f, isz * 0.9f);
        fillColor(c); fill();
        label(x + isz + gap, cy, s, size, c, ALIGN_LEFT | ALIGN_MIDDLE, f);
    }
    void panelBox(const R& r) { box(r, th->panel, th->radius + 2.f); frame(r, th->panelEdge, th->radius + 2.f, th->panelEdgeWidth); }
    void button(const R& r, const char* s, bool on, float size = 11.5f)
    {
        if (on) box(r, th->selFill); else { box(r, th->btnFill); frame(r, th->btnEdge, -1.f, 1.f); }
        label(r.cx(), r.cy() + 1, s, size, on ? th->selText : th->btnText, ALIGN_CENTER | ALIGN_MIDDLE, th->fontButton);
    }
    void overlayBox(const R& r)
    {
        beginPath(); rect(0, 0, W, H); fillColor(th->overlay); fill();
        box(r, th->panel, th->radius + 3.f); frame(r, th->panelEdge, th->radius + 3.f, th->panelEdgeWidth + 0.5f);
    }
    Color accentInk() const { return th->logo == lgRiso ? th->text : th->accent; }
    Color onColor() const { return th->logo == lgNeon ? Color(236, 36, 50) : th->accent; }   // lit steps (NEON red, as KDSP-16)
    void drawKnob(const R& r, float v01, const char* name, const std::string& val, bool locked, bool highlight)
    {
        const float cx = r.cx(), cy = r.y + 44.f, rad = 26.f;
        const float a0 = kPi * 0.75f, a1 = kPi * 2.25f;
        v01 = clampf(v01, 0.f, 1.f);
        const Color hi = locked ? th->lock : th->knobArc;
        const float a = a0 + (a1 - a0) * v01;
        switch (th->knob) {
        case ksTicks: {
            for (int i = 0; i < 25; ++i) {
                const float t = i / 24.f, ang = a0 + (a1 - a0) * t; const bool lit = t <= v01 + 1e-4f;
                beginPath(); circle(cx + std::cos(ang) * (rad + 3), cy + std::sin(ang) * (rad + 3), lit ? 1.6f : 1.1f); fillColor(lit ? hi : th->knobTrack); fill();
            }
            if (th->glow) { beginPath(); circle(cx, cy, rad - 3); fillPaint(radialGradient(cx, cy, rad - 8, rad + 8, Color(hi, Color(0, 0, 0, 0.f), 0.75f), Color(0, 0, 0, 0.f))); fill(); }
            beginPath(); circle(cx, cy, rad - 5); fillColor(th->knobBody); fill();
            strokeColor(locked ? th->lock : th->knobRing); strokeWidth(locked ? 2.f : 1.2f); stroke();
            beginPath(); moveTo(cx + std::cos(a) * 4, cy + std::sin(a) * 4); lineTo(cx + std::cos(a) * (rad - 9), cy + std::sin(a) * (rad - 9));
            strokeColor(locked ? th->lock : th->knobPointer); strokeWidth(2.4f); lineCap(ROUND); stroke();
            break;
        }
        case ksArcThick: {
            beginPath(); arc(cx, cy, rad, a0, a1, CW); strokeColor(th->knobTrack); strokeWidth(6.f); lineCap(BUTT); stroke();
            beginPath(); arc(cx, cy, rad, a0, a0 + (a1 - a0) * std::max(0.001f, v01), CW); strokeColor(hi); strokeWidth(6.f); stroke();
            beginPath(); circle(cx, cy, rad - 6); fillColor(locked ? th->lock : th->knobBody); fill();
            strokeColor(th->knobRing); strokeWidth(1.2f); stroke();
            beginPath(); moveTo(cx + std::cos(a) * 6, cy + std::sin(a) * 6); lineTo(cx + std::cos(a) * (rad - 9), cy + std::sin(a) * (rad - 9));
            strokeColor(th->knobPointer); strokeWidth(2.4f); lineCap(ROUND); stroke();
            break;
        }
        default: {
            beginPath(); circle(cx, cy, rad + 4); fillColor(th->inner); fill();
            beginPath(); arc(cx, cy, rad, a0, a1, CW); strokeColor(th->knobTrack); strokeWidth(5.f); stroke();
            beginPath(); arc(cx, cy, rad, a0, a0 + (a1 - a0) * std::max(0.001f, v01), CW); strokeColor(hi); strokeWidth(5.f); stroke();
            beginPath(); circle(cx, cy, rad - 7); fillColor(highlight ? Color(th->knobBody, Color(255, 255, 255), 0.1f) : th->knobBody); fill();
            beginPath(); moveTo(cx + std::cos(a) * (rad - 18), cy + std::sin(a) * (rad - 18)); lineTo(cx + std::cos(a) * (rad - 8), cy + std::sin(a) * (rad - 8));
            strokeColor(th->knobPointer); strokeWidth(3.f); stroke();
            break;
        }
        }
        label(cx, r.y + 88.f, name, 12.f, locked ? th->lock : th->text);
        number(cx, r.y + 106.f, val.c_str(), 14.f, locked ? th->lock : (th->fontNum == fnSans ? th->dim : th->text));
    }
    void drawLogo()
    {
        drawOrbitMark(*this, 14.f, 7.f, 46.f, th->logo);
        const char* nm = "KBAS-1";
        useFont(fnLogo); fontSize(30.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        const float nx = 68.f, ny = 23.f;
        if (th->glow) { fontBlur(10.f); fillColor(Color(th->accent, Color(0, 0, 0, 0.f), 0.45f)); text(nx, ny, nm, nullptr); fontBlur(0.f); }
        fillColor(th->glow ? Color(232, 236, 255) : th->text); text(nx, ny, nm, nullptr);
        const int ust = updater ? (int)updater->getState() : -1;
        if (ust == upd::Checker::stAvailable || ust == upd::Checker::stInstalling || ust == upd::Checker::stInstalled) {
            const std::string v = updater->manifest().version;
            const std::string t = ust == upd::Checker::stAvailable ? "UPDATE " + v + " AVAILABLE - CLICK"
                                : ust == upd::Checker::stInstalling ? "INSTALLING UPDATE " + v + "..." : "UPDATED TO " + v + " - REOPEN TO USE IT";
            label(nx + 1, 45.f, t.c_str(), 9.5f, accentInk(), ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        } else if (licInfo.valid || !lic::kDemoModeEnabled) label(nx + 1, 45.f, "KINETIK  ACID BASS PROGRAM", 9.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
        else label(nx + 1, 45.f, "DEMO - CLICK TO REGISTER", 9.5f, accentInk(), ALIGN_LEFT | ALIGN_MIDDLE, fnMono);
    }

    // ------------------------------------------------------------------ helpers
    float gp(int p) const { return values[p]; }
    int editPattern() const { return clampi((int)std::lround(gp(pPATTERN)), 0, kPatterns - 1); }
    Pattern& pat() { return bank->patterns[editPattern()]; }
    int64_t fineStep() const { return (int64_t)std::lround(gp(pOUT_STEP)); }
    bool playingThis() const { return gp(pOUT_STEP) >= 0.f && (int)std::lround(gp(pOUT_PATTERN)) == editPattern(); }
    int stepNow() { if (!playingThis()) return -1; const int64_t k = fineStep() / kFineSteps; return (int)(k % std::max<int>(1, pat().length)); }

    void setParam(uint32_t idx, float v)
    {
        if (values[idx] != v && idx != (uint32_t)pPLAY && idx != (uint32_t)pPATTERN) autoSnap("sound", 1.2);
        values[idx] = v;
        setParameterValue(idx, v);
        char b[40]; std::snprintf(b, sizeof(b), "pv %u %.4f\n", (unsigned)idx, v); pvQueue += b;
    }
    std::string pvQueue;
    void flushPv() { if (pvQueue.empty()) return; pvQueue.pop_back(); setState("cmd", pvQueue.c_str()); pvQueue.clear(); }
    void setParamGesture(uint32_t idx, float v) { editParameter(idx, true); setParam(idx, v); editParameter(idx, false); }
    void sendCmd(const std::string& c)
    {
        const bool live = c.rfind("key ", 0) == 0;
        if (!live) { autoSnap("edit", 0.5); applyCommand(*bank, c.c_str()); }
        setState("cmd", c.c_str());
    }
    void sendLines(const std::string& c)
    {
        autoSnap("edit", 0.5);
        size_t st = 0;
        while (st < c.size()) { size_t e = c.find('\n', st); if (e == std::string::npos) e = c.size(); applyCommand(*bank, c.substr(st, e - st).c_str()); st = e + 1; }
        setState("cmd", c.c_str());
    }
    void sendPatternSettings() { char b[48]; std::snprintf(b, sizeof(b), "P %d %d %d", editPattern(), pat().length, pat().swing); sendCmd(b); }
    void sendGlob() { char b[48]; std::snprintf(b, sizeof(b), "G %d %d %d", bank->glob.scale, bank->glob.root, bank->glob.transpose); sendCmd(b); }
    void sendStep(int s)
    {
        const Pattern& p = pat();
        char b[64]; std::snprintf(b, sizeof(b), "s %d %d %d %d %d %d", editPattern(), s, p.on[s], p.note[s], p.accent[s], p.slide[s]); sendCmd(b);
    }
    void audition(int note, bool on) { char b[32]; std::snprintf(b, sizeof(b), "key %d %d", note, on ? 100 : 0); sendCmd(b); }

    // ------------------------------------------------------------------ layout
    R logoArea() const { return {12.f, 4.f, 372.f, 50.f}; }
    R patLenBox() const { return {536.f, 9.f, 76.f, 40.f}; }
    R swingBox() const { return {618.f, 9.f, 76.f, 40.f}; }
    R patBox() const { return {700.f, 9.f, 130.f, 40.f}; }
    R playBtn() const { return {840.f, 9.f, 84.f, 40.f}; }
    R tempoBox() const { return {932.f, 9.f, 110.f, 40.f}; }
    R skinBtn() const { return {1050.f, 9.f, 116.f, 40.f}; }
    R lcdR() const { return {24.f, 66.f, 280.f, 160.f}; }
    R vKnob(int k) const { return {316.f + k * 105.f, 82.f, 100.f, 120.f}; }        // voice: 8 knobs
    R fKnob(int k) const { return {22.f + k * 98.f, 256.f, 96.f, 120.f}; }          // amp + effects: 7 knobs
    R patBtn(int i) const { return {734.f + (i % 8) * 53.f, 274.f + (i / 8) * 36.f, 48.f, 30.f}; }
    R patCopy() const { return {734.f, 348.f, 136.f, 28.f}; }
    R patPaste() const { return {876.f, 348.f, 136.f, 28.f}; }
    R patClear() const { return {1018.f, 348.f, 136.f, 28.f}; }
    // sequencer
    R pageBtn(int i) const { return {24.f + i * 68.f, 406.f, 64.f, 26.f}; }
    R rotL() const { return {166.f, 406.f, 28.f, 26.f}; }
    R rotR() const { return {196.f, 406.f, 28.f, 26.f}; }
    R acidBtn() const { return {232.f, 406.f, 104.f, 26.f}; }
    R scaleBtn() const { return {342.f, 406.f, 150.f, 26.f}; }
    R rootBtn() const { return {496.f, 406.f, 64.f, 26.f}; }
    R snapBtn() const { return {564.f, 406.f, 70.f, 26.f}; }
    R transBtn() const { return {638.f, 406.f, 124.f, 26.f}; }
    R undoBtn() const { return {770.f, 406.f, 116.f, 26.f}; }
    R presetBtn() const { return {892.f, 406.f, 130.f, 26.f}; }
    R saveBtn() const { return {1028.f, 406.f, 130.f, 26.f}; }
    float colX(int i) const { return 110.f + i * 66.f; }
    R trigCell(int i) const { return {colX(i), 442.f, 62.f, 36.f}; }
    R noteCell(int i) const { return {colX(i), 484.f, 62.f, 116.f}; }
    R accCell(int i) const { return {colX(i), 606.f, 62.f, 30.f}; }
    R slideCell(int i) const { return {colX(i), 642.f, 62.f, 30.f}; }
    R gripR() const { return {W - 22.f, H - 22.f, 22.f, 22.f}; }
    R viewBox() const { return {240.f, 96.f, 700.f, 520.f}; }
    R skinRow(int i) const { return {264.f + (i % 2) * 330.f, 150.f + (i / 2) * 148.f, 316.f, 136.f}; }
    R zoomBtn(int i) const { return {264.f + i * 109.f, 486.f, 101.f, 36.f}; }
    R presetsBox() const { return {80.f, 60.f, 1020.f, 600.f}; }
    R presetItem(int col, int row) const { return {100.f + col * 500.f, 130.f + row * 30.f, 480.f, 27.f}; }
    R presetsClose() const { return {980.f, 606.f, 100.f, 36.f}; }
    R saveBox() const { return {290.f, 200.f, 600.f, 270.f}; }
    R saveField() const { return {320.f, 290.f, 540.f, 52.f}; }
    R saveOk() const { return {620.f, 400.f, 240.f, 40.f}; }
    R saveCancel() const { return {320.f, 400.f, 140.f, 40.f}; }
    R licBox() const { return {290.f, 170.f, 600.f, 330.f}; }
    R licLoad() const { return {320.f, 420.f, 190.f, 40.f}; }
    R licPaste() const { return {520.f, 420.f, 190.f, 40.f}; }
    R licClose() const { return {720.f, 420.f, 140.f, 40.f}; }
    R updBox() const { return {290.f, 150.f, 600.f, 380.f}; }
    R updAuto() const { return {320.f, 400.f, 170.f, 34.f}; }
    R updAutoInst() const { return {496.f, 400.f, 190.f, 34.f}; }
    R updLic() const { return {692.f, 400.f, 168.f, 34.f}; }
    R updMain() const { return {320.f, 456.f, 250.f, 40.f}; }
    R updSkip() const { return {580.f, 456.f, 150.f, 40.f}; }
    R updClose() const { return {740.f, 456.f, 120.f, 40.f}; }
    static constexpr int kVoice[8] = {pTUNE, pWAVE, pCUTOFF, pRESO, pENVMOD, pDECAY, pACCENT, pSLIDE};
    static constexpr int kFx[7] = {pAMPDEC, pDRIVE, pTONE, pDLYTIME, pDLYFB, pDLYMIX, pVOLUME};

    // ------------------------------------------------------------------ main draw
    void onNanoDisplay() override
    {
        sx = getWidth() / W;
        scale(sx, sx);
        if (!haloImage.isValid()) makeTextures();
        beginPath(); rect(0, 0, W, H); fillColor(th->bg); fill();
        if (th->halftone && haloImage.isValid()) {
            Paint p = imagePattern(0, 0, 6.f, 6.f, 0.f, haloImage, 1.f); p.innerColor = th->dots; p.outerColor = th->dots;
            beginPath(); rect(0, 0, W, H); fillPaint(p); fill();
        }
        drawHeader();
        drawVoice();
        drawFxPatterns();
        drawSequencer();
        if (overlay == ovPresets) drawPresets();
        if (overlay == ovSkins) drawSkins();
        if (overlay == ovSave) drawSave();
        if (overlay == ovLicense) drawLicense();
        if (overlay == ovUpdate) drawUpdate();
        if (tipShown && overlay == ovNone) drawTip();
        drawGrip();
    }
    void makeTextures()
    {
        uint8_t h[6 * 6 * 4] = {};
        for (int i : {7, 8, 13, 14}) { h[i * 4] = 255; h[i * 4 + 1] = 255; h[i * 4 + 2] = 255; h[i * 4 + 3] = 140; }
        haloImage = createImageFromRGBA(6, 6, h, IMAGE_REPEAT_X | IMAGE_REPEAT_Y | (1 << 5));
    }
    void lcdCell(const R& r, const char* top, const char* val)
    {
        box(r, th->lcdBg); frame(r, th->panelEdge, -1.f, th->panelEdgeWidth);
        label(r.cx(), r.y + 11, top, 9.f, th->lcdDim);
        number(r.cx(), r.y + 28, val, 16.f, th->lcdText);
    }
    void drawHeader()
    {
        drawLogo();
        char b[48];
        std::snprintf(b, sizeof(b), "%d", pat().length); lcdCell(patLenBox(), "LENGTH", b);
        std::snprintf(b, sizeof(b), "%d%%", pat().swing); lcdCell(swingBox(), "SWING", b);
        const int ep = editPattern(), pp = (int)std::lround(gp(pOUT_PATTERN));
        const bool running = gp(pOUT_STEP) >= 0.f;
        const R pr = patBox();
        box(pr, th->lcdBg); frame(pr, th->panelEdge, -1.f, th->panelEdgeWidth);
        label(pr.x + 10, pr.cy() + 1, "PAT", 10.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);
        if (running && pp != ep) std::snprintf(b, sizeof(b), "A%02d>A%02d", pp + 1, ep + 1); else std::snprintf(b, sizeof(b), "A%02d", ep + 1);
        number(pr.x + pr.w - 10, pr.cy() + 1, b, running && pp != ep ? 16.f : 20.f, th->lcdText, ALIGN_RIGHT | ALIGN_MIDDLE);
        const bool play = gp(pPLAY) > 0.5f;
        const R pb = playBtn();
        if (play) box(pb, th->selFill); else { box(pb, th->btnFill); frame(pb, th->btnEdge); }
        iconLabel(pb.cx(), pb.cy() + 1, play ? icStop : icPlay, play ? "STOP" : "PLAY", 12.f, play ? th->selText : th->btnText);
        const R tb = tempoBox();
        box(tb, th->lcdBg); frame(tb, th->panelEdge, -1.f, th->panelEdgeWidth);
        std::snprintf(b, sizeof(b), "%.1f", gp(pTEMPO));
        number(tb.x + 70, tb.cy() + 1, b, 20.f, th->lcdText, ALIGN_RIGHT | ALIGN_MIDDLE);
        label(tb.x + 75, tb.cy() + 2, "BPM", 10.f, th->lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);
        const R sk = skinBtn();
        box(sk, th->btnFill); frame(sk, th->btnEdge);
        label(sk.cx(), sk.y + 12, "SKIN / SIZE", 9.f, th->dim);
        std::snprintf(b, sizeof(b), "%s %d%%", th->name, zoomPercent());
        label(sk.cx(), sk.y + 28, b, 10.5f, th->text);
    }

    // voice: LCD (filter curve + the playing note) and the 8 voice knobs
    void drawVoice()
    {
        panelBox({14.f, 58.f, 1152.f, 176.f});
        const R lc = lcdR();
        box(lc, th->lcdBg); frame(lc, th->panelEdge, -1.f, th->panelEdgeWidth);
        const bool gate = gp(pOUT_GATE) > 0.5f;
        number(lc.x + 12, lc.y + 22, gate ? noteName((int)gp(pOUT_NOTE)).c_str() : "-", 20.f, th->lcdText, ALIGN_LEFT | ALIGN_MIDDLE);
        beginPath(); circle(lc.x + lc.w - 16, lc.y + 18, 5.f); fillColor(gate ? (th->logo == lgNeon ? Color(125, 255, 90) : th->accent) : th->lcdDim); fill();
        label(lc.x + lc.w - 28, lc.y + 18, presetNameStr.c_str(), 9.5f, th->lcdDim, ALIGN_RIGHT | ALIGN_MIDDLE);
        // filter response: cutoff + resonance peak, and the envelope's reach (ENV MOD) as a shaded band
        const R g{lc.x + 12, lc.y + 44, lc.w - 24, lc.h - 56};
        const float fc = gp(pCUTOFF) / 127.f, res = gp(pRESO) / 127.f, env = gp(pENVMOD) / 127.f * 5.5f / std::log2(400.f);
        auto curve = [&](float c, const Color& col, float wdt) {
            beginPath();
            for (int i = 0; i <= 90; ++i) {
                const float x = i / 90.f, d = (x - c) * 9.f;
                float mag = 1.f / std::sqrt(1.f + std::pow(std::exp(d), 8.f));
                mag *= 1.f + res * res * 3.f * std::exp(-d * d * 3.f);
                const float y = g.y + g.h - (g.h - 4) * clampf(mag * 0.55f, 0.f, 1.f);
                if (i == 0) moveTo(g.x + g.w * x, y); else lineTo(g.x + g.w * x, y);
            }
            strokeColor(col); strokeWidth(wdt); stroke();
        };
        beginPath(); rect(g.x + g.w * fc, g.y, std::max(1.f, g.w * std::min(env, 1.f - fc)), g.h); fillColor(withAlpha(th->lcdText, 0.10f)); fill();
        curve(std::min(1.f, fc + env), withAlpha(th->lcdDim, 0.8f), 1.2f);
        curve(fc, th->lcdText, 2.f);
        for (int k = 0; k < 8; ++k)
            drawKnob(vKnob(k), gp(kVoice[k]) / 127.f, kParamNames[kVoice[k]], paramText(kVoice[k], gp(kVoice[k])), false, drag == dkKnob && dragIndex == kVoice[k]);
    }
    void drawFxPatterns()
    {
        panelBox({14.f, 242.f, 700.f, 148.f});
        for (int k = 0; k < 7; ++k)
            drawKnob(fKnob(k), gp(kFx[k]) / 127.f, kParamNames[kFx[k]], paramText(kFx[k], gp(kFx[k])), false, drag == dkKnob && dragIndex == kFx[k]);
        panelBox({722.f, 242.f, 444.f, 148.f});
        label(736, 258, "PATTERNS", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        if (!patMsg.empty()) label(1154, 258, patMsg.c_str(), 9.5f, th->lock, ALIGN_RIGHT | ALIGN_MIDDLE);
        const int ep = editPattern(), pp = (int)std::lround(gp(pOUT_PATTERN));
        const bool running = gp(pOUT_STEP) >= 0.f;
        for (int i = 0; i < kPatterns; ++i) {
            char b[8]; std::snprintf(b, sizeof(b), "A%02d", i + 1);
            const R r = patBtn(i);
            if (i == ep) box(r, th->selFill); else { box(r, th->btnFill); frame(r, th->btnEdge); }
            number(r.cx(), r.cy() + 1, b, 13.f, i == ep ? th->selText : (bank->patterns[i].empty() ? th->faint : th->text));
            if (running && i == pp && pp != ep) frame(r, th->accent, -1.f, 2.f);
        }
        button(patCopy(), "COPY", false, 10.f); button(patPaste(), "PASTE", false, 10.f); button(patClear(), "CLEAR", false, 10.f);
    }

    void drawSequencer()
    {
        panelBox({14.f, 398.f, 1152.f, 310.f});
        Pattern& p = pat();
        char b[48];
        for (int i = 0; i < 2; ++i) {
            std::snprintf(b, sizeof(b), "%d-%d", i * 16 + 1, i * 16 + 16);
            const R r = pageBtn(i);
            if (stepPg == i) button(r, b, true, 10.f);
            else { box(r, th->btnFill); frame(r, th->btnEdge); label(r.cx(), r.cy() + 1, b, 10.f, p.length > i * 16 ? th->btnText : th->faint); }
        }
        button(rotL(), "<", false, 11.f); button(rotR(), ">", false, 11.f);
        box(acidBtn(), th->btnFill); frame(acidBtn(), accentInk(), -1.f, 1.5f); label(acidBtn().cx(), acidBtn().cy() + 1, "ACID GEN", 10.5f, accentInk());
        button(scaleBtn(), kScaleNames[bank->glob.scale % kScales], false, 10.f);
        static const char* roots[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        std::snprintf(b, sizeof(b), "ROOT %s", roots[bank->glob.root % 12]); button(rootBtn(), b, false, 10.f);
        button(snapBtn(), "SNAP", snap, 10.f);
        button(transBtn(), "KEY TRANSPOSE", bank->glob.transpose != 0, 10.f);
        if (!undoStack.empty()) button(undoBtn(), "UNDO", false, 10.5f);
        else { box(undoBtn(), th->btnFill); frame(undoBtn(), Color(th->btnEdge, th->bg, 0.5f)); label(undoBtn().cx(), undoBtn().cy() + 1, "UNDO", 10.5f, th->faint); }
        box(presetBtn(), th->btnFill); frame(presetBtn(), th->text, -1.f, 1.5f); label(presetBtn().cx(), presetBtn().cy() + 1, "PRESETS", 10.5f, th->text);
        button(saveBtn(), "SAVE AS PRESET", false, 10.f);
        // row names
        label(28, trigCell(0).cy(), "STEP", 10.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        label(28, noteCell(0).cy(), "NOTE", 10.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        label(28, accCell(0).cy(), "ACCENT", 10.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        label(28, slideCell(0).cy(), "SLIDE", 10.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        const int now = stepNow();
        // pitch range shown in the note cells: two octaves and a half around the pattern
        int lo = 127, hi = 0;
        for (int s = 0; s < p.length; ++s) if (p.on[s]) { lo = std::min<int>(lo, p.note[s]); hi = std::max<int>(hi, p.note[s]); }
        if (lo > hi) { lo = 36; hi = 48; }
        const int span = std::max(24, hi - lo + 4), base = std::max(0, (lo + hi) / 2 - span / 2);
        for (int i = 0; i < 16; ++i) {
            const int s = stepPg * 16 + i;
            const bool inLen = s < p.length, on = p.on[s] != 0;
            const R t = trigCell(i), n = noteCell(i), a = accCell(i), sl = slideCell(i);
            const Color cell = inLen ? ((i / 4) % 2 ? th->stepAlt : th->stepFill) : Color(th->stepOff, th->bg, 0.4f);
            box(t, cell, 3.f); box(n, cell, 3.f); box(a, cell, 3.f); box(sl, cell, 3.f);
            if (!inLen) continue;
            // trig
            if (on) {
                const Color lc = onColor();
                if (th->glow) { beginPath(); rect(t.x - 3, t.y - 3, t.w + 6, t.h + 6); fillPaint(boxGradient(t.x, t.y, t.w, t.h, 3, 8, Color(lc, Color(0, 0, 0, 0.f), 0.45f), Color(0, 0, 0, 0.f))); fill(); }
                box({t.x + 3, t.y + 3, t.w - 6, 8}, lc, 2.f);
            } else box({t.x + 3, t.y + 3, t.w - 6, 8}, th->ledOff, 2.f);
            std::snprintf(b, sizeof(b), "%d", s + 1);
            number(t.cx(), t.y + 24, b, 12.f, on ? th->stepText : th->faint);
            if (s % 4 == 0) box({t.cx() - 7.f, t.y + 32.f, 14.f, 1.f}, on ? th->stepText : th->faint);
            if (s == now) frame(t, th->text, 3.f, 2.f);
            // note: name + a marker at its pitch; arrows = octave up / down
            const float py = n.y + n.h - 22 - (n.h - 44) * clampf((p.note[s] - base) / (float)span, 0.f, 1.f);
            const Color ink = on ? th->stepText : th->faint;
            beginPath(); rect(n.x + 8, py - 2, n.w - 16, 4); fillColor(on ? onColor() : withAlpha(th->faint, 0.6f)); fill();
            number(n.cx(), py - 11, noteName(p.note[s]).c_str(), 11.f, ink);
            label(n.cx(), n.y + 9, "+", 10.f, withAlpha(ink, 0.7f));
            label(n.cx(), n.y + n.h - 9, "-", 10.f, withAlpha(ink, 0.7f));
            if (snap && !inScale(p.note[s], bank->glob.root, bank->glob.scale)) { beginPath(); circle(n.x + n.w - 8, n.y + 9, 2.5f); fillColor(th->lock); fill(); }   // outside the scale
            // accent / slide
            if (p.accent[s]) { box({a.x + 4, a.y + 4, a.w - 8, a.h - 8}, th->accent, 2.f); label(a.cx(), a.cy() + 1, "ACC", 9.f, th->accentText); }
            if (p.slide[s]) {
                box({sl.x + 4, sl.y + 4, sl.w - 8, sl.h - 8}, th->lock, 2.f);
                // the glide goes into the next step
                beginPath(); moveTo(sl.x + sl.w - 6, sl.cy()); lineTo(sl.x + sl.w + 8, sl.cy()); strokeColor(th->lock); strokeWidth(2.f); stroke();
                label(sl.cx(), sl.cy() + 1, "SLIDE", 8.5f, th->selText);
            }
        }
    }
    static Color withAlpha(Color c, float a) { c.alpha *= a; return c; }

    // ACID GEN: a new line for the edited pattern in the chosen scale and root
    void acidGenerate()
    {
        rndSeed = (rndSeed * 1664525u + 1013904223u) ^ (uint32_t)(nowSec() * 1000.0);
        std::unique_ptr<Bank> nb(new Bank(*bank));
        acidGen(nb->patterns[editPattern()], bank->glob.scale, bank->glob.root, rndSeed);
        snapshot("acid gen");
        applyState(values, nb.get());
        presetNameStr = "(acid gen)";
    }
    // ------------------------------------------------------------------ presets (factory + user files)
    static std::string userDir()
    {
       #ifdef _WIN32
        const std::string base = fs::env("APPDATA");
        if (base.empty()) return std::string();
        std::string d = base + "\\Kinetik"; fs::makeDir(d); d += "\\KBAS-1"; fs::makeDir(d); d += "\\Presets"; fs::makeDir(d);
        return d + "\\";
       #else
        const std::string base = fs::env("HOME");
        if (base.empty()) return std::string();
       #ifdef __APPLE__
        std::string d = base + "/Library/Application Support";
       #else
        std::string d = base + "/.config";
       #endif
        fs::makeDir(d); d += "/Kinetik"; fs::makeDir(d); d += "/KBAS-1"; fs::makeDir(d); d += "/Presets"; fs::makeDir(d);
        return d + "/";
       #endif
    }
    void scanUser()
    {
        userPresets.clear();
        const std::string d = userDir();
        if (d.empty()) return;
        for (const std::string& f : fs::list(d, ".kbas")) userPresets.push_back(f.substr(0, f.size() - 5));
        std::sort(userPresets.begin(), userPresets.end());
    }
    void drawPresets()
    {
        const R bx = presetsBox();
        overlayBox(bx);
        number(100, 92, "Presets", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(100, 120, "FACTORY", 10.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        label(600, 120, "YOURS", 10.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        for (int i = 0; i < kNumPresets && i < 15; ++i) {
            const R r = presetItem(0, i);
            const bool sel = presetNameStr == kPresets[i].name;
            if (sel) box(r, th->selFill); else box(r, th->btnFill);
            number(r.x + 10, r.cy() + 1, kPresets[i].name, 13.f, sel ? th->selText : th->text, ALIGN_LEFT | ALIGN_MIDDLE);
            label(r.x + r.w - 10, r.cy() + 1, kPresets[i].desc, 8.5f, sel ? th->selText : th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        }
        if (userPresets.empty()) label(600, 145, "none yet: SAVE AS PRESET stores the sound and the 16 patterns", 9.5f, th->faint, ALIGN_LEFT | ALIGN_MIDDLE);
        for (int i = 0; i < (int)userPresets.size() && i < 15; ++i) {
            const R r = presetItem(1, i);
            const bool sel = presetNameStr == userPresets[i];
            if (sel) box(r, th->selFill); else box(r, th->btnFill);
            number(r.x + 10, r.cy() + 1, userPresets[i].c_str(), 13.f, sel ? th->selText : th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        }
        if (!presetMsg.empty()) label(100, 624, presetMsg.c_str(), 10.f, th->lock, ALIGN_LEFT | ALIGN_MIDDLE);
        button(presetsClose(), "CLOSE", true, 11.f);
    }
    void presetsClick(float x, float y)
    {
        for (int i = 0; i < kNumPresets && i < 15; ++i) if (presetItem(0, i).in(x, y)) { loadFactory(i); return; }
        for (int i = 0; i < (int)userPresets.size() && i < 15; ++i) if (presetItem(1, i).in(x, y)) { loadUser(userPresets[i]); return; }
        if (presetsClose().in(x, y) || !presetsBox().in(x, y)) { overlay = ovNone; presetMsg.clear(); }
        repaint();
    }
    void loadFactory(int i)
    {
        snapshot("preset");
        std::vector<float> nv(values, values + kTotalParams);
        std::unique_ptr<Bank> nb(new Bank());
        buildPreset(i, nv.data(), *nb);
        applyState(nv.data(), nb.get());
        setParamGesture(pPATTERN, 0.f);
        presetNameStr = kPresets[i].name; stepPg = 0;
        repaint();
    }
    void loadUser(const std::string& name)
    {
        FILE* fp = fs::open(userDir() + name + ".kbas", "rb");
        if (!fp) { presetMsg = "Could not open " + name; repaint(); return; }
        std::string all; char buf[4096]; size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) all.append(buf, n);
        std::fclose(fp);
        if (all.rfind("KBAS1-PRESET", 0) != 0) { presetMsg = "Not a KBAS-1 preset: " + name; repaint(); return; }
        snapshot("preset");
        std::vector<float> nv(values, values + kTotalParams);
        const size_t bankPos = all.find("\nBANK\n");
        const std::string head = all.substr(0, bankPos == std::string::npos ? all.size() : bankPos);
        size_t st = 0;
        while (st < head.size()) {
            size_t en = head.find('\n', st); if (en == std::string::npos) en = head.size();
            int idx; float v;
            if (std::sscanf(head.c_str() + st, "V %d %f", &idx, &v) == 2 && idx >= 0 && idx < kSavedParams && std::isfinite(v))
                nv[idx] = idx == pTEMPO ? clampf(v, 40.f, 240.f) : clampf(v, 0.f, 127.f);
            st = en + 1;
        }
        std::unique_ptr<Bank> nb(new Bank());
        if (bankPos == std::string::npos || !deserializeBank(*nb, all.c_str() + bankPos + 6)) nb.reset(new Bank(*bank));
        applyState(nv.data(), nb.get());
        presetNameStr = name;
        repaint();
    }
    void openSave() { saveName = presetNameStr == "Init" ? "" : presetNameStr; saveConfirm = false; overlay = ovSave; repaint(); }
    bool saveExists() const { return std::find(userPresets.begin(), userPresets.end(), saveName) != userPresets.end(); }
    void commitSave()
    {
        while (!saveName.empty() && saveName.back() == ' ') saveName.pop_back();
        if (saveName.empty()) return;
        if (saveExists() && !saveConfirm) { saveConfirm = true; repaint(); return; }
        FILE* fp = fs::open(userDir() + saveName + ".kbas", "wb");
        if (!fp) { presetMsg = "Could not write the preset."; overlay = ovNone; repaint(); return; }
        std::fprintf(fp, "KBAS1-PRESET 1\nNAME %s\n", saveName.c_str());
        for (int i = 0; i < kSavedParams; ++i) {
            if (i == pPLAY || i == pPATTERN) continue;
            std::fprintf(fp, "V %d %.4f\n", i, values[i]);
        }
        std::fprintf(fp, "BANK\n%s", serializeBank(*bank).c_str());
        std::fclose(fp);
        presetNameStr = saveName; scanUser();
        overlay = ovNone; repaint();
    }
    void drawSave()
    {
        const R bx = saveBox();
        overlayBox(bx);
        number(320, 236, "Save as preset", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(320, 270, "Name (type on the keyboard, Enter = save, Esc = cancel)", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        const R f = saveField();
        box(f, th->lcdBg); frame(f, th->lcdText, -1.f, 1.5f);
        number(f.x + 16, f.cy() + 1, saveName.c_str(), 20.f, th->lcdText, ALIGN_LEFT | ALIGN_MIDDLE);
        if (saveExists()) label(320, 366, saveConfirm ? "Click SAVE again to replace it." : "A preset with this name exists: it will be replaced.", 10.5f, th->lock, ALIGN_LEFT | ALIGN_MIDDLE);
        else label(320, 366, "Saves the sound and the 16 patterns.", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        button(saveCancel(), "CANCEL", false, 11.f);
        button(saveOk(), saveConfirm ? "REPLACE" : "SAVE", true, 12.f);
    }
    bool onCharacterInput(const CharacterInputEvent& ev) override
    {
        if (overlay != ovSave) return false;
        const uint c = ev.character;
        if (c >= 32 && c < 127 && saveName.size() < 28) {
            const char ch = (char)c;
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ' || ch == '-' || ch == '_') { saveName.push_back(ch); saveConfirm = false; repaint(); }
        }
        return true;
    }

    // ------------------------------------------------------------------ skins window
    void drawSkins()
    {
        overlayBox(viewBox());
        number(264, 124, "Skin and size", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(916, 124, "saved with the project and for new instances", 10.f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        for (int i = 0; i < kNumThemes; ++i) {
            const R r = skinRow(i);
            const Theme& t = kThemes[i];
            box(r, t.bg, 4.f); frame(r, i == skin ? th->accent : th->btnEdge, 4.f, i == skin ? 3.f : 1.f);
            const Theme* keep = th; th = &t;
            number(r.x + 14, r.y + 22, t.name, 16.f, t.text, ALIGN_LEFT | ALIGN_MIDDLE);
            box({r.x + 14, r.y + 40, 120, 52}, t.lcdBg);
            label(r.x + 22, r.y + 52, "RESO", 8.f, t.lcdDim, ALIGN_LEFT | ALIGN_MIDDLE);
            number(r.x + 22, r.y + 74, "75", 20.f, t.lcdText, ALIGN_LEFT | ALIGN_MIDDLE);
            drawKnob({r.x + 140, r.y + 26, 80, 110}, 0.6f, "", "", false, false);
            for (int k = 0; k < 4; ++k) {
                const R st{r.x + 224 + (k % 2) * 40.f, r.y + 40 + (k / 2) * 44.f, 36, 40};
                box(st, t.stepFill, 3.f);
                if (k == 0 || k == 3) box({st.x + 3, st.y + 3, st.w - 6, st.h - 6}, onColor(), 2.f);
            }
            th = keep;
        }
        label(264, 468, "WINDOW SIZE", 11.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        static const int zooms[6] = {80, 100, 120, 140, 160, 200};
        for (int i = 0; i < 6; ++i) { char b[16]; std::snprintf(b, sizeof(b), "%d%%", zooms[i]); button(zoomBtn(i), b, std::abs(zoomPercent() - zooms[i]) <= 2, 12.f); }
        label(264, 546, "You can also drag the corner at the bottom right of the window to any size.", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
    }
    void drawGrip()
    {
        const Color c = drag == dkResize ? th->accent : Color(th->dim, th->bg, 0.2f);
        for (int i = 0; i < 3; ++i) {
            const float o = 6.f + i * 5.f;
            beginPath(); moveTo(W - 3, H - o); lineTo(W - o, H - 3); strokeColor(c); strokeWidth(1.6f); lineCap(ROUND); stroke();
        }
    }

    // ------------------------------------------------------------------ About / updates and licence windows
    void drawUpdate()
    {
        const R bx = updBox();
        overlayBox(bx);
        using C = upd::Checker;
        const C::State st = updater->getState();
        const upd::Manifest m = updater->manifest();
        number(320, bx.y + 36, "Kinetik KBAS-1", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        label(860, bx.y + 36, (std::string("version ") + KN_VERSION).c_str(), 11.f, th->dim, ALIGN_RIGHT | ALIGN_MIDDLE);
        std::string l1, l2;
        switch (st) {
        case C::stChecking: l1 = "Checking for updates..."; break;
        case C::stAvailable: l1 = "Version " + m.version + " is available."; l2 = m.notes; break;
        case C::stUpToDate: l1 = "You have the latest version."; break;
        case C::stDownloading: l1 = "Downloading and checking the installer..."; break;
        case C::stInstalling: l1 = "Installing version " + m.version + "..."; l2 = "Accept the system prompt. Keep working: the music does not stop."; break;
        case C::stInstalled: l1 = "Version " + m.version + " is installed."; l2 = "It is used the next time the plug-in loads."; break;
        case C::stOpened: l1 = "The installer is open."; l2 = "Close your music software before you finish the installation, then open it again."; break;
        case C::stFailed: l1 = updater->message(); l2 = "Check your internet connection and try again."; break;
        default: l1 = "Click CHECK NOW to look for a new version."; break;
        }
        label(320, bx.y + 86, l1.c_str(), 13.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        if (!l2.empty()) { fontFace(NANOVG_DEJAVU_SANS_TTF); fontSize(12.5f); fillColor(th->dim); textAlign(ALIGN_LEFT | ALIGN_TOP); textBox(320, bx.y + 104, 540, l2.c_str(), nullptr); }
        button(updAuto(), updater->autoCheck() ? "AUTO CHECK: ON" : "AUTO CHECK: OFF", updater->autoCheck(), 10.5f);
        button(updAutoInst(), updater->autoInstall() ? "AUTO INSTALL: ON" : "AUTO INSTALL: OFF", updater->autoInstall(), 10.5f);
        button(updLic(), licInfo.valid ? "LICENCE: REGISTERED" : "ENTER A LICENCE", false, 10.5f);
        const bool avail = st == C::stAvailable, busy = st == C::stChecking || st == C::stDownloading || st == C::stInstalling;
        button(updMain(), busy ? "PLEASE WAIT..." : (avail ? (m.url.empty() ? "OPEN DOWNLOAD PAGE" : "DOWNLOAD & INSTALL") : "CHECK NOW"), avail, 11.f);
        if (avail) button(updSkip(), "SKIP THIS ONE", false, 10.5f);
        button(updClose(), "CLOSE", false, 11.f);
    }
    bool updateClick(float x, float y)
    {
        using C = upd::Checker;
        const C::State st = updater->getState();
        if (updMain().in(x, y)) { if (st == C::stAvailable) updater->install(); else if (st != C::stChecking && st != C::stDownloading && st != C::stInstalling) updater->check(true); }
        else if (st == C::stAvailable && updSkip().in(x, y)) updater->skipThisVersion();
        else if (updAuto().in(x, y)) updater->setAutoCheck(!updater->autoCheck());
        else if (updAutoInst().in(x, y)) updater->setAutoInstall(!updater->autoInstall());
        else if (updLic().in(x, y)) { overlay = ovLicense; licMsg.clear(); }
        else if (updClose().in(x, y) || !updBox().in(x, y)) overlay = ovNone;
        repaint(); return true;
    }
    void drawLicense()
    {
        const R bx = licBox();
        overlayBox(bx);
        number(320, bx.y + 36, licInfo.valid ? "Registered" : "Register KBAS-1", 22.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
        if (licInfo.valid) {
            label(320, bx.y + 82, ("Licensed to " + licInfo.name).c_str(), 13.f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
            label(320, bx.y + 108, licInfo.email.c_str(), 11.f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
            label(320, bx.y + 134, ("Licence " + licInfo.product + "  -  order " + licInfo.order + "  -  " + licInfo.issued).c_str(), 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        } else {
            label(320, bx.y + 80, "This copy runs in demo mode: the sound drops out for 3 seconds every 45 seconds.", 10.5f, th->text, ALIGN_LEFT | ALIGN_MIDDLE);
            label(320, bx.y + 106, "After your purchase you receive a licence file by e-mail.", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
            label(320, bx.y + 130, "Load that file, or copy its text and press PASTE LICENSE (or Ctrl/Cmd+V).", 10.5f, th->dim, ALIGN_LEFT | ALIGN_MIDDLE);
        }
        label(320, bx.y + 210, licMsg.c_str(), 10.5f, th->lock, ALIGN_LEFT | ALIGN_MIDDLE);
        button(licLoad(), "LOAD LICENSE FILE", false, 10.5f);
        button(licPaste(), "PASTE LICENSE", false, 10.5f);
        button(licClose(), "CLOSE", true, 11.f);
    }
    void applyLicenseText(const std::string& t)
    {
        if (t.empty()) { licMsg = "Nothing to read."; repaint(); return; }
        const lic::Info i = lic::install(t, "KBAS-1");
        if (i.valid) { licInfo = i; licMsg = "Licence installed. The demo limits are removed."; setState("cmd", "lic"); }
        else licMsg = i.error;
        repaint();
    }
    void pasteLicense()
    {
        size_t n = 0;
        const void* d = getWindow().getClipboard(n);
        if (d == nullptr || n == 0) { licMsg = "The clipboard is empty. Copy the licence text first."; repaint(); return; }
        std::string t((const char*)d, n);
        while (!t.empty() && t.back() == '\0') t.pop_back();
        applyLicenseText(t);
    }
    void uiFileBrowserSelected(const char* filename) override
    {
        if (filename == nullptr) return;
        std::string text;
        if (!lic::readFile(filename, text)) { licMsg = "Could not read that file."; repaint(); return; }
        applyLicenseText(text);
    }
    bool licenseClick(float x, float y)
    {
        if (licLoad().in(x, y)) {
            FileBrowserOptions o; o.title = "Load your Kinetik licence";
            if (!openFileBrowser(o)) licMsg = "The file browser is not available here: use PASTE LICENSE.";
            repaint(); return true;
        }
        if (licPaste().in(x, y)) { pasteLicense(); return true; }
        if (licClose().in(x, y) || !licBox().in(x, y)) { overlay = ovNone; licMsg.clear(); repaint(); }
        return true;
    }

    // ------------------------------------------------------------------ whole-state helpers (presets, random, undo)
    void applyState(const float* nv, const Bank* nb)
    {
        const bool was = restoring; restoring = true;
        for (int i = 0; i < kSavedParams; ++i) {
            if (i == pPLAY || i == pPATTERN) continue;
            if (std::fabs(values[i] - nv[i]) > 1e-4f) setParamGesture((uint32_t)i, nv[i]);
        }
        if (nb) { *bank = *nb; setState("bank", serializeBank(*bank).c_str()); }
        restoring = was;
        repaint();
    }
    struct Snap { std::vector<float> values; std::string bank; std::string what; };
    static double nowSec() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
    Snap capture(const char* what) const { Snap s; s.values.assign(values, values + kTotalParams); s.bank = serializeBank(*bank); s.what = what; return s; }
    void snapshot(const char* what)
    {
        if (restoring) return;
        undoStack.push_back(capture(what));
        if (undoStack.size() > 40) undoStack.pop_front();
        redoStack.clear();
        lastSnapAt = nowSec(); lastSnapWhat = what;
    }
    void autoSnap(const char* what, double window)
    {
        if (restoring) return;
        const double t = nowSec();
        if (t - lastSnapAt < 0.15 || (lastSnapWhat == what && t - lastSnapAt < window)) { if (lastSnapWhat == what) lastSnapAt = t; return; }
        snapshot(what);
    }
    void restoreSnap(const Snap& s)
    {
        std::unique_ptr<Bank> b(new Bank()); deserializeBank(*b, s.bank.c_str());
        std::vector<float> v = s.values; applyState(v.data(), b.get());
        lastSnapAt = -10.0; lastSnapWhat.clear();
    }
    void undo() { if (undoStack.empty()) return; Snap s = std::move(undoStack.back()); undoStack.pop_back(); redoStack.push_back(capture(s.what.c_str())); restoreSnap(s); repaint(); }
    void redo() { if (redoStack.empty()) return; Snap s = std::move(redoStack.back()); redoStack.pop_back(); undoStack.push_back(capture(s.what.c_str())); restoreSnap(s); repaint(); }

    void drawTip()
    {
        if (tipText.empty()) return;
        const float wMax = 300.f;
        fontFace(NANOVG_DEJAVU_SANS_TTF); fontSize(12.f); textAlign(ALIGN_LEFT | ALIGN_TOP); textLineHeight(1.25f);
        float b[4]; textBoxBounds(0, 0, wMax, tipText.c_str(), nullptr, b);
        const float w = b[2] - b[0] + 20.f, h = b[3] - b[1] + 14.f;
        float x = hovX + 14.f, y = hovY + 18.f;
        if (x + w > W - 6) x = W - 6 - w;
        if (y + h > H - 6) y = hovY - h - 10.f;
        beginPath(); roundedRect(x + 2, y + 3, w, h, 6); fillColor(Color(0, 0, 0, 0.25f)); fill();
        box({x, y, w, h}, th->panel, 6.f); frame({x, y, w, h}, th->text, 6.f);
        fillColor(th->text); textBox(x + 10, y + 7, wMax, tipText.c_str(), nullptr);
    }
    // ------------------------------------------------------------------ tooltips
    std::string tipAt(float x, float y)
    {
        static const char* vt[8] = {"TUNE: the whole voice up or down (semitones).", "WAVE: saw to square.", "CUTOFF: where the 4-pole low pass filter closes.",
            "RESO: filter resonance - the squelch. High values sing at the cutoff.", "ENV MOD: how far each note opens the filter.",
            "DECAY: how fast the filter closes after each note (accented notes are always short).",
            "ACCENT: how much louder and brighter the accented steps are; accents in a row climb higher.", "SLIDE: glide time between sliding steps."};
        static const char* ft[7] = {"AMP DECAY: fade of each note (HOLD = full level while the gate is open).", "DRIVE: saturation after the filter.", "TONE: darker / brighter after the drive.",
            "DELAY TIME: echo time in tempo.", "FEEDBACK: echo repeats (ping-pong).", "DELAY MIX: echo level.", "VOLUME: output level."};
        for (int k = 0; k < 8; ++k) if (vKnob(k).in(x, y)) return vt[k];
        for (int k = 0; k < 7; ++k) if (fKnob(k).in(x, y)) return ft[k];
        for (int i = 0; i < 16; ++i) {
            if (trigCell(i).in(x, y)) return "Click: note on / off on this step (drag to paint).";
            if (noteCell(i).in(x, y)) return "Drag up / down: the note (semitones; SNAP keeps it in the scale). Click the + / - ends: an octave up / down. Double-click: the root.";
            if (accCell(i).in(x, y)) return "ACCENT: louder, brighter and snappier note.";
            if (slideCell(i).in(x, y)) return "SLIDE: this note glides into the next one without a new attack.";
        }
        if (acidBtn().in(x, y)) return "ACID GEN: a new random line in the scale and root (UNDO restores).";
        if (scaleBtn().in(x, y)) return "Scale used by ACID GEN and SNAP. Click = next, right-click = previous.";
        if (rootBtn().in(x, y)) return "Root note of the scale. Click = up, right-click = down.";
        if (snapBtn().in(x, y)) return "SNAP: dragged notes stay in the scale.";
        if (transBtn().in(x, y)) return "KEY TRANSPOSE: while the pattern plays, MIDI keys transpose it (C3 = as written) instead of playing notes.";
        if (rotL().in(x, y) || rotR().in(x, y)) return "Shift the whole pattern one step left or right.";
        for (int i = 0; i < 2; ++i) if (pageBtn(i).in(x, y)) return "Show steps 1-16 or 17-32 (set LENGTH up to 32).";
        if (patLenBox().in(x, y)) return "Pattern length in steps (1-32). Drag or scroll.";
        if (swingBox().in(x, y)) return "Swing: delays every second 16th (50% = straight). Drag or scroll.";
        if (playBtn().in(x, y)) return "Internal play when your music software is stopped (KBAS-1 follows its transport when it plays).";
        if (tempoBox().in(x, y)) return "Internal tempo. Drag or scroll.";
        for (int i = 0; i < kPatterns; ++i) if (patBtn(i).in(x, y)) return "Choose the pattern (while playing it starts at the end of the current one).";
        if (undoBtn().in(x, y)) return "Undo (Ctrl/Cmd+Z). Right-click = redo.";
        if (presetBtn().in(x, y)) return "Factory presets and your own.";
        if (saveBtn().in(x, y)) return "Save the sound and the 16 patterns as a preset of your own.";
        if (skinBtn().in(x, y)) return "Skin and window size. Right-click = next skin.";
        if (logoArea().in(x, y)) return "About KBAS-1, updates and licence.";
        return std::string();
    }

    // ------------------------------------------------------------------ editing
    int snapNote(int n, int dir) const
    {
        n = clampi(n, 12, 108);
        if (!snap) return n;
        for (int i = 0; i < 12 && !inScale(n, bank->glob.root, bank->glob.scale); ++i) n += dir >= 0 ? 1 : -1;
        return clampi(n, 12, 108);
    }
    void setNote(int s, int n) { if (pat().note[s] == n) return; pat().note[s] = (uint8_t)n; sendStep(s); }
    bool stopped() const { return gp(pOUT_STEP) < 0.f; }
    void showPatMsg(const std::string& m) { patMsg = m; patMsgUntil = nowSec() + 2.0; repaint(); }

    bool onKeyboard(const KeyboardEvent& ev) override
    {
        if (overlay == ovLicense) {
            if (ev.press && (ev.key == 'v' || ev.key == 'V') && (ev.mod & (kModifierControl | kModifierSuper))) pasteLicense();
            else if (ev.press && ev.key == kKeyEscape) { overlay = ovNone; licMsg.clear(); repaint(); }
            return true;
        }
        if (overlay == ovSave) {
            if (ev.press) {
                if (ev.key == kKeyEscape) { overlay = ovNone; repaint(); }
                else if (ev.key == kKeyEnter || ev.key == '\n') commitSave();
                else if (ev.key == kKeyBackspace || ev.key == kKeyDelete) { if (!saveName.empty()) { saveName.pop_back(); saveConfirm = false; repaint(); } }
            }
            return true;
        }
        if (ev.press && (ev.mod & (kModifierControl | kModifierSuper)) && overlay == ovNone) {
            if (ev.key == 'z' || ev.key == 'Z') { if (ev.mod & kModifierShift) redo(); else undo(); return true; }
            if (ev.key == 'y' || ev.key == 'Y') { redo(); return true; }
        }
        if (ev.press && ev.key == kKeyEscape && overlay != ovNone) { overlay = ovNone; repaint(); return true; }
        return false;
    }

    bool onMouse(const MouseEvent& ev) override
    {
        const float x = ev.pos.getX() / sx, y = ev.pos.getY() / sx;
        if (tipShown) { tipShown = false; repaint(); }
        tipBlockUntil = nowSec() + 1.0;
        if (!ev.press) {
            if (drag == dkKnob || drag == dkTempo) editParameter((uint32_t)dragIndex, false);
            if (drag == dkPatLen || drag == dkSwing) sendPatternSettings();
            if (drag == dkNote) { if (auditioning >= 0) { audition(auditioning, false); auditioning = -1; } sendStep(dragIndex); }
            if (drag == dkResize) saveView();
            if (drag != dkNone) { drag = dkNone; repaint(); }
            return true;
        }
        const bool dbl = (ev.time - lastClickTime) < 350 && std::fabs(x - lastX) < 4 && std::fabs(y - lastY) < 4;
        lastClickTime = ev.time; lastX = x; lastY = y;
        const bool right = ev.button == kMouseButtonRight;

        if (gripR().in(x, y) && overlay == ovNone) { drag = dkResize; resizeStartW = getWidth(); grabX = ev.pos.getX(); grabY = ev.pos.getY(); return true; }
        if (overlay == ovPresets) { presetsClick(x, y); return true; }
        if (overlay == ovLicense) return licenseClick(x, y);
        if (overlay == ovUpdate) return updateClick(x, y);
        if (overlay == ovSave) { if (saveOk().in(x, y)) commitSave(); else if (saveCancel().in(x, y) || !saveBox().in(x, y)) { overlay = ovNone; repaint(); } return true; }
        if (overlay == ovSkins) {
            static const int zooms[6] = {80, 100, 120, 140, 160, 200};
            for (int i = 0; i < kNumThemes; ++i) if (skinRow(i).in(x, y)) { setSkin(i, true); return true; }
            for (int i = 0; i < 6; ++i) if (zoomBtn(i).in(x, y)) { setZoom(zooms[i] / 100.0); saveView(); return true; }
            if (!viewBox().in(x, y)) { overlay = ovNone; repaint(); }
            return true;
        }
        if (logoArea().in(x, y)) { overlay = ovUpdate; repaint(); return true; }
        if (skinBtn().in(x, y)) { if (right) setSkin((skin + 1) % kNumThemes, true); else overlay = ovSkins; repaint(); return true; }
        if (playBtn().in(x, y)) { setParamGesture(pPLAY, gp(pPLAY) > 0.5f ? 0.f : 1.f); repaint(); return true; }
        if (tempoBox().in(x, y)) { drag = dkTempo; dragIndex = pTEMPO; dragY = y; dragStart = gp(pTEMPO); editParameter(pTEMPO, true); return true; }
        if (patLenBox().in(x, y)) { drag = dkPatLen; dragY = y; dragStart = pat().length; return true; }
        if (swingBox().in(x, y)) { drag = dkSwing; dragY = y; dragStart = pat().swing; return true; }
        for (int k = 0; k < 15; ++k) {
            const int p = k < 8 ? kVoice[k] : kFx[k - 8];
            if (!(k < 8 ? vKnob(k) : fKnob(k - 8)).in(x, y)) continue;
            if (dbl) { setParamGesture((uint32_t)p, kParamDefaults[p]); repaint(); return true; }
            drag = dkKnob; dragIndex = p; dragY = y; dragStart = gp(p); editParameter((uint32_t)p, true); repaint(); return true;
        }
        for (int i = 0; i < kPatterns; ++i) if (patBtn(i).in(x, y)) { setParamGesture(pPATTERN, (float)i); repaint(); return true; }
        if (patCopy().in(x, y)) { patClip = editPattern(); showPatMsg("A" + std::to_string(patClip + 1) + " copied"); return true; }
        if (patPaste().in(x, y) && patClip >= 0) { char b[32]; std::snprintf(b, sizeof(b), "cp %d %d", patClip, editPattern()); sendCmd(b); showPatMsg("pasted"); return true; }
        if (patClear().in(x, y)) { char b[24]; std::snprintf(b, sizeof(b), "cl %d", editPattern()); sendCmd(b); repaint(); return true; }
        for (int i = 0; i < 2; ++i) if (pageBtn(i).in(x, y)) { stepPg = i; repaint(); return true; }
        if (rotL().in(x, y) || rotR().in(x, y)) { char b[32]; std::snprintf(b, sizeof(b), "rot %d %d", editPattern(), rotL().in(x, y) ? -1 : 1); sendCmd(b); repaint(); return true; }
        if (acidBtn().in(x, y)) { acidGenerate(); return true; }
        if (scaleBtn().in(x, y)) { bank->glob.scale = (uint8_t)((bank->glob.scale + (right ? kScales - 1 : 1)) % kScales); sendGlob(); repaint(); return true; }
        if (rootBtn().in(x, y)) { bank->glob.root = (uint8_t)((bank->glob.root + (right ? 11 : 1)) % 12); sendGlob(); repaint(); return true; }
        if (snapBtn().in(x, y)) { snap = !snap; repaint(); return true; }
        if (transBtn().in(x, y)) { bank->glob.transpose = bank->glob.transpose ? 0 : 1; sendGlob(); repaint(); return true; }
        if (undoBtn().in(x, y)) { if (right) redo(); else undo(); return true; }
        if (presetBtn().in(x, y)) { scanUser(); overlay = ovPresets; repaint(); return true; }
        if (saveBtn().in(x, y)) { scanUser(); openSave(); return true; }
        // steps
        Pattern& p = pat();
        for (int i = 0; i < 16; ++i) {
            const int s = stepPg * 16 + i;
            if (s >= p.length) continue;
            if (trigCell(i).in(x, y)) {
                paintVal = !p.on[s]; p.on[s] = paintVal; sendStep(s);
                drag = dkPaint; repaint(); return true;
            }
            if (noteCell(i).in(x, y)) {
                const R n = noteCell(i);
                if (dbl) { setNote(s, snapNote(36 + bank->glob.root, 1)); repaint(); return true; }
                if (y < n.y + 18) { setNote(s, clampi(p.note[s] + 12, 12, 108)); repaint(); return true; }
                if (y > n.y + n.h - 18) { setNote(s, clampi(p.note[s] - 12, 12, 108)); repaint(); return true; }
                if (!p.on[s]) { p.on[s] = 1; sendStep(s); }
                drag = dkNote; dragIndex = s; dragY = y; dragStart = p.note[s];
                if (stopped()) { auditioning = p.note[s]; audition(auditioning, true); }
                repaint(); return true;
            }
            if (accCell(i).in(x, y)) { p.accent[s] = !p.accent[s]; if (p.accent[s] && !p.on[s]) p.on[s] = 1; sendStep(s); repaint(); return true; }
            if (slideCell(i).in(x, y)) { p.slide[s] = !p.slide[s]; if (p.slide[s] && !p.on[s]) p.on[s] = 1; sendStep(s); repaint(); return true; }
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        {
            const float hx = ev.pos.getX() / sx, hy = ev.pos.getY() / sx;
            if (std::fabs(hx - hovX) > 3.f || std::fabs(hy - hovY) > 3.f) { hovX = hx; hovY = hy; hovSince = nowSec(); tipNone = false; if (tipShown) { tipShown = false; repaint(); } }
        }
        if (drag == dkNone) return false;
        if (drag == dkResize) {
            const double dx = ev.pos.getX() - grabX, dy = (ev.pos.getY() - grabY) * (double)W / H;
            const double nw = resizeStartW + (dx + dy) * 0.5;
            const double z = std::max(0.6, std::min(3.0, nw / (W * baseScale())));
            const uint w = (uint)std::lround(W * baseScale() * z), h = (uint)std::lround(H * baseScale() * z);
            if (std::abs((int)w - (int)getWidth()) >= 3) setSize(w, h);
            return true;
        }
        const float x = ev.pos.getX() / sx, y = ev.pos.getY() / sx;
        const float sens = (ev.mod & kModifierShift) ? 0.12f : 0.6f;
        const float v = dragStart + (dragY - y) * sens;
        switch (drag) {
        case dkKnob: setParam((uint32_t)dragIndex, clampf(v, 0.f, 127.f)); repaint(); break;
        case dkTempo: setParam(pTEMPO, clampf(std::round((dragStart + (dragY - y) * ((ev.mod & kModifierShift) ? 0.05f : 0.5f)) * 10.f) / 10.f, 40.f, 240.f)); repaint(); break;
        case dkPatLen: pat().length = (uint8_t)clampi((int)std::lround(dragStart + (dragY - y) * 0.15f), 1, kSteps); repaint(); break;
        case dkSwing: pat().swing = (uint8_t)clampi((int)std::lround(dragStart + (dragY - y) * 0.15f), 50, 80); repaint(); break;
        case dkNote: {
            const int raw = (int)std::lround(dragStart + (dragY - y) / 6.f);
            const int n = snapNote(raw, raw >= (int)dragStart ? 1 : -1);
            if (n != pat().note[dragIndex]) {
                pat().note[dragIndex] = (uint8_t)n;
                if (auditioning >= 0) { audition(auditioning, false); auditioning = n; audition(n, true); }
                repaint();
            }
            break;
        }
        case dkPaint: {
            Pattern& p = pat();
            for (int i = 0; i < 16; ++i) {
                const R r = trigCell(i);
                const int s = stepPg * 16 + i;
                if (x >= r.x && x < r.x + r.w + 4.f && s < p.length && (p.on[s] != 0) != paintVal) { p.on[s] = paintVal; sendStep(s); }
            }
            repaint(); break;
        }
        default: break;
        }
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float x = ev.pos.getX() / sx, y = ev.pos.getY() / sx;
        const float d = ev.delta.getY() > 0 ? 1.f : (ev.delta.getY() < 0 ? -1.f : 0.f);
        if (d == 0.f || overlay != ovNone) return false;
        for (int k = 0; k < 15; ++k) {
            const int p = k < 8 ? kVoice[k] : kFx[k - 8];
            if ((k < 8 ? vKnob(k) : fKnob(k - 8)).in(x, y)) { setParamGesture((uint32_t)p, clampf(gp(p) + d, 0.f, 127.f)); repaint(); return true; }
        }
        for (int i = 0; i < 16; ++i) {
            const int s = stepPg * 16 + i;
            if (noteCell(i).in(x, y) && s < pat().length) { setNote(s, snapNote(pat().note[s] + (int)d, (int)d)); repaint(); return true; }
        }
        if (tempoBox().in(x, y)) { setParamGesture(pTEMPO, clampf(gp(pTEMPO) + d, 40.f, 240.f)); repaint(); return true; }
        if (patLenBox().in(x, y)) { pat().length = (uint8_t)clampi(pat().length + (int)d, 1, kSteps); sendPatternSettings(); repaint(); return true; }
        if (swingBox().in(x, y)) { pat().swing = (uint8_t)clampi(pat().swing + (int)d, 50, 80); sendPatternSettings(); repaint(); return true; }
        return false;
    }

    void uiIdle() override
    {
        if (!tipShown && !tipNone && drag == dkNone && overlay == ovNone && hovX >= 0.f && nowSec() - hovSince > 0.8 && nowSec() > tipBlockUntil) {
            tipText = tipAt(hovX, hovY);
            if (!tipText.empty()) { tipShown = true; repaint(); } else tipNone = true;
        }
        flushPv();
        if (updater && (int)updater->getState() != updLastState) { updLastState = (int)updater->getState(); repaint(); }
        if (!patMsg.empty() && nowSec() > patMsgUntil) { patMsg.clear(); repaint(); }
        if (viewDirty && drag != dkResize && ++viewDirtyTicks > 30) saveView();
    }

private:
    float values[kTotalParams];
    std::unique_ptr<Bank> bank;
    float sx = 1.f;
    int skin = kDefaultSkin;
    const Theme* th = &kThemes[kDefaultSkin];
    FontId fonts[kFontCount] = {};
    NanoImage haloImage;
    int stepPg = 0, lockStep = -1, auditioning = -1;
    bool snap = true;
    uint32_t rndSeed = 0x2545F491u;
    Overlay overlay = ovNone;
    DragKind drag = dkNone; int dragIndex = 0; float dragY = 0, dragStart = 0; bool paintVal = true;
    uint lastClickTime = 0; float lastX = -100, lastY = -100;
    int patClip = -1;
    std::string patMsg; double patMsgUntil = 0;
    std::string presetNameStr = "Init", presetMsg, saveName; bool saveConfirm = false;
    std::vector<std::string> userPresets;
    std::deque<Snap> undoStack, redoStack; bool restoring = false; double lastSnapAt = -10.0; std::string lastSnapWhat;
    std::unique_ptr<upd::Checker> updater; int updLastState = -1;
    lic::Info licInfo; std::string licMsg;
    bool viewDirty = false; int viewDirtyTicks = 0;
    double resizeStartW = 0, grabX = 0, grabY = 0;
    float hovX = -1.f, hovY = -1.f; double hovSince = 0, tipBlockUntil = 0; bool tipShown = false, tipNone = false; std::string tipText;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KbasUI)
};

UI* createUI() { return new KbasUI(); }

END_NAMESPACE_DISTRHO
