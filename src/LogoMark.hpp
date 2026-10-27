// Kinetik — the "Orbit" mark (identical in kinetik-kdsp16 and kinetik-kmsp6; J-Slice and KDYN-10 draw the same
// geometry with their own toolkits). Drawn in a 120 x 120 box: rounded square (6..114, radius 26), ring r 30 /
// stroke 9 and core r 11 at the centre, satellite r 10 at (84, 38).
#pragma once
#include "Skins.hpp"

namespace kn {

struct MarkColors { Color badge, ink, sat; bool glow; };

// one colour set per skin family: red badge everywhere, except RISO (black ink badge on the red paper)
inline MarkColors markColors(LogoStyle s)
{
    switch (s) {
    case lgRiso: return {Color(24, 16, 14), Color(214, 69, 43), Color(255, 244, 236), false};
    case lgSerif: return {Color(222, 38, 44), Color(30, 34, 58), Color(237, 230, 214), false};   // MIDNIGHT
    case lgClassic: return {Color(228, 32, 47), Color(22, 24, 28), Color(255, 255, 255), false};
    case lgNeon: default: return {Color(228, 32, 47), Color(9, 11, 26), Color(255, 255, 255), true};
    }
}

// V = a DGL NanoVG widget; (x, y) = top-left corner, size = side of the 120-unit box
template <class V>
inline void drawOrbitMark(V& v, float x, float y, float size, LogoStyle style)
{
    const MarkColors c = markColors(style);
    const float u = size / 120.f;
    if (c.glow) {
        v.beginPath(); v.rect(x - 14.f * u, y - 14.f * u, 148.f * u, 148.f * u);
        v.fillPaint(v.boxGradient(x + 6.f * u, y + 6.f * u, 108.f * u, 108.f * u, 26.f * u, 30.f * u,
                                   Color(c.badge, Color(0, 0, 0, 0.f), 0.45f), Color(0, 0, 0, 0.f)));
        v.fill();
    }
    v.beginPath(); v.roundedRect(x + 6.f * u, y + 6.f * u, 108.f * u, 108.f * u, 26.f * u); v.fillColor(c.badge); v.fill();
    v.beginPath(); v.circle(x + 60.f * u, y + 60.f * u, 30.f * u); v.strokeColor(c.ink); v.strokeWidth(9.f * u); v.stroke();
    v.beginPath(); v.circle(x + 60.f * u, y + 60.f * u, 11.f * u); v.fillColor(c.ink); v.fill();
    v.beginPath(); v.circle(x + 84.f * u, y + 38.f * u, 10.f * u); v.fillColor(c.sat); v.fill();
}

} // namespace kn
