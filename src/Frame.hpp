// Kinetik common frame — shared by KDSP-16, KMSP-6, KBAS-1, KFX-8 and KGRN-4 (identical in every repository,
// checked by scripts/check-shared.sh). One window size and the same header and bottom bar at the same places in every
// plug-in (owner's request, 2026-11: "les boutons aux mêmes endroits, même ordre, même taille").
//
//   header (y 9..49)   logo | the plug-in's own boxes, packed right up to x 692 | PAT | PLAY | BPM | SKIN / SIZE
//   bottom bar (728)   REC CLICK | RANDOM (menu) | FILL (HOLD) | UNDO | the plug-in's extras ... | PRESETS | SAVE
//
// Header-only. The drawing goes through each UI's own primitives (box, frame, label, number) and its theme (Skins.hpp,
// shared), so the bar looks like the rest of the plug-in. R is the UI's rectangle type ({x, y, w, h}, in(), cx(), cy()).
#pragma once
#include <string>
#include <vector>
#include <utility>
#include <algorithm>

namespace kframe {

static constexpr float kW = 1180.f, kH = 800.f;   // the canvas of every plug-in (zoom scales it)

struct Rc {
    float x, y, w, h;
    bool in(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// ---- header
inline Rc patBox() { return {700.f, 9.f, 130.f, 40.f}; }
inline Rc playBtn() { return {840.f, 9.f, 84.f, 40.f}; }
inline Rc tempoBox() { return {932.f, 9.f, 110.f, 40.f}; }
inline Rc skinBtn() { return {1050.f, 9.f, 116.f, 40.f}; }
static constexpr float kHeaderBoxesEnd = 692.f;    // the plug-in's own header boxes end here, 4 px apart

// ---- bottom bar
inline Rc bar() { return {14.f, 728.f, 1152.f, 62.f}; }
inline Rc recBtn() { return {26.f, 737.f, 96.f, 44.f}; }
inline Rc clickBtn() { return {128.f, 737.f, 80.f, 44.f}; }
inline Rc randomBtn() { return {222.f, 737.f, 130.f, 44.f}; }    // left part: repeat the last choice
inline Rc randomArrow() { return {324.f, 737.f, 28.f, 44.f}; }   // right part: open the menu
inline Rc fillBtn() { return {358.f, 737.f, 130.f, 44.f}; }
inline Rc undoBtn() { return {494.f, 737.f, 112.f, 44.f}; }
static constexpr float kExtrasX = 618.f, kExtrasEnd = 834.f;   // the plug-in's own bottom buttons, 6 px apart
inline Rc extraBtn(float x, float w) { return {x, 737.f, w, 44.f}; }
inline Rc presetsBtn() { return {842.f, 737.f, 224.f, 44.f}; }
inline Rc saveBtn() { return {1072.f, 737.f, 82.f, 44.f}; }
// the RANDOM menu opens upwards from its button
inline Rc randomItem(int i, int n) { return {222.f, 731.f - (float)(n - i) * 38.f, 300.f, 36.f}; }
inline Rc randomMenu(int n) { return {216.f, 731.f - (float)n * 38.f - 6.f, 312.f, (float)n * 38.f + 8.f}; }

struct Bar {
    bool hasRec = true, rec = false, recLive = false, click = false;
    bool hasFill = true, fill = false;
    bool undoValid = false, redoValid = false;
    std::string undoWhat, preset;
    bool randomOpen = false;
    std::string randomLast;                                         // what a click on RANDOM does again
    std::vector<std::pair<std::string, std::string>> randomItems;   // name, what it changes
};

enum Hit { hNone = -1, hRec = 0, hClick, hRandom, hFill, hUndo, hPresets, hSave, hRandomArrow, hRandomItem = 100 };

// what a click at (x, y) hits; with the RANDOM menu open, its items come first (hRandomItem + index)
inline int hit(const Bar& b, float x, float y)
{
    if (b.randomOpen) {
        const int n = (int)b.randomItems.size();
        for (int i = 0; i < n; ++i) if (randomItem(i, n).in(x, y)) return hRandomItem + i;
    }
    if (b.hasRec && recBtn().in(x, y)) return hRec;
    if (b.hasRec && clickBtn().in(x, y)) return hClick;
    if (randomArrow().in(x, y)) return hRandomArrow;
    if (randomBtn().in(x, y)) return hRandom;
    if (b.hasFill && fillBtn().in(x, y)) return hFill;
    if (undoBtn().in(x, y)) return hUndo;
    if (presetsBtn().in(x, y)) return hPresets;
    if (saveBtn().in(x, y)) return hSave;
    return hNone;
}

// the drawing: each UI declares `template <class, class> friend struct kframe::Draw;` (its primitives are protected)
template <class R, class UI> struct Draw {
    static R toR(const Rc& c) { return R{c.x, c.y, c.w, c.h}; }

    // a button of the bar: one line, or a name + a small second line
    static void barButton(UI& u, const Rc& c, const char* name, const char* sub, bool on, bool red = false)
    {
        const R r = toR(c);
        if (on) u.box(r, u.th->selFill);
        else { u.box(r, u.th->btnFill); u.frame(r, red ? u.th->lock : u.th->btnEdge, -1.f, red ? 1.5f : 1.f); }
        const auto tc = on ? u.th->selText : (red ? u.th->lock : u.th->btnText);
        if (sub && *sub) {
            u.label(r.cx(), r.y + 16.f, name, 11.f, tc);
            u.label(r.cx(), r.y + 32.f, sub, 9.f, on ? u.th->selText : u.th->dim);
        } else u.label(r.cx(), r.cy() + 1.f, name, 11.f, tc);
    }

    static void drawBar(UI& u, const Bar& b)
    {
        u.panelBox(toR(bar()));
        if (b.hasRec) {
            const R rb = toR(recBtn());
            if (b.rec) u.box(rb, u.th->accent); else { u.box(rb, u.th->btnFill); u.frame(rb, u.th->btnEdge, -1.f, 1.5f); }
            const auto tc = b.rec ? u.th->accentText : u.th->btnText;
            u.beginPath(); u.circle(rb.cx() - 22.f, rb.cy(), 5.f); u.fillColor(b.rec ? u.th->accentText : u.th->accent); u.fill();
            u.label(rb.cx() + 8.f, rb.cy() + 1.f, "REC", 12.f, tc);
            if (b.recLive) { u.beginPath(); u.rect(rb.x + 6, rb.y + rb.h - 4, rb.w - 12, 2); u.fillColor(u.th->accentText); u.fill(); }
            barButton(u, clickBtn(), "CLICK", b.click ? "on" : "off", b.click);
        }
        {   // RANDOM: left = do the last choice again (shown below the name), right = the menu (triangle up while open)
            const R r = toR(randomBtn()), a = toR(randomArrow());
            u.box(r, u.th->btnFill); u.frame(r, u.th->btnEdge);
            if (b.randomOpen) u.box(a, u.th->selFill);
            u.beginPath(); u.rect(a.x, r.y + 6, 1.f, r.h - 12); u.fillColor(u.th->btnEdge); u.fill();
            u.label(r.x + 12.f, r.y + 16.f, "RANDOM", 11.f, u.th->btnText, UI::ALIGN_LEFT | UI::ALIGN_MIDDLE);
            u.label(r.x + 12.f, r.y + 32.f, b.randomLast.c_str(), 9.f, u.th->dim, UI::ALIGN_LEFT | UI::ALIGN_MIDDLE);
            const auto tc = b.randomOpen ? u.th->selText : u.th->btnText;
            const float cx = a.cx(), cy = a.cy();
            u.beginPath();
            if (b.randomOpen) { u.moveTo(cx - 6, cy + 4); u.lineTo(cx, cy - 3); u.lineTo(cx + 6, cy + 4); }
            else { u.moveTo(cx - 6, cy - 3); u.lineTo(cx, cy + 4); u.lineTo(cx + 6, cy - 3); }
            u.closePath(); u.fillColor(tc); u.fill();
        }
        if (b.hasFill) barButton(u, fillBtn(), "FILL", "hold", b.fill);
        barButton(u, undoBtn(), "UNDO", b.undoValid ? b.undoWhat.c_str() : (b.redoValid ? "right-click: redo" : ""), false, b.undoValid);
        {   // PRESETS shows the preset's name
            const R r = toR(presetsBtn());
            u.box(r, u.th->btnFill); u.frame(r, u.th->btnEdge, -1.f, 1.5f);
            u.label(r.x + 12.f, r.y + 12.f, "PRESETS", 9.f, u.th->dim, UI::ALIGN_LEFT | UI::ALIGN_MIDDLE);
            u.number(r.x + 12.f, r.y + r.h - 14.f, b.preset.c_str(), 17.f, u.th->text, UI::ALIGN_LEFT | UI::ALIGN_MIDDLE);
            u.beginPath(); u.moveTo(r.x + r.w - 20, r.cy() - 6); u.lineTo(r.x + r.w - 12, r.cy()); u.lineTo(r.x + r.w - 20, r.cy() + 6);
            u.closePath(); u.fillColor(u.th->btnText); u.fill();
        }
        barButton(u, saveBtn(), "SAVE", nullptr, false);
    }

    // a die: one-click random for the zone it sits in (sound, sequence, pattern, effect)
    static void dice(UI& u, const Rc& c, bool hot = false)
    {
        const R r = toR(c);
        u.box(r, hot ? u.th->selFill : u.th->btnFill); u.frame(r, u.th->btnEdge);
        const float s = std::min(r.w, r.h) - 8.f, x0 = r.cx() - s * 0.5f, y0 = r.cy() - s * 0.5f, p = s / 4.f;
        const float pr = std::max(1.6f, s * 0.09f);
        static const float pips[5][2] = {{1, 1}, {3, 1}, {2, 2}, {1, 3}, {3, 3}};
        for (const auto& q : pips) { u.beginPath(); u.circle(x0 + q[0] * p, y0 + q[1] * p, pr); u.fillColor(hot ? u.th->selText : u.th->btnText); u.fill(); }
    }

    // the RANDOM menu (draw it last, over everything)
    static void drawRandomMenu(UI& u, const Bar& b)
    {
        if (!b.randomOpen) return;
        const int n = (int)b.randomItems.size();
        const R m = toR(randomMenu(n));
        u.box(m, u.th->panel); u.frame(m, u.th->panelEdge, -1.f, 1.5f);
        for (int i = 0; i < n; ++i) {
            const R r = toR(randomItem(i, n));
            u.box(r, u.th->btnFill); u.frame(r, u.th->btnEdge);
            u.label(r.x + 12.f, r.cy() + 1.f, b.randomItems[(size_t)i].first.c_str(), 11.f, u.th->btnText, UI::ALIGN_LEFT | UI::ALIGN_MIDDLE);
            u.label(r.x + r.w - 12.f, r.cy() + 1.f, b.randomItems[(size_t)i].second.c_str(), 9.f, u.th->dim, UI::ALIGN_RIGHT | UI::ALIGN_MIDDLE);
        }
    }

};

} // namespace kframe
