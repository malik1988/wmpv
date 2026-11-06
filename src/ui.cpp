#include "ui.h"
#include "i18n.h"
#include "version.h"
#include <cmath>
#include <cwchar>
#include <gdiplus.h>
using namespace Gdiplus;

static inline COLORREF blend(COLORREF a, COLORREF b, double t) {
    BYTE ar = GetRValue(a), ag = GetGValue(a), ab = GetBValue(a);
    BYTE br = GetRValue(b), bg = GetGValue(b), bb = GetBValue(b);
    return RGB((BYTE)(ar + (br - ar) * t),
               (BYTE)(ag + (bg - ag) * t),
               (BYTE)(ab + (bb - ab) * t));
}

static inline Color GC(COLORREF c, BYTE a = 255) {
    return Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static void RoundRectPath(GraphicsPath &p, REAL x, REAL y, REAL w, REAL h, REAL r) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r < 0) r = 0;
    REAL d = r * 2;
    p.StartFigure();
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

static void FillGradient(Graphics *g, const RectF &r, COLORREF a, COLORREF b, BYTE alpha = 255) {
    LinearGradientBrush br(r, GC(a, alpha), GC(b, alpha), LinearGradientModeVertical);
    g->FillRectangle(&br, r);
}

static void FillRound(Graphics *g, const RectF &r, REAL radius, Color c) {
    GraphicsPath p;
    RoundRectPath(p, r.X, r.Y, r.Width, r.Height, radius);
    SolidBrush b(c);
    g->FillPath(&b, &p);
}

static void FillGradientRound(Graphics *g, const RectF &r, REAL radius,
                              COLORREF a, COLORREF b, BYTE alpha = 255) {
    LinearGradientBrush br(r, GC(a, alpha), GC(b, alpha), LinearGradientModeVertical);
    GraphicsPath p;
    RoundRectPath(p, r.X, r.Y, r.Width, r.Height, radius);
    g->FillPath(&br, &p);
}

static void SoftShadow(Graphics *g, const RectF &r, REAL radius, bool dark) {
    BYTE a = (BYTE)(dark ? 70 : 34);
    for (int i = 3; i >= 1; i--) {
        RectF s(r.X - i, r.Y - i + 2, r.Width + i * 2, r.Height + i * 2);
        FillRound(g, s, radius + i, Color((BYTE)(a / (4 - i)), 0, 0, 0));
    }
}

static void DrawTextG(Graphics *g, const std::wstring &s, const RectF &r, REAL size,
                      bool bold, COLORREF color,
                      StringAlignment align = StringAlignmentNear, BYTE alpha = 255) {
    if (s.empty() || r.Width <= 0 || r.Height <= 0)
        return;
    Font font(util::UiFont(), size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    StringFormat fmt;
    fmt.SetAlignment(align);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    SolidBrush b(GC(color, alpha));
    g->DrawString(s.c_str(), (INT)s.size(), &font, r, &fmt, &b);
}

static REAL MeasureG(Graphics *g, const std::wstring &s, REAL size, bool bold) {
    Font font(util::UiFont(), size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    StringFormat fmt;
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    RectF layout(0, 0, 10000, 100), bounds;
    g->MeasureString(s.c_str(), (INT)s.size(), &font, layout, &fmt, &bounds);
    return bounds.Width;
}

void Chrome::compute() {
    int titleH = util::Scale(52, dpi_);
    int ctrlH = util::Scale(108, dpi_);
    int btn = util::Scale(38, dpi_);
    int play = util::Scale(44, dpi_);
    int pad = util::Scale(20, dpi_);
    int gap = util::Scale(6, dpi_);

    // Layout is fixed: the bars are floating popups, so their visibility must
    // never change the geometry (that would resize the mpv render target).
    titleRect_ = RECT{0, 0, cx_, titleH};
    int ctrlTop = cy_ - ctrlH;
    ctrlRect_ = RECT{0, ctrlTop, cx_, cy_};

    {
        int by = (titleH - btn) / 2;
        int bw = util::Scale(44, dpi_);
        int right = cx_ - util::Scale(10, dpi_);
        rClose_ = RECT{right - bw, by, right, by + btn};
        right = rClose_.left;
        rMax_ = RECT{right - bw, by, right, by + btn};
        right = rMax_.left;
        rMin_ = RECT{right - bw, by, right, by + btn};
        right = rMin_.left - gap;
        rOpen_ = RECT{right - btn, by, right, by + btn};
        int logo = util::Scale(30, dpi_);
        logoRect_ = RECT{pad, (titleH - logo) / 2, pad + logo, (titleH + logo) / 2};
    }

    {
        int seekH = util::Scale(5, dpi_);
        int seekY = ctrlTop + util::Scale(18, dpi_);
        seekRect_ = RECT{pad, seekY, cx_ - pad, seekY + seekH};

        int rowY = ctrlTop + util::Scale(42, dpi_);
        int x = pad - util::Scale(4, dpi_);
        int smallY = rowY + (play - btn) / 2;
        rPrev_ = RECT{x, smallY, x + btn, smallY + btn}; x = rPrev_.right + gap;
        rPlay_ = RECT{x, rowY, x + play, rowY + play}; x = rPlay_.right + gap;
        rNext_ = RECT{x, smallY, x + btn, smallY + btn};
        x = rNext_.right + gap + util::Scale(8, dpi_);

        int right = cx_ - pad;
        rFull_ = RECT{right - btn, smallY, right, smallY + btn};
        right = rFull_.left - gap;
        rSub_ = RECT{right - btn, smallY, right, smallY + btn};
        right = rSub_.left - gap * 2;
        int speedW = util::Scale(58, dpi_);
        rSpeed_ = RECT{right - speedW, smallY, right, smallY + btn};
        right = rSpeed_.left - gap * 2;
        int vtW = util::Scale(40, dpi_);
        volTextRect_ = RECT{right - vtW, rowY, right, rowY + play};
        right = volTextRect_.left - util::Scale(4, dpi_);
        int volW = util::Scale(88, dpi_);
        volRect_ = RECT{right - volW, rowY + (play - seekH) / 2,
                        right, rowY + (play - seekH) / 2 + seekH};
        right = volRect_.left - util::Scale(8, dpi_);
        rMute_ = RECT{right - btn, smallY, right, smallY + btn};
        timeRect_ = RECT{x, rowY, rMute_.left - gap, rowY + play};
    }
}

RECT Chrome::videoRect() const {
    return RECT{0, 0, cx_, cy_};
}

Ctrl Chrome::hitTest(int x, int y) const {
    POINT p = {x, y};
    if (titleRect_.bottom > 0) {
        if (PtInRect(&rOpen_, p)) return Ctrl::Open;
        if (PtInRect(&rClose_, p)) return Ctrl::Close;
        if (PtInRect(&rMax_, p)) return Ctrl::Max;
        if (PtInRect(&rMin_, p)) return Ctrl::Min;
        if (y < titleRect_.bottom) return Ctrl::Drag;
    }
    if (ctrlRect_.bottom > ctrlRect_.top) {
        if (PtInRect(&rPlay_, p)) return Ctrl::PlayPause;
        if (PtInRect(&rPrev_, p)) return Ctrl::Prev;
        if (PtInRect(&rNext_, p)) return Ctrl::Next;
        if (PtInRect(&rFull_, p)) return Ctrl::Fullscreen;
        if (PtInRect(&rSub_, p)) return Ctrl::Subtitle;
        if (PtInRect(&rSpeed_, p)) return Ctrl::Speed;
        if (PtInRect(&rMute_, p)) return Ctrl::Mute;
        if (y >= seekRect_.top - 10 && y <= seekRect_.bottom + 10 &&
            x >= seekRect_.left - 4 && x <= seekRect_.right + 4)
            return Ctrl::Seek;
        if (y >= volRect_.top - 8 && y <= volRect_.bottom + 8 &&
            x >= volRect_.left - 4 && x <= volRect_.right + 4)
            return Ctrl::Volume;
    }
    return Ctrl::None;
}

bool Chrome::hitEmptyButton(int x, int y) const {
    POINT p = {x, y};
    return PtInRect(&emptyBtn_, p) != FALSE;
}

double Chrome::seekFraction(int x) const {
    int w = seekRect_.right - seekRect_.left;
    if (w <= 0) return 0.0;
    double f = (double)(x - seekRect_.left) / w;
    if (f < 0) f = 0; if (f > 1) f = 1;
    return f;
}

double Chrome::volumeFraction(int x) const {
    int w = volRect_.right - volRect_.left;
    if (w <= 0) return 0.0;
    double f = (double)(x - volRect_.left) / w;
    if (f < 0) f = 0; if (f > 1) f = 1;
    return f;
}

int Chrome::menuItemWidth() const { return util::Scale(306, dpi_); }
int Chrome::menuItemHeight(int kind) const {
    if (kind == 1) return util::Scale(30, dpi_);
    if (kind == 2) return util::Scale(13, dpi_);
    return util::Scale(38, dpi_);
}

// ---------- shapes and icons ----------

void Chrome::drawLogo(Graphics *g, const RECT &r) {
    RectF rf((REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left), (REAL)(r.bottom - r.top));
    FillRound(g, RectF(rf.X - 3, rf.Y - 3, rf.Width + 6, rf.Height + 6),
              util::Scale(11, dpi_), GC(theme_.accent, 45));
    FillGradientRound(g, rf, util::Scale(9, dpi_), theme_.accent, theme_.accent2);
    REAL cx = rf.X + rf.Width / 2, cy = rf.Y + rf.Height / 2;
    REAL s = rf.Width * 0.16f;
    PointF pts[3] = {PointF(cx - s * 0.72f, cy - s), PointF(cx - s * 0.72f, cy + s),
                     PointF(cx + s, cy)};
    SolidBrush white(Color(255, 255, 255, 255));
    g->FillPolygon(&white, pts, 3);
}

void Chrome::drawHover(Graphics *g, const RECT &r, bool hover, bool danger) {
    if (!hover)
        return;
    RectF rf((REAL)r.left + 2, (REAL)r.top + 2,
             (REAL)(r.right - r.left) - 4, (REAL)(r.bottom - r.top) - 4);
    if (danger) {
        FillRound(g, rf, util::Scale(9, dpi_), GC(RGB(224, 66, 78)));
    } else {
        COLORREF c = blend(theme_.panelTop, theme_.hover, theme_.dark ? 0.15 : 0.07);
        FillRound(g, rf, util::Scale(9, dpi_), GC(c));
    }
}

void Chrome::drawPlay(Graphics *g, const RECT &r, COLORREF color) {
    int w = r.right - r.left, h = r.bottom - r.top;
    int cx = (r.left + r.right) / 2 + w / 24, cy = (r.top + r.bottom) / 2;
    int s = (int)(h * 0.20);
    Point pts[3] = {Point(cx - s, cy - s), Point(cx - s, cy + s),
                    Point(cx + (int)(s * 1.1), cy)};
    SolidBrush b(GC(color));
    g->FillPolygon(&b, pts, 3);
}

void Chrome::drawPause(Graphics *g, const RECT &r, COLORREF color) {
    int w = r.right - r.left, h = r.bottom - r.top;
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    int bw = (int)(w * 0.085);
    int bh = (int)(h * 0.34);
    SolidBrush b(GC(color));
    GraphicsPath p;
    RoundRectPath(p, (REAL)(cx - bw * 1.7), (REAL)(cy - bh / 2),
                  (REAL)(bw * 2), (REAL)bh, (REAL)bw);
    RoundRectPath(p, (REAL)(cx - bw * 0.3), (REAL)(cy - bh / 2),
                  (REAL)(bw * 2), (REAL)bh, (REAL)bw);
    g->FillPath(&b, &p);
}

void Chrome::drawSkip(Graphics *g, const RECT &r, bool next) {
    int w = r.right - r.left, h = r.bottom - r.top;
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    int s = (int)(h * 0.20);
    Pen pen(GC(theme_.icon), (REAL)(w * 0.075));
    pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    SolidBrush b(GC(theme_.icon));
    if (next) {
        Point pts[3] = {{cx - s, cy - s}, {cx - s, cy + s}, {cx + s, cy}};
        g->FillPolygon(&b, pts, 3);
        g->DrawLine(&pen, cx + s, cy - s, cx + s, cy + s);
    } else {
        Point pts[3] = {{cx + s, cy - s}, {cx + s, cy + s}, {cx - s, cy}};
        g->FillPolygon(&b, pts, 3);
        g->DrawLine(&pen, cx - s, cy - s, cx - s, cy + s);
    }
}

void Chrome::drawOpen(Graphics *g, const RECT &r) {
    int w = r.right - r.left, h = r.bottom - r.top;
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    int fw = (int)(w * 0.48), fh = (int)(h * 0.38);
    int l = cx - fw / 2, t = cy - fh / 2, rr = l + fw, btm = t + fh;
    int tab = (int)(fw * 0.32);
    Pen pen(GC(theme_.icon), (REAL)(w * 0.055));
    pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    GraphicsPath p;
    p.StartFigure();
    p.AddLine(l, t + (int)(fh * 0.30), l, btm);
    p.AddLine(l, btm, rr, btm);
    p.AddLine(rr, btm, rr, t + (int)(fh * 0.10));
    p.AddLine(rr, t + (int)(fh * 0.10), l + tab + (int)(fw * 0.10), t);
    p.AddLine(l + tab + (int)(fw * 0.10), t, l + tab, t + (int)(fh * 0.30));
    p.AddLine(l + tab, t + (int)(fh * 0.30), l, t + (int)(fh * 0.30));
    p.CloseFigure();
    g->DrawPath(&pen, &p);
}

void Chrome::drawMin(Graphics *g, const RECT &r) {
    int w = r.right - r.left;
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    int len = (int)(w * 0.24);
    Pen pen(GC(theme_.icon), (REAL)util::Scale(2, dpi_));
    pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    g->DrawLine(&pen, cx - len, cy, cx + len, cy);
}

void Chrome::drawMax(Graphics *g, const RECT &r, bool maxed) {
    int w = r.right - r.left;
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    int s = (int)(w * 0.16);
    Pen pen(GC(theme_.icon), (REAL)util::Scale(2, dpi_));
    pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    if (!maxed) {
        g->DrawRectangle(&pen, cx - s, cy - s, s * 2, s * 2);
    } else {
        int off = (int)(s * 0.34);
        g->DrawRectangle(&pen, cx - s, cy - s + off, s * 2 - off, s * 2 - off);
        g->DrawLine(&pen, cx - s + off, cy - s, cx + s, cy - s);
        g->DrawLine(&pen, cx + s, cy - s, cx + s, cy + s - off);
    }
}

void Chrome::drawClose(Graphics *g, const RECT &r, bool hover) {
    int w = r.right - r.left;
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    int s = (int)(w * 0.15);
    Pen pen(GC(hover ? RGB(255, 255, 255) : theme_.icon), (REAL)util::Scale(2, dpi_));
    pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    g->DrawLine(&pen, cx - s, cy - s, cx + s, cy + s);
    g->DrawLine(&pen, cx + s, cy - s, cx - s, cy + s);
}

void Chrome::drawFullscreen(Graphics *g, const RECT &r) {
    // Four diagonal arrows pointing outwards = "expand to full screen".
    // While in full screen they point inwards = "restore window".
    int w = r.right - r.left, h = r.bottom - r.top;
    int u = w < h ? w : h;
    REAL cx = (REAL)(r.left + r.right) / 2, cy = (REAL)(r.top + r.bottom) / 2;
    REAL outer = (REAL)u * 0.27f;
    REAL shaft = (REAL)u * 0.17f;
    REAL wing = (REAL)u * 0.10f;
    Pen pen(GC(theme_.icon), (REAL)util::Scale(2, dpi_));
    pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    for (int i = 0; i < 4; i++) {
        REAL sx = (i == 0 || i == 2) ? -1.0f : 1.0f;
        REAL sy = (i == 0 || i == 1) ? -1.0f : 1.0f;
        REAL xo = cx + sx * outer, yo = cy + sy * outer;
        REAL xi = cx + sx * (outer - shaft), yi = cy + sy * (outer - shaft);
        REAL tx = xo, ty = yo;                       // arrow head
        REAL bx = xi, by = yi;                       // arrow tail
        if (fullscreen_) {
            tx = xi; ty = yi;
            bx = xo; by = yo;
        }
        REAL back = fullscreen_ ? wing : -wing;   // wings trail away from the head
        g->DrawLine(&pen, bx, by, tx, ty);
        g->DrawLine(&pen, tx, ty, tx + sx * back, ty);
        g->DrawLine(&pen, tx, ty, tx, ty + sy * back);
    }
}

void Chrome::drawSubtitle(Graphics *g, const RECT &r, bool active) {
    int w = r.right - r.left, h = r.bottom - r.top;
    REAL bw = (REAL)(w * 0.52), bh = (REAL)(h * 0.42);
    RectF box((REAL)(r.left + r.right) / 2 - bw / 2,
              (REAL)(r.top + r.bottom) / 2 - bh / 2, bw, bh);
    COLORREF c = active ? theme_.accent : theme_.icon;
    Pen pen(GC(c, active ? 255 : 215), (REAL)util::Scale(2, dpi_));
    GraphicsPath p;
    RoundRectPath(p, box.X, box.Y, box.Width, box.Height, util::Scale(4, dpi_));
    g->DrawPath(&pen, &p);
    DrawTextG(g, L"CC", RectF(box.X, box.Y - 1, box.Width, box.Height),
              (REAL)util::Scale(9, dpi_), true, c, StringAlignmentCenter,
              active ? 255 : 215);
    if (active) {
        SolidBrush dot(GC(theme_.accent));
        g->FillEllipse(&dot, RectF(box.GetRight() - 2, box.Y - 2,
                                   (REAL)util::Scale(6, dpi_), (REAL)util::Scale(6, dpi_)));
    }
}

void Chrome::drawSpeed(Graphics *g, const RECT &r, double speed, bool hover) {
    RectF box((REAL)r.left + 2, (REAL)r.top + 2,
              (REAL)(r.right - r.left) - 4, (REAL)(r.bottom - r.top) - 4);
    bool fast = fabs(speed - 1.0) > 0.001;
    COLORREF c = (hover || fast) ? theme_.accent : theme_.icon;
    Pen pen(GC(c, fast || hover ? 235 : 190), 1.3f);
    GraphicsPath p;
    RoundRectPath(p, box.X, box.Y, box.Width, box.Height, (REAL)util::Scale(8, dpi_));
    g->DrawPath(&pen, &p);
    DrawTextG(g, util::FormatSpeed(speed), box, (REAL)util::Scale(11, dpi_),
              true, c, StringAlignmentCenter);
    if (fast) {
        SolidBrush dot(GC(theme_.accent));
        g->FillEllipse(&dot, RectF(box.GetRight() - 2, box.Y - 2,
                                   (REAL)util::Scale(6, dpi_), (REAL)util::Scale(6, dpi_)));
    }
}

void Chrome::drawVolume(Graphics *g, const RECT &r, bool muted) {
    // A complete speaker: small neck rectangle plus a four-point cone, with
    // two sound waves (or a cross) to the right of the mouth.
    int w = r.right - r.left, h = r.bottom - r.top;
    int u = w < h ? w : h;
    REAL cx = (REAL)(r.left + r.right) / 2, cy = (REAL)(r.top + r.bottom) / 2;
    Pen pen(GC(theme_.icon), (REAL)util::Scale(2, dpi_));
    pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    SolidBrush b(GC(theme_.icon));

    REAL neckL = cx - (REAL)u * 0.28f;
    REAL neckW = (REAL)u * 0.10f;
    REAL neckH = (REAL)u * 0.15f;
    REAL mouthX = neckL + neckW + (REAL)u * 0.14f;
    REAL mouthH = (REAL)u * 0.27f;
    g->FillRectangle(&b, RectF(neckL, cy - neckH / 2, neckW, neckH));
    PointF cone[4] = {
        PointF(neckL + neckW, cy - neckH / 2),
        PointF(mouthX, cy - mouthH),
        PointF(mouthX, cy + mouthH),
        PointF(neckL + neckW, cy + neckH / 2)
    };
    g->FillPolygon(&b, cone, 4);

    if (!muted) {
        REAL ax = neckL, ay = cy;
        REAL radii[2] = {(REAL)u * 0.40f, (REAL)u * 0.54f};
        for (int i = 0; i < 2; i++) {
            REAL rad = radii[i];
            g->DrawArc(&pen, ax - rad, ay - rad, rad * 2, rad * 2, -42, 84);
        }
    } else {
        REAL d = (REAL)u * 0.10f;
        REAL x0 = cx + (REAL)u * 0.12f;
        g->DrawLine(&pen, x0 - d, cy - d, x0 + d, cy + d);
        g->DrawLine(&pen, x0 + d, cy - d, x0 - d, cy + d);
    }
}

void Chrome::drawTrack(Graphics *g, const RECT &r, double frac, bool hot) {
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    REAL expand = hot ? util::Scale(1, dpi_) : 0;
    RectF rf((REAL)r.left, (REAL)r.top - expand,
             (REAL)(r.right - r.left), (REAL)(r.bottom - r.top) + expand * 2);
    REAL rad = rf.Height / 2;
    FillRound(g, rf, rad, GC(theme_.track, 205));
    REAL fw = (REAL)(rf.Width * frac);
    if (fw > 1) {
        GraphicsPath p;
        RoundRectPath(p, rf.X, rf.Y, fw, rf.Height, rad);
        LinearGradientBrush br(RectF(rf.X, rf.Y, fw, rf.Height),
                               GC(theme_.accent), GC(theme_.accent2),
                               LinearGradientModeHorizontal);
        g->FillPath(&br, &p);
    }
    REAL cr = (REAL)util::Scale(hot ? 7 : 5, dpi_);
    REAL hx = rf.X + fw, hy = rf.Y + rf.Height / 2;
    SolidBrush glow(GC(theme_.accent, 55));
    g->FillEllipse(&glow, RectF(hx - cr - 3, hy - cr - 3, cr * 2 + 6, cr * 2 + 6));
    SolidBrush knob(Color(255, 255, 255, 255));
    g->FillEllipse(&knob, RectF(hx - cr, hy - cr, cr * 2, cr * 2));
}

void Chrome::drawSeekTooltip(Graphics *g, int x) {
    if (seekHoverX_ < 0 || state_.duration <= 0)
        return;
    (void)x;
    std::wstring tip = util::FormatTime(seekFraction(seekHoverX_) * state_.duration);
    REAL fs = (REAL)util::Scale(12, dpi_);
    REAL tw = MeasureG(g, tip, fs, true) + util::Scale(22, dpi_);
    REAL th = (REAL)util::Scale(28, dpi_);
    REAL bx = (REAL)seekHoverX_ - tw / 2;
    REAL by = (REAL)seekRect_.top - th - util::Scale(8, dpi_);
    REAL margin = (REAL)util::Scale(8, dpi_);
    if (bx < margin) bx = margin;
    if (bx + tw > cx_ - margin) bx = (REAL)cx_ - margin - tw;
    RectF card(bx, by, tw, th);
    SoftShadow(g, card, util::Scale(7, dpi_), theme_.dark);
    FillRound(g, card, util::Scale(7, dpi_), GC(theme_.surface));
    GraphicsPath border;
    RoundRectPath(border, card.X, card.Y, card.Width, card.Height, util::Scale(7, dpi_));
    Pen bp(GC(theme_.line), 1);
    g->DrawPath(&bp, &border);
    DrawTextG(g, tip, card, fs, true, theme_.text, StringAlignmentCenter);
}

// ---------- window chrome ----------

// Both bars are rendered into a 32bpp PARGB bitmap, so the background can
// carry a real per-pixel alpha ramp while glyphs stay fully opaque.
static void GlassGradient(Graphics *g, const RectF &r, const ui::Theme &t, bool bottomBar) {
    (void)t;
    COLORREF glass = bottomBar ? RGB(22, 25, 36) : RGB(26, 29, 41);
    LinearGradientBrush br(r, Color(0, 0, 0, 0), Color(0, 0, 0, 0),
                           LinearGradientModeVertical);
    Color stops[3];
    REAL pos[3] = {0.0f, 0.42f, 1.0f};
    if (bottomBar) {
        stops[0] = GC(glass, 0);
        stops[1] = GC(glass, 178);
        stops[2] = GC(glass, 245);
    } else {
        stops[0] = GC(glass, 232);
        stops[1] = GC(glass, 205);
        stops[2] = GC(glass, 132);
    }
    br.SetInterpolationColors(stops, pos, 3);
    g->FillRectangle(&br, r);
}

void Chrome::paintTitleLayer(Graphics *g) {
    RectF bar(0, 0, (REAL)cx_, (REAL)titleRect_.bottom);
    GlassGradient(g, bar, theme_, false);
    SolidBrush line(GC(theme_.line, 110));
    g->FillRectangle(&line, RectF(0, (REAL)titleRect_.bottom - 1, (REAL)cx_, 1));

    drawLogo(g, logoRect_);

    std::wstring title = state_.title.empty() ? i18n::T(i18n::Str::AppTitle) : state_.title;
    std::wstring caption = state_.hasFile ? i18n::T(i18n::Str::CaptionPlaying)
                                          : i18n::T(i18n::Str::CaptionIdle);
    REAL tx = (REAL)logoRect_.right + util::Scale(12, dpi_);
    REAL tr = (REAL)rOpen_.left - util::Scale(14, dpi_);
    if (tr > tx) {
        DrawTextG(g, title,
                  RectF(tx, (REAL)util::Scale(6, dpi_), tr - tx, (REAL)util::Scale(24, dpi_)),
                  (REAL)util::Scale(13, dpi_), true, theme_.text);
        DrawTextG(g, caption,
                  RectF(tx, (REAL)util::Scale(28, dpi_), tr - tx, (REAL)util::Scale(18, dpi_)),
                  (REAL)util::Scale(10, dpi_), false, theme_.textFaint);
    }

    drawHover(g, rOpen_, hover_ == Ctrl::Open);
    drawHover(g, rMin_, hover_ == Ctrl::Min);
    drawHover(g, rMax_, hover_ == Ctrl::Max);
    drawHover(g, rClose_, hover_ == Ctrl::Close, true);

    drawOpen(g, rOpen_);
    drawMin(g, rMin_);
    drawMax(g, rMax_, maximized_);
    drawClose(g, rClose_, hover_ == Ctrl::Close);
}

void Chrome::paintControlsLayer(Graphics *g) {
    RectF bar(0, (REAL)ctrlRect_.top, (REAL)cx_, (REAL)(ctrlRect_.bottom - ctrlRect_.top));
    GlassGradient(g, bar, theme_, true);

    RectF hl(0, (REAL)ctrlRect_.top, (REAL)cx_, 2);
    LinearGradientBrush brush(hl, Color(0, 0, 0, 0), Color(0, 0, 0, 0),
                              LinearGradientModeHorizontal);
    Color stops[3] = {GC(theme_.accent, 0), GC(theme_.accent2, 90), GC(theme_.accent, 0)};
    REAL pos[3] = {0.0f, 0.5f, 1.0f};
    brush.SetInterpolationColors(stops, pos, 3);
    g->FillRectangle(&brush, hl);

    double frac = state_.duration > 0 ? state_.timePos / state_.duration : 0.0;
    drawTrack(g, seekRect_, frac, seekHoverX_ >= 0);

    drawHover(g, rPrev_, hover_ == Ctrl::Prev);
    drawHover(g, rNext_, hover_ == Ctrl::Next);
    drawHover(g, rMute_, hover_ == Ctrl::Mute);
    drawHover(g, rSub_, hover_ == Ctrl::Subtitle);
    drawHover(g, rSpeed_, hover_ == Ctrl::Speed);
    drawHover(g, rFull_, hover_ == Ctrl::Fullscreen);

    RectF playR((REAL)rPlay_.left, (REAL)rPlay_.top,
                (REAL)(rPlay_.right - rPlay_.left), (REAL)(rPlay_.bottom - rPlay_.top));
    SolidBrush glow(GC(theme_.accent, hover_ == Ctrl::PlayPause ? 70 : 42));
    g->FillEllipse(&glow, RectF(playR.X - 4, playR.Y - 4, playR.Width + 8, playR.Height + 8));
    LinearGradientBrush pb(playR, GC(theme_.accent), GC(theme_.accent2),
                           LinearGradientModeVertical);
    g->FillEllipse(&pb, playR);
    if (state_.paused)
        drawPlay(g, rPlay_, RGB(255, 255, 255));
    else
        drawPause(g, rPlay_, RGB(255, 255, 255));

    drawSkip(g, rPrev_, false);
    drawSkip(g, rNext_, true);
    drawVolume(g, rMute_, state_.mute);
    drawTrack(g, volRect_, state_.volume / 100.0, hover_ == Ctrl::Volume);
    drawSubtitle(g, rSub_, subtitleActive() && sub_.visible);
    drawSpeed(g, rSpeed_, state_.speed, hover_ == Ctrl::Speed);
    drawFullscreen(g, rFull_);

    REAL fs = (REAL)util::Scale(12, dpi_);
    if (timeRect_.right > timeRect_.left) {
        std::wstring now = util::FormatTime(state_.timePos);
        std::wstring total = L"  /  " + util::FormatTime(state_.duration);
        REAL w1 = MeasureG(g, now, fs, true) + 2;
        RectF tr((REAL)timeRect_.left, (REAL)timeRect_.top,
                 (REAL)(timeRect_.right - timeRect_.left), (REAL)(timeRect_.bottom - timeRect_.top));
        DrawTextG(g, now, RectF(tr.X, tr.Y, w1, tr.Height), fs, true, theme_.text);
        DrawTextG(g, total, RectF(tr.X + w1, tr.Y, tr.Width - w1, tr.Height),
                  fs, false, theme_.textFaint);
    }
    if (volTextRect_.right > volTextRect_.left) {
        std::wstring pct = std::to_wstring((int)(state_.volume + 0.5)) + L"%";
        DrawTextG(g, pct,
                  RectF((REAL)volTextRect_.left, (REAL)volTextRect_.top,
                        (REAL)(volTextRect_.right - volTextRect_.left),
                        (REAL)(volTextRect_.bottom - volTextRect_.top)),
                  (REAL)util::Scale(10, dpi_), false, theme_.textDim, StringAlignmentFar);
    }
    drawSeekTooltip(g, seekHoverX_);
}

// ---------- empty state ----------

void Chrome::drawChip(Graphics *g, int x, int y, const std::wstring &s, int *width) {
    REAL fs = (REAL)util::Scale(10, dpi_);
    int w = (int)MeasureG(g, s, fs, false) + util::Scale(24, dpi_);
    int h = util::Scale(26, dpi_);
    RectF r((REAL)x, (REAL)y, (REAL)w, (REAL)h);
    FillRound(g, r, h / 2.0f, GC(theme_.surface, theme_.dark ? 190 : 220));
    GraphicsPath p;
    RoundRectPath(p, r.X, r.Y, r.Width, r.Height, h / 2.0f);
    Pen border(GC(theme_.line, 170), 1);
    g->DrawPath(&border, &p);
    DrawTextG(g, s, r, fs, false, theme_.textDim, StringAlignmentCenter);
    *width = w;
}

void Chrome::paintEmpty(HDC hdc, const RECT &r) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    int W = r.right - r.left, H = r.bottom - r.top;
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;

    FillGradient(&g, RectF((REAL)r.left, (REAL)r.top, (REAL)W, (REAL)H),
                 theme_.canvasTop, theme_.canvasBottom);
    SolidBrush halo(GC(theme_.accent, theme_.dark ? 24 : 16));
    g.FillEllipse(&halo, RectF((REAL)(cx - util::Scale(230, dpi_)),
                               (REAL)(cy - util::Scale(230, dpi_)),
                               (REAL)util::Scale(460, dpi_), (REAL)util::Scale(400, dpi_)));
    SolidBrush halo2(GC(theme_.accent2, theme_.dark ? 18 : 12));
    g.FillEllipse(&halo2, RectF((REAL)(cx - util::Scale(160, dpi_)),
                                (REAL)(cy - util::Scale(40, dpi_)),
                                (REAL)util::Scale(320, dpi_), (REAL)util::Scale(260, dpi_)));

    int m = util::Scale(26, dpi_);
    if (W > m * 2 && H > m * 2) {
        Pen dash(GC(theme_.line, theme_.dark ? 170 : 210), 1.2f);
        dash.SetDashStyle(DashStyleDash);
        GraphicsPath frame;
        RoundRectPath(frame, (REAL)(r.left + m), (REAL)(r.top + m),
                      (REAL)(W - m * 2), (REAL)(H - m * 2), (REAL)util::Scale(16, dpi_));
        g.DrawPath(&dash, &frame);
    }

    int unit = util::Scale(1, dpi_);
    double k = H < util::Scale(440, dpi_) ? (double)H / util::Scale(440, dpi_) : 1.0;
    auto off = [&](int v) { return (int)(v * unit * k); };

    int logo = util::Scale(88, dpi_);
    RECT lr = {cx - logo / 2, cy - off(196) - logo / 2,
               cx + logo / 2, cy - off(196) + logo / 2};
    RectF lrf((REAL)lr.left, (REAL)lr.top, (REAL)logo, (REAL)logo);
    FillRound(&g, RectF(lrf.X - 9, lrf.Y - 9, lrf.Width + 18, lrf.Height + 18),
              util::Scale(34, dpi_), GC(theme_.accent, 34));
    FillGradientRound(&g, lrf, util::Scale(26, dpi_), theme_.accent, theme_.accent2);
    REAL s = lrf.Width * 0.15f;
    PointF tri[3] = {PointF(cx - s * 0.65f, cy - off(196) - s),
                     PointF(cx - s * 0.65f, cy - off(196) + s),
                     PointF(cx + s, (REAL)cy - off(196))};
    SolidBrush white(Color(255, 255, 255, 255));
    g.FillPolygon(&white, tri, 3);

    RECT t1 = {r.left, cy - off(112), r.right, cy - off(112) + util::Scale(40, dpi_)};
    DrawTextG(&g, i18n::T(i18n::Str::WelcomeTitle),
              RectF((REAL)t1.left, (REAL)t1.top, (REAL)(t1.right - t1.left), (REAL)(t1.bottom - t1.top)),
              (REAL)util::Scale(23, dpi_), true, theme_.text, StringAlignmentCenter);
    RECT t2 = {r.left, t1.bottom + util::Scale(4, dpi_), r.right,
               t1.bottom + util::Scale(4, dpi_) + util::Scale(26, dpi_)};
    DrawTextG(&g, i18n::T(i18n::Str::WelcomeSubtitle),
              RectF((REAL)t2.left, (REAL)t2.top, (REAL)(t2.right - t2.left), (REAL)(t2.bottom - t2.top)),
              (REAL)util::Scale(12, dpi_), false, theme_.textDim, StringAlignmentCenter);

    int bw = util::Scale(184, dpi_), bh = util::Scale(44, dpi_);
    emptyBtn_ = RECT{cx - bw / 2, t2.bottom + util::Scale(26, dpi_),
                     cx + bw / 2, t2.bottom + util::Scale(26, dpi_) + bh};
    RectF btn((REAL)emptyBtn_.left, (REAL)emptyBtn_.top, (REAL)bw, (REAL)bh);
    SoftShadow(&g, btn, util::Scale(12, dpi_), theme_.dark);
    COLORREF c1 = emptyHover_ ? blend(theme_.accent, RGB(255, 255, 255), 0.18) : theme_.accent;
    COLORREF c2 = emptyHover_ ? blend(theme_.accent2, RGB(255, 255, 255), 0.18) : theme_.accent2;
    FillGradientRound(&g, btn, util::Scale(12, dpi_), c1, c2);
    DrawTextG(&g, i18n::T(i18n::Str::OpenFile),
              RectF(btn.X, btn.Y - 1, btn.Width, btn.Height),
              (REAL)util::Scale(13, dpi_), true, RGB(255, 255, 255), StringAlignmentCenter);

    std::wstring chips[3] = {i18n::T(i18n::Str::ChipSpace),
                             i18n::T(i18n::Str::ChipOpen),
                             i18n::T(i18n::Str::ChipDrop)};
    int totalW = 0, widths[3] = {0, 0, 0};
    for (int i = 0; i < 3; i++) {
        widths[i] = (int)MeasureG(&g, chips[i], (REAL)util::Scale(10, dpi_), false) +
                    util::Scale(24, dpi_);
        totalW += widths[i];
    }
    totalW += util::Scale(8, dpi_) * 2;
    int chipX = cx - totalW / 2;
    int chipY = emptyBtn_.bottom + util::Scale(26, dpi_);
    for (int i = 0; i < 3; i++) {
        int w = 0;
        drawChip(&g, chipX, chipY, chips[i], &w);
        chipX += w + util::Scale(8, dpi_);
    }

    RECT t3 = {r.left, chipY + util::Scale(42, dpi_), r.right,
               chipY + util::Scale(42, dpi_) + util::Scale(24, dpi_)};
    DrawTextG(&g, i18n::T(i18n::Str::FormatsHint),
              RectF((REAL)t3.left, (REAL)t3.top, (REAL)(t3.right - t3.left), (REAL)(t3.bottom - t3.top)),
              (REAL)util::Scale(10, dpi_), false, theme_.textFaint, StringAlignmentCenter);

    std::wstring ver = std::wstring(L"v") + WMPV_VERSION_W;
    DrawTextG(&g, ver,
              RectF((REAL)(r.right - m - util::Scale(120, dpi_)),
                    (REAL)(r.bottom - m - util::Scale(24, dpi_)),
                    (REAL)util::Scale(110, dpi_), (REAL)util::Scale(20, dpi_)),
              (REAL)util::Scale(10, dpi_), false, theme_.textFaint, StringAlignmentFar);
}

// ---------- translucent rounded popup menu ----------

int Chrome::menuShadowPad() const { return util::Scale(20, dpi_); }
int Chrome::menuPadV() const { return util::Scale(8, dpi_); }
int Chrome::menuRadius() const { return util::Scale(14, dpi_); }

void Chrome::paintMenu(Graphics *g, const RECT &card, const MenuVisual *items,
                       int count, int hot, int scrollY, int viewH) {
    RectF cr((REAL)card.left, (REAL)card.top,
             (REAL)(card.right - card.left), (REAL)(card.bottom - card.top));
    REAL radius = (REAL)menuRadius();

    SoftShadow(g, cr, radius, theme_.dark);
    COLORREF top = theme_.dark ? blend(theme_.surface, theme_.surfaceAlt, 0.30)
                               : theme_.surface;
    COLORREF bottom = theme_.dark ? theme_.surface
                                  : blend(theme_.surface, theme_.line, 0.35);
    FillGradientRound(g, cr, radius, top, bottom, 232);

    GraphicsPath edge;
    RoundRectPath(edge, cr.X + 0.5f, cr.Y + 0.5f, cr.Width - 1, cr.Height - 1, radius);
    Pen border(GC(theme_.dark ? RGB(255, 255, 255) : RGB(0, 0, 0),
                  theme_.dark ? 48 : 24), 1);
    g->DrawPath(&border, &edge);

    REAL padV = (REAL)menuPadV();
    g->SetClip(RectF(cr.X + 2, cr.Y + padV, cr.Width - 4, (REAL)viewH));
    g->TranslateTransform(0, -(REAL)scrollY);
    int y = card.top + (int)padV;
    for (int i = 0; i < count; i++) {
        int hh = menuItemHeight(items[i].kind);
        RECT row = {card.left, y, card.right, y + hh};
        paintMenuItem(g, row, items[i], i == hot, false);
        y += hh;
    }
    g->ResetTransform();
    g->ResetClip();

    int contentH = y - card.top - (int)padV;
    if (contentH > viewH) {
        SolidBrush arrow(GC(theme_.textFaint, 170));
        REAL ax = cr.X + cr.Width / 2;
        if (scrollY > 0) {
            PointF t[3] = {PointF(ax - 4, cr.Y + padV + 3),
                           PointF(ax + 4, cr.Y + padV + 3),
                           PointF(ax, cr.Y + padV - 2)};
            g->FillPolygon(&arrow, t, 3);
        }
        if (scrollY < contentH - viewH) {
            REAL by = cr.Y + padV + viewH - 2;
            PointF t[3] = {PointF(ax - 4, by + 2), PointF(ax + 4, by + 2),
                           PointF(ax, by + 7)};
            g->FillPolygon(&arrow, t, 3);
        }
    }
}

void Chrome::paintMenuItem(Graphics *g, const RECT &rc, const MenuVisual &v,
                           bool selected, bool disabled) {
    // No opaque row fill: the translucent card painted by paintMenu() shows through.
    RectF r((REAL)rc.left, (REAL)rc.top, (REAL)(rc.right - rc.left),
            (REAL)(rc.bottom - rc.top));
    REAL padL = (REAL)util::Scale(14, dpi_);
    if (v.kind == 2) {
        Pen sep(GC(theme_.line, 150), 1);
        g->DrawLine(&sep, r.X + padL, r.Y + r.Height / 2,
                    r.GetRight() - padL, r.Y + r.Height / 2);
        return;
    }
    if (v.kind == 1) {
        DrawTextG(g, v.text,
                  RectF(r.X + padL, r.Y, r.Width - padL * 2, r.Height),
                  (REAL)util::Scale(10, dpi_), true, theme_.textFaint);
        return;
    }

    REAL inset = (REAL)util::Scale(6, dpi_);
    if (selected) {
        FillRound(g, RectF(r.X + inset, r.Y + 1, r.Width - inset * 2, r.Height - 2),
                  (REAL)util::Scale(8, dpi_),
                  GC(theme_.hover, theme_.dark ? 26 : 16));
        REAL barW = (REAL)util::Scale(3, dpi_);
        FillRound(g, RectF(r.X + (REAL)util::Scale(3, dpi_),
                           r.Y + r.Height * 0.26f, barW, r.Height * 0.48f),
                  barW / 2, GC(theme_.accent, 235));
    }

    REAL d = (REAL)util::Scale(13, dpi_);
    REAL markY = r.Y + (r.Height - d) / 2;
    if (v.kind == 3) {
        Pen ring(GC(v.checked ? theme_.accent : theme_.textFaint, 220), 1.3f);
        g->DrawEllipse(&ring, RectF(r.X + padL, markY, d, d));
        if (v.checked) {
            SolidBrush dot(GC(theme_.accent));
            g->FillEllipse(&dot, RectF(r.X + padL + d * 0.28, markY + d * 0.28,
                                       d * 0.44, d * 0.44));
        }
    } else if (v.kind == 4) {
        Pen box(GC(v.checked ? theme_.accent : theme_.textFaint, 220), 1.3f);
        GraphicsPath bp;
        RoundRectPath(bp, r.X + padL, markY, d, d, 3);
        g->DrawPath(&box, &bp);
        if (v.checked) {
            Pen tick(GC(theme_.accent), 2.0f);
            tick.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
            g->DrawLine(&tick, r.X + padL + 3, markY + d * 0.55,
                        r.X + padL + d * 0.43, markY + d * 0.78);
            g->DrawLine(&tick, r.X + padL + d * 0.43, markY + d * 0.78,
                        r.X + padL + d - 2, markY + 2);
        }
    }

    REAL tx = r.X + (REAL)util::Scale(36, dpi_);
    REAL hintW = (REAL)util::Scale(96, dpi_);
    REAL textW = r.GetRight() - tx - (v.hint.empty() ? padL : hintW + padL);
    DrawTextG(g, v.text, RectF(tx, r.Y, textW, r.Height),
              (REAL)util::Scale(12, dpi_), false,
              disabled ? theme_.textFaint : theme_.text);
    if (!v.hint.empty()) {
        DrawTextG(g, v.hint,
                  RectF(r.GetRight() - hintW - padL, r.Y, hintW, r.Height),
                  (REAL)util::Scale(10, dpi_), false, theme_.textFaint, StringAlignmentFar);
    }
}
