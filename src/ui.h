#pragma once
#include <windows.h>
#include <string>
#include "theme.h"
#include "player.h"

namespace Gdiplus { class Graphics; }

enum class Ctrl {
    None,
    Drag,       // title area -> drag window
    PlayPause,
    Prev,
    Next,
    Open,
    Min,
    Max,
    Close,
    Fullscreen,
    Subtitle,
    Speed,
    Mute,
    Volume,
    Seek
};

// Visual description of one owner-drawn popup-menu row.
struct MenuVisual {
    std::wstring text;
    std::wstring hint;
    int kind = 0;       // 0 item, 1 header, 2 separator, 3 radio, 4 check
    bool checked = false;
};

class Chrome {
public:
    void setDpi(UINT dpi) { dpi_ = dpi; }
    void setTheme(const ui::Theme &t) { theme_ = t; }
    void resize(int cx, int cy) { cx_ = cx; cy_ = cy; compute(); }
    void setMode(bool fullscreen, bool bars) { fullscreen_ = fullscreen; bars_ = bars; compute(); }
    void setMaximized(bool m) { maximized_ = m; }

    void setState(const PlayerState &s) { state_ = s; compute(); }
    void setSubtitle(const SubtitleInfo &s) { sub_ = s; }
    void setHover(Ctrl c) { hover_ = c; }
    void setSeekHover(int x) { seekHoverX_ = x; }   // -1 = not hovering
    void setEmptyHover(bool h) { emptyHover_ = h; }
    bool emptyHover() const { return emptyHover_; }
    bool hitEmptyButton(int x, int y) const;

    RECT controlsRect() const { return ctrlRect_; }
    RECT subtitleButtonRect() const { return rSub_; }
    RECT speedButtonRect() const { return rSpeed_; }
    int titleBarHeight() const { return titleRect_.bottom; }
    int controlsHeight() const { return ctrlRect_.bottom - ctrlRect_.top; }
    bool subtitleActive() const { return sub_.sid != 0 && !sub_.tracks.empty(); }

    // The render target always covers the whole client area; the two bars are
    // separate transparent popups and never resize it.
    RECT videoRect() const;

    // Both bars are painted into a 32bpp PARGB bitmap owned by the layer window.
    void paintTitleLayer(Gdiplus::Graphics *g);
    void paintControlsLayer(Gdiplus::Graphics *g);
    void paintEmpty(HDC hdc, const RECT &r);
    void paintMenuItem(HDC hdc, const RECT &r, const MenuVisual &v, bool selected, bool disabled);
    int menuItemWidth() const;
    int menuItemHeight(int kind) const;
    Ctrl hitTest(int x, int y) const;
    double seekFraction(int x) const;
    double volumeFraction(int x) const;

private:
    void compute();
    void drawLogo(Gdiplus::Graphics *g, const RECT &r);
    void drawHover(Gdiplus::Graphics *g, const RECT &r, bool hover, bool danger = false);
    void drawPlay(Gdiplus::Graphics *g, const RECT &r, COLORREF color);
    void drawPause(Gdiplus::Graphics *g, const RECT &r, COLORREF color);
    void drawSkip(Gdiplus::Graphics *g, const RECT &r, bool next);
    void drawOpen(Gdiplus::Graphics *g, const RECT &r);
    void drawMin(Gdiplus::Graphics *g, const RECT &r);
    void drawMax(Gdiplus::Graphics *g, const RECT &r, bool maxed);
    void drawClose(Gdiplus::Graphics *g, const RECT &r, bool hover);
    void drawFullscreen(Gdiplus::Graphics *g, const RECT &r);
    void drawSubtitle(Gdiplus::Graphics *g, const RECT &r, bool active);
    void drawSpeed(Gdiplus::Graphics *g, const RECT &r, double speed, bool hover);
    void drawVolume(Gdiplus::Graphics *g, const RECT &r, bool muted);
    void drawTrack(Gdiplus::Graphics *g, const RECT &r, double frac, bool hot);
    void drawSeekTooltip(Gdiplus::Graphics *g, int x);
    void drawChip(Gdiplus::Graphics *g, int x, int y, const std::wstring &s, int *width);

    int cx_ = 0, cy_ = 0, dpi_ = 96;
    bool fullscreen_ = false, bars_ = true, maximized_ = false;
    bool emptyHover_ = false;
    ui::Theme theme_;
    PlayerState state_;
    SubtitleInfo sub_;
    Ctrl hover_ = Ctrl::None;
    int seekHoverX_ = -1;

    RECT titleRect_ = {0};
    RECT ctrlRect_ = {0};
    RECT seekRect_ = {0};
    RECT volRect_ = {0};
    RECT rOpen_ = {0}, rPlay_ = {0}, rPrev_ = {0}, rNext_ = {0};
    RECT rFull_ = {0}, rMute_ = {0}, rSub_ = {0}, rSpeed_ = {0};
    RECT rMin_ = {0}, rMax_ = {0}, rClose_ = {0};
    RECT logoRect_ = {0}, timeRect_ = {0}, volTextRect_ = {0};
    RECT emptyBtn_ = {0};
};
