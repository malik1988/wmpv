#pragma once
#include <windows.h>
#include "util.h"

namespace ui {

// A compact design system shared by the window chrome, the empty-state
// welcome page and the owner-drawn subtitle menu.
struct Theme {
    bool dark = true;
    COLORREF panel = RGB(0, 0, 0);        // solid fallback / window brush
    COLORREF panelTop = RGB(0, 0, 0);     // bar gradient (top)
    COLORREF panelBottom = RGB(0, 0, 0);  // bar gradient (bottom)
    COLORREF surface = RGB(0, 0, 0);      // cards, menus, tooltips
    COLORREF surfaceAlt = RGB(0, 0, 0);   // hovered rows / raised surfaces
    COLORREF text = RGB(0, 0, 0);         // primary text
    COLORREF textDim = RGB(0, 0, 0);      // secondary text
    COLORREF textFaint = RGB(0, 0, 0);    // captions / hints
    COLORREF accent = RGB(0, 0, 0);       // brand gradient start
    COLORREF accent2 = RGB(0, 0, 0);      // brand gradient end
    COLORREF track = RGB(0, 0, 0);        // unfilled slider track
    COLORREF hover = RGB(0, 0, 0);        // hover mix target
    COLORREF icon = RGB(0, 0, 0);         // glyph color
    COLORREF line = RGB(0, 0, 0);         // hairlines / separators
    COLORREF canvasTop = RGB(0, 0, 0);    // empty-state gradient
    COLORREF canvasBottom = RGB(0, 0, 0);
};

inline Theme MakeDarkTheme() {
    Theme t;
    t.dark = true;
    t.panel = RGB(16, 18, 26);
    t.panelTop = RGB(26, 29, 41);
    t.panelBottom = RGB(13, 15, 22);
    t.surface = RGB(29, 32, 45);
    t.surfaceAlt = RGB(47, 52, 70);
    t.text = RGB(242, 244, 252);
    t.textDim = RGB(165, 172, 192);
    t.textFaint = RGB(113, 121, 143);
    t.accent = RGB(92, 138, 255);
    t.accent2 = RGB(170, 106, 255);
    t.track = RGB(60, 65, 84);
    t.hover = RGB(255, 255, 255);
    t.icon = RGB(226, 231, 246);
    t.line = RGB(52, 57, 75);
    t.canvasTop = RGB(22, 25, 38);
    t.canvasBottom = RGB(9, 10, 17);
    return t;
}

inline Theme MakeLightTheme() {
    Theme t;
    t.dark = false;
    t.panel = RGB(247, 248, 252);
    t.panelTop = RGB(255, 255, 255);
    t.panelBottom = RGB(238, 241, 249);
    t.surface = RGB(255, 255, 255);
    t.surfaceAlt = RGB(233, 237, 248);
    t.text = RGB(25, 29, 43);
    t.textDim = RGB(88, 96, 118);
    t.textFaint = RGB(138, 146, 166);
    t.accent = RGB(56, 108, 246);
    t.accent2 = RGB(139, 92, 246);
    t.track = RGB(216, 221, 236);
    t.hover = RGB(0, 0, 0);
    t.icon = RGB(42, 47, 65);
    t.line = RGB(222, 226, 239);
    t.canvasTop = RGB(253, 253, 255);
    t.canvasBottom = RGB(231, 236, 249);
    return t;
}

inline Theme GetSystemTheme() {
    Theme t = MakeDarkTheme();
    HKEY hk;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hk) == ERROR_SUCCESS) {
        DWORD val = 0, sz = sizeof(val);
        if (RegQueryValueExW(hk, L"AppsUseLightTheme", nullptr, nullptr,
                             (LPBYTE)&val, &sz) == ERROR_SUCCESS) {
            if (val != 0)
                t = MakeLightTheme();
        }
        RegCloseKey(hk);
    }
    return t;
}

} // namespace ui
