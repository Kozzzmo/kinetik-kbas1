// Kinetik KDSP-16 — interface skins
#pragma once
#include "Color.hpp"

namespace kn {

using DGL_NAMESPACE::Color;

enum LogoStyle { lgSerif, lgRiso, lgNeon, lgClassic };
enum KnobStyle { ksTicks, ksArcThick, ksArc };
enum FontId { fnSans, fnSerif, fnSerifItalic, fnMono, fnBlack, fnLogo, kFontCount };   // fnLogo = Unbounded Bold

struct Theme {
    const char* name;
    // surfaces
    Color bg, panel, panelEdge, inner;            // inner = recessed area (pattern grid, randomize group)
    Color text, dim, faint;
    Color accent, accentText;                     // REC, trig LEDs, playhead
    Color lock;                                   // locked / recording values
    Color selFill, selText;                       // selected tabs, tracks, patterns
    Color btnFill, btnEdge, btnText;
    Color lcdBg, lcdText, lcdDim, nameFill, nameText;
    Color knobBody, knobRing, knobArc, knobTrack, knobPointer;
    Color stepFill, stepAlt, stepOff, ledOff, stepText;
    Color dots;                                   // dotted textures
    Color overlay;
    Color extra;                                  // ratchet / condition text under steps
    float radius;
    float panelEdgeWidth;
    int fontNum, fontLabel, fontButton;
    float labelSpacing;
    KnobStyle knob;
    LogoStyle logo;
    bool halftone;                                // dotted background texture
    bool glow;                                    // glowing numbers and logo
    bool dottedLeds;                              // dotted pattern on unlit steps / bars
};

static const Theme kThemes[] = {
    // ---- MIDNIGHT: navy, cream and vermilion, condensed serif numerals ----
    {"MIDNIGHT",
     Color(44, 51, 82), Color(40, 47, 77), Color(78, 87, 122), Color(33, 39, 66),
     Color(237, 230, 214), Color(160, 166, 190), Color(98, 106, 140),
     Color(232, 90, 64), Color(30, 34, 58),
     Color(236, 160, 70),
     Color(237, 230, 214), Color(32, 37, 64),
     Color(40, 47, 77), Color(96, 105, 140), Color(237, 230, 214),
     Color(29, 34, 58), Color(237, 230, 214), Color(160, 166, 190), Color(237, 230, 214), Color(30, 34, 58),
     Color(29, 34, 58), Color(96, 105, 140), Color(237, 230, 214), Color(78, 87, 122), Color(237, 230, 214),
     Color(33, 40, 68), Color(35, 43, 72), Color(38, 45, 74), Color(30, 35, 60), Color(237, 230, 214),
     Color(120, 128, 160),
     Color(10, 12, 24, 0.66f),
     Color(236, 160, 70),
     3.f, 1.2f, fnSerif, fnMono, fnMono, 0.8f, ksTicks, lgSerif, false, false, true},
    // ---- RISO: risograph vermilion, salmon paper and black ink ----
    {"RISO",
     Color(214, 69, 43), Color(214, 69, 43), Color(120, 30, 18), Color(240, 178, 160),
     Color(24, 16, 14), Color(96, 30, 18), Color(150, 50, 30),
     Color(24, 16, 14), Color(244, 190, 172),
     Color(255, 244, 236),
     Color(24, 16, 14), Color(244, 190, 172),
     Color(214, 69, 43), Color(120, 30, 18), Color(24, 16, 14),
     Color(240, 178, 160), Color(24, 16, 14), Color(110, 40, 26), Color(24, 16, 14), Color(244, 190, 172),
     Color(244, 196, 180), Color(120, 30, 18), Color(120, 30, 18), Color(196, 80, 56), Color(24, 16, 14),
     Color(240, 178, 160), Color(236, 170, 150), Color(222, 120, 96), Color(240, 178, 160), Color(24, 16, 14),
     Color(170, 60, 40),
     Color(60, 14, 8, 0.6f),
     Color(24, 16, 14),
     2.f, 2.f, fnBlack, fnMono, fnMono, 0.8f, ksArcThick, lgRiso, true, false, true},
    // ---- NEON: night blue with glowing periwinkle (from the logo) ----
    {"NEON",
     Color(9, 11, 26), Color(14, 18, 40), Color(42, 50, 100), Color(8, 10, 24),
     Color(200, 208, 255), Color(118, 128, 180), Color(60, 68, 120),
     Color(170, 182, 255), Color(9, 11, 26),
     Color(255, 70, 85),
     Color(170, 182, 255), Color(9, 11, 26),
     Color(14, 18, 40), Color(52, 62, 120), Color(200, 208, 255),
     Color(6, 8, 20), Color(206, 214, 255), Color(118, 128, 180), Color(170, 182, 255), Color(9, 11, 26),
     Color(16, 20, 46), Color(42, 50, 100), Color(170, 182, 255), Color(36, 42, 84), Color(225, 230, 255),
     Color(16, 20, 44), Color(19, 24, 52), Color(12, 15, 32), Color(22, 27, 58), Color(200, 208, 255),
     Color(70, 80, 140),
     Color(2, 3, 10, 0.72f),
     Color(255, 140, 150),
     6.f, 1.f, fnSerif, fnMono, fnMono, 0.7f, ksTicks, lgNeon, false, true, true},
    // ---- CLASSIC: the original graphite look ----
    {"CLASSIC",
     Color(22, 24, 28), Color(33, 36, 42), Color(60, 65, 74), Color(28, 30, 35),
     Color(214, 218, 224), Color(120, 127, 138), Color(80, 85, 94),
     Color(240, 64, 52), Color(255, 255, 255),
     Color(255, 170, 40),
     Color(36, 110, 104), Color(255, 255, 255),
     Color(42, 46, 53), Color(60, 65, 74), Color(214, 218, 224),
     Color(18, 26, 22), Color(150, 240, 170), Color(60, 110, 75), Color(30, 45, 36), Color(150, 240, 170),
     Color(52, 57, 65), Color(55, 60, 68), Color(60, 200, 190), Color(55, 60, 68), Color(235, 238, 242),
     Color(48, 52, 60), Color(54, 58, 67), Color(30, 33, 38), Color(70, 30, 28), Color(214, 218, 224),
     Color(80, 85, 94),
     Color(0, 0, 0, 0.6f),
     Color(180, 200, 255),
     4.f, 1.f, fnSans, fnSans, fnSans, 0.f, ksArc, lgClassic, false, false, false},
};
static constexpr int kNumThemes = (int)(sizeof(kThemes) / sizeof(kThemes[0]));
static constexpr int kDefaultSkin = 2; // NEON

} // namespace kn
