#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <vector>
#include <string>
#include <cwchar>
#include <cstring>
#include <cmath>
#include <initializer_list>
#include <utility>
#include "player.h"
#include "ui.h"
#include "util.h"
#include "theme.h"
#include "i18n.h"
#include <gdiplus.h>
using namespace Gdiplus;

#define WM_APP_FWD_MOUSE    (WM_APP + 10)
#define WM_APP_MOUSE_LEAVE  (WM_APP + 12)
#define IDT_CLICK 2
#define IDI_ICON1 101

enum : UINT {
    ID_SUB_OPEN = 1,
    ID_SUB_OFF = 2,
    ID_SUB_VISIBLE = 3,
    ID_SUB_DELAY_MINUS = 4,
    ID_SUB_DELAY_PLUS = 5,
    ID_SUB_DELAY_RESET = 6,
    ID_SUB_FONT_MINUS = 7,
    ID_SUB_FONT_PLUS = 8,
    ID_SUB_TRACK = 1000,
    ID_SPEED_PRESET = 2000,
    ID_SPEED_MINUS = 2101,
    ID_SPEED_PLUS = 2102,
    ID_SPEED_RESET = 2103,
    ID_LANG_AUTO = 3001,
    ID_LANG_ZH = 3002,
    ID_LANG_EN = 3003
};

struct MenuItem {
    MenuVisual v;
    UINT id = 0;
};

class App {
public:
    HWND hwnd = nullptr, videoHost = nullptr;
    HWND titleLayer = nullptr, ctrlLayer = nullptr;
    MpvPlayer player;
    Chrome chrome;
    ui::Theme theme;
    std::vector<std::wstring> playlist;
    std::vector<std::wstring> pendingSubs;
    std::vector<MenuItem> menuItems;
    size_t cur = 0;
    bool fullscreen = false;
    bool seekDrag = false, volDrag = false;
    bool pendingClick = false;
    bool cursorHidden = false;
    bool menuOpen = false;
    bool videoShown = false;
    bool titleVisible = false, ctrlVisible = false;
    RECT savedRect = {0};
    RECT lastVideoRect = {0, 0, 0, 0};
    Ctrl lastHover = Ctrl::None;
    int lastSeekHover = -1;
    int lastMouseX = -1, lastMouseY = -1;
    UINT dpi = 96;

    HDC hMem = nullptr;
    HBITMAP hBmp = nullptr;
    int memW = 0, memH = 0;

    void onCreate(HWND w);
    void onSize(int cx, int cy);
    void onPaint(HDC hdc);
    void openFiles();
    void openSubtitleFiles();
    void showSubtitleMenu();
    void showSpeedMenu();
    void showLanguageMenu();
    int runMenu(const std::vector<MenuItem> &items, const RECT &anchor);
    void loadPaths(const std::vector<std::wstring> &paths);
    void applyPendingSubs();
    void loadCurrent();
    void playIndex(size_t i);
    void toggleFullscreen();

    void syncLayers();
    void renderLayer(bool title);
    void refreshLayers();
    void setLayerVisible(bool title, bool show);
    void updateLayers(int x, int y);
    void onMouseLeave();
    void handleMouseMove(int x, int y);
    void handleClick(int x, int y);
    void endDrag();
    void repositionVideo();
    void doSeek(int x);
    void doVolume(int x);
};

static App *g_app = nullptr;

static std::vector<std::wstring> ParseOpenFiles(wchar_t *buf, UINT offset) {
    std::vector<std::wstring> files;
    if (offset == 0) {
        wchar_t *p = buf;
        std::wstring dir(p);
        p += wcslen(p) + 1;
        while (*p) {
            files.push_back(dir + L"\\" + std::wstring(p));
            p += wcslen(p) + 1;
        }
        if (files.empty())
            files.push_back(dir);
    } else {
        files.push_back(std::wstring(buf));
    }
    return files;
}

void App::onCreate(HWND w) {
    hwnd = w;
    dpi = util::GetDpi(w);
    theme = ui::GetSystemTheme();
    chrome.setDpi(dpi);
    chrome.setTheme(theme);

    util::InitDwm();
    util::SetCornerPreference(hwnd, DWMWCP_ROUND);
    util::SetImmersiveDarkMode(hwnd, theme.dark);
    util::SetMicaBackdrop(hwnd, DWMSBT_MAINWINDOW);

    videoHost = CreateWindowExW(0, L"WMPVVIDEO", nullptr,
                                WS_CHILD, 0, 0, 10, 10,
                                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
    DWORD ex = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    titleLayer = CreateWindowExW(ex, L"WMPVLAYER", L"", WS_POPUP,
                                 0, 0, 1, 1, hwnd, nullptr, hInst, nullptr);
    ctrlLayer = CreateWindowExW(ex, L"WMPVLAYER", L"", WS_POPUP,
                                0, 0, 1, 1, hwnd, nullptr, hInst, nullptr);

    DragAcceptFiles(hwnd, TRUE);

    HICON icon = LoadIconW(hInst, MAKEINTRESOURCE(IDI_ICON1));
    if (icon) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    }

    RECT rc;
    GetClientRect(hwnd, &rc);
    chrome.resize(rc.right, rc.bottom);

    if (!player.init(videoHost, hwnd)) {
        MessageBoxW(hwnd, i18n::T(i18n::Str::InitError),
                    L"wmpv", MB_OK | MB_ICONWARNING);
    }

    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        std::vector<std::wstring> paths;
        for (int i = 1; i < argc; i++)
            if (wcsncmp(argv[i], L"--", 2) != 0)   // 跳过 --lang= 等开关
                paths.push_back(argv[i]);
        if (!paths.empty())
            loadPaths(paths);
    }
    LocalFree(argv);

    repositionVideo();
    syncLayers();
    updateLayers(-1, -1);
    refreshLayers();
}

void App::onSize(int cx, int cy) {
    chrome.resize(cx, cy);
    repositionVideo();
    InvalidateRect(hwnd, nullptr, TRUE);
}

void App::repositionVideo() {
    if (!videoHost)
        return;
    bool show = player.getState().hasFile;
    if (show != videoShown) {
        ShowWindow(videoHost, show ? SW_SHOW : SW_HIDE);
        videoShown = show;
        InvalidateRect(hwnd, nullptr, TRUE);
        updateLayers(lastMouseX, lastMouseY);
    }
    RECT vr = chrome.videoRect();
    if (vr.left == lastVideoRect.left && vr.top == lastVideoRect.top &&
        vr.right == lastVideoRect.right && vr.bottom == lastVideoRect.bottom)
        return;
    lastVideoRect = vr;
    SetWindowPos(videoHost, nullptr, vr.left, vr.top,
                 vr.right - vr.left, vr.bottom - vr.top, SWP_NOZORDER);
}

void App::onPaint(HDC hdc) {
    if (!player.getState().hasFile)
        chrome.paintEmpty(hdc, chrome.videoRect());
}

// ---------- transparent floating bars ----------

void App::renderLayer(bool title) {
    HWND layer = title ? titleLayer : ctrlLayer;
    if (!layer)
        return;
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right;
    // Extra transparent headroom above the control bar keeps the seek tooltip
    // inside the layer window (it must not be clipped).
    int extra = util::Scale(40, dpi);
    int h = title ? chrome.titleBarHeight() : chrome.controlsHeight() + extra;
    if (w <= 0 || h <= 0)
        return;

    Bitmap bmp(w, h, PixelFormat32bppPARGB);
    {
        // Over video, a dark frosted HUD reads far better than a light bar;
        // the welcome page and menus keep following the system theme.
        ui::Theme saved = theme;
        if (player.getState().hasFile)
            chrome.setTheme(ui::MakeDarkTheme());
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.Clear(Color(0, 0, 0, 0));
        if (title) {
            chrome.paintTitleLayer(&g);
        } else {
            g.TranslateTransform(0, -(REAL)(chrome.controlsRect().top - extra));
            chrome.paintControlsLayer(&g);
        }
        chrome.setTheme(saved);
    }

    BitmapData data;
    Rect lockRect(0, 0, w, h);
    if (bmp.LockBits(&lockRect, ImageLockModeRead,
                     PixelFormat32bppPARGB, &data) != Ok)
        return;

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP dib = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib && bits) {
        HBITMAP old = (HBITMAP)SelectObject(mem, dib);
        for (int y = 0; y < h; y++)
            memcpy((BYTE *)bits + (size_t)y * w * 4,
                   (BYTE *)data.Scan0 + (LONG_PTR)y * data.Stride, (size_t)w * 4);
        SIZE size = {w, h};
        POINT src = {0, 0};
        BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(layer, screen, nullptr, &size, mem, &src, 0, &bf, ULW_ALPHA);
        SelectObject(mem, old);
        DeleteObject(dib);
    }
    bmp.UnlockBits(&data);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
}

void App::syncLayers() {
    if (!titleLayer || !ctrlLayer)
        return;
    RECT rc;
    GetClientRect(hwnd, &rc);
    POINT p = {0, 0};
    ClientToScreen(hwnd, &p);
    int th = chrome.titleBarHeight();
    int ch = chrome.controlsHeight() + util::Scale(40, dpi);
    SetWindowPos(titleLayer, nullptr, p.x, p.y, rc.right, th,
                 SWP_NOACTIVATE | SWP_NOZORDER);
    SetWindowPos(ctrlLayer, nullptr, p.x, p.y + rc.bottom - ch, rc.right, ch,
                 SWP_NOACTIVATE | SWP_NOZORDER);
    refreshLayers();
}

void App::refreshLayers() {
    if (titleVisible)
        renderLayer(true);
    if (ctrlVisible)
        renderLayer(false);
}

void App::setLayerVisible(bool title, bool show) {
    HWND layer = title ? titleLayer : ctrlLayer;
    bool &flag = title ? titleVisible : ctrlVisible;
    if (!layer || flag == show)
        return;
    flag = show;
    if (show) {
        renderLayer(title);
        ShowWindow(layer, SW_SHOWNOACTIVATE);
    } else {
        ShowWindow(layer, SW_HIDE);
    }
}

void App::updateLayers(int x, int y) {
    bool media = player.getState().hasFile;
    RECT rc;
    GetClientRect(hwnd, &rc);
    bool inside = x >= 0 && y >= 0 && x < rc.right && y < rc.bottom;
    bool onTitle = inside && y < chrome.titleBarHeight();
    bool onCtrl = inside && y >= chrome.controlsRect().top;

    bool wantTitle = !media || onTitle;
    bool wantCtrl = media && (onCtrl || menuOpen || seekDrag || volDrag);
    setLayerVisible(true, wantTitle);
    setLayerVisible(false, wantCtrl);

    if (fullscreen) {
        bool clean = media && !wantTitle && !wantCtrl && !cursorHidden;
        if (clean) {
            ShowCursor(FALSE);
            cursorHidden = true;
        } else if ((wantTitle || wantCtrl) && cursorHidden) {
            ShowCursor(TRUE);
            cursorHidden = false;
        }
    } else if (cursorHidden) {
        ShowCursor(TRUE);
        cursorHidden = false;
    }
}

void App::onMouseLeave() {
    lastMouseX = lastMouseY = -1;
    if (lastHover != Ctrl::None || lastSeekHover != -1) {
        lastHover = Ctrl::None;
        lastSeekHover = -1;
        chrome.setHover(Ctrl::None);
        chrome.setSeekHover(-1);
        refreshLayers();
    }
    if (player.getState().hasFile && !menuOpen) {
        setLayerVisible(true, false);
        setLayerVisible(false, false);
        if (fullscreen && !cursorHidden) {
            ShowCursor(FALSE);
            cursorHidden = true;
        }
    }
}

void App::handleMouseMove(int x, int y) {
    lastMouseX = x;
    lastMouseY = y;
    updateLayers(x, y);

    bool media = player.getState().hasFile;
    bool emptyH = !media && chrome.hitEmptyButton(x, y);
    if (emptyH != chrome.emptyHover()) {
        chrome.setEmptyHover(emptyH);
        InvalidateRect(hwnd, nullptr, TRUE);
    }
    if (seekDrag)
        doSeek(x);
    else if (volDrag)
        doVolume(x);

    Ctrl c = chrome.hitTest(x, y);
    bool titleZone = y < chrome.titleBarHeight();
    if (!media && !titleZone)
        c = Ctrl::None;
    if (titleZone && !titleVisible)
        c = Ctrl::None;
    if (!titleZone && !ctrlVisible)
        c = Ctrl::None;

    int newSeek = (c == Ctrl::Seek) ? x : -1;
    if (c != lastHover || newSeek != lastSeekHover) {
        lastHover = c;
        lastSeekHover = newSeek;
        chrome.setHover(c);
        chrome.setSeekHover(newSeek);
        refreshLayers();
    }
}

void App::handleClick(int x, int y) {
    bool media = player.getState().hasFile;
    if (!media && chrome.hitEmptyButton(x, y)) {
        openFiles();
        return;
    }
    Ctrl c = chrome.hitTest(x, y);
    bool titleZone = y < chrome.titleBarHeight();
    if (!media && !titleZone)
        c = Ctrl::None;
    if (titleZone && !titleVisible)
        c = Ctrl::None;
    if (!titleZone && !ctrlVisible)
        c = Ctrl::None;

    switch (c) {
    case Ctrl::Open: openFiles(); break;
    case Ctrl::Min: PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0); break;
    case Ctrl::Max:
        if (fullscreen)
            toggleFullscreen();
        else
            PostMessageW(hwnd, WM_SYSCOMMAND, IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
        break;
    case Ctrl::Close: PostMessageW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0); break;
    case Ctrl::PlayPause: player.playPause(); break;
    case Ctrl::Prev: playIndex(cur > 0 ? cur - 1 : 0); break;
    case Ctrl::Next: playIndex(cur + 1 < playlist.size() ? cur + 1 : cur); break;
    case Ctrl::Fullscreen: toggleFullscreen(); break;
    case Ctrl::Subtitle: showSubtitleMenu(); break;
    case Ctrl::Speed: showSpeedMenu(); break;
    case Ctrl::Mute: { PlayerState s = player.getState(); player.setMute(!s.mute); break; }
    case Ctrl::Seek: seekDrag = true; doSeek(x); break;
    case Ctrl::Volume: volDrag = true; doVolume(x); break;
    default: break;
    }
}

void App::endDrag() {
    seekDrag = false;
    volDrag = false;
    updateLayers(lastMouseX, lastMouseY);
}

void App::doSeek(int x) {
    PlayerState s = player.getState();
    if (s.duration > 0)
        player.seek(chrome.seekFraction(x) * s.duration);
}

void App::doVolume(int x) {
    player.setVolume(chrome.volumeFraction(x) * 100.0);
}

// ---------- files & playlists ----------

static const wchar_t *kMediaFilter =
    L"*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.flv;*.webm;*.mpg;*.mpeg;*.m4v;*.ts;"
    L"*.mp3;*.m4a;*.flac;*.wav;*.ogg;*.aac";
static const wchar_t *kSubFilter =
    L"*.srt;*.ass;*.ssa;*.sub;*.vtt;*.smi;*.rt;*.mpl2;*.idx;*.sbv;*.lrc;*.scc;*.stl";

static std::wstring BuildFilter(std::initializer_list<std::pair<const wchar_t *, std::wstring>> parts) {
    std::wstring out;
    for (const auto &part : parts) {
        out += part.first;
        out.push_back(L'\0');
        out += part.second;
        out.push_back(L'\0');
    }
    return out;
}

void App::openFiles() {
    wchar_t buf[65536] = {0};
    std::wstring filter = BuildFilter({
        {i18n::T(i18n::Str::FilterMediaSubs), std::wstring(kMediaFilter) + L";" + kSubFilter},
        {i18n::T(i18n::Str::FilterMedia), kMediaFilter},
        {i18n::T(i18n::Str::FilterSubs), kSubFilter},
        {i18n::T(i18n::Str::FilterAll), L"*.*"}
    });
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = 65536;
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn))
        loadPaths(ParseOpenFiles(buf, ofn.nFileOffset));
}

void App::openSubtitleFiles() {
    wchar_t buf[65536] = {0};
    std::wstring filter = BuildFilter({{i18n::T(i18n::Str::FilterSubs), kSubFilter},
                                       {i18n::T(i18n::Str::FilterAll), L"*.*"}});
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = 65536;
    ofn.lpstrFilter = filter.c_str();
    ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn))
        loadPaths(ParseOpenFiles(buf, ofn.nFileOffset));
}

void App::loadPaths(const std::vector<std::wstring> &paths) {
    std::vector<std::wstring> media, subs;
    for (const std::wstring &p : paths) {
        if (util::IsSubtitlePath(p))
            subs.push_back(p);
        else
            media.push_back(p);
    }
    if (!media.empty()) {
        playlist = media;
        cur = 0;
        pendingSubs = subs;
        loadCurrent();
        return;
    }
    if (subs.empty())
        return;
    if (!player.getState().hasFile) {
        pendingSubs.insert(pendingSubs.end(), subs.begin(), subs.end());
        return;
    }
    for (size_t i = 0; i < subs.size(); i++)
        player.addSubtitle(subs[i], i + 1 == subs.size());
}

void App::applyPendingSubs() {
    if (pendingSubs.empty())
        return;
    std::vector<std::wstring> subs;
    subs.swap(pendingSubs);
    for (size_t i = 0; i < subs.size(); i++)
        player.addSubtitle(subs[i], i + 1 == subs.size());
}

void App::loadCurrent() {
    if (cur < playlist.size())
        player.loadFile(playlist[cur]);
}

void App::playIndex(size_t i) {
    if (i < playlist.size()) {
        cur = i;
        loadCurrent();
    }
}

void App::toggleFullscreen() {
    if (!fullscreen) {
        GetWindowRect(hwnd, &savedRect);
        fullscreen = true;
        chrome.setMaximized(true);
        util::SetCornerPreference(hwnd, DWMWCP_DONOTROUND);
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {0};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(mon, &mi)) {
            SetWindowPos(hwnd, nullptr, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOZORDER | SWP_FRAMECHANGED);
        }
    } else {
        fullscreen = false;
        chrome.setMaximized(IsZoomed(hwnd));
        util::SetCornerPreference(hwnd, IsZoomed(hwnd) ? DWMWCP_DONOTROUND : DWMWCP_ROUND);
        SetWindowPos(hwnd, nullptr, savedRect.left, savedRect.top,
                     savedRect.right - savedRect.left, savedRect.bottom - savedRect.top,
                     SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    syncLayers();
    updateLayers(lastMouseX, lastMouseY);
    InvalidateRect(hwnd, nullptr, TRUE);
}

// ---------- owner drawn popup menus ----------

int App::runMenu(const std::vector<MenuItem> &items, const RECT &anchor) {
    menuItems = items;
    HMENU hmenu = CreatePopupMenu();
    HBRUSH menuBg = CreateSolidBrush(theme.surface);
    MENUINFO mi = {0};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_BACKGROUND;
    mi.hbrBack = menuBg;
    SetMenuInfo(hmenu, &mi);

    for (size_t i = 0; i < menuItems.size(); i++) {
        MENUITEMINFOW ii = {0};
        ii.cbSize = sizeof(ii);
        ii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_DATA | MIIM_STATE;
        ii.fType = MFT_OWNERDRAW;
        ii.wID = menuItems[i].id;
        ii.dwItemData = (ULONG_PTR)&menuItems[i];
        ii.fState = (menuItems[i].v.kind == 1 || menuItems[i].v.kind == 2)
                        ? MFS_DISABLED : MFS_ENABLED;
        InsertMenuItemW(hmenu, (UINT)i, TRUE, &ii);
    }

    POINT pt = {anchor.right, anchor.top - util::Scale(4, dpi)};
    ClientToScreen(hwnd, &pt);
    menuOpen = true;
    setLayerVisible(false, true);
    int cmd = (int)TrackPopupMenuEx(hmenu,
                                    TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD,
                                    pt.x, pt.y, hwnd, nullptr);
    menuOpen = false;
    DestroyMenu(hmenu);
    DeleteObject(menuBg);
    updateLayers(lastMouseX, lastMouseY);
    return cmd;
}

void App::showSubtitleMenu() {
    player.refreshSubtitles();
    SubtitleInfo si = player.getSubtitleInfo();

    std::vector<MenuItem> items;
    auto push = [&](UINT id, const std::wstring &text, const std::wstring &hint,
                    int kind, bool checked) {
        MenuItem m;
        m.id = id;
        m.v.text = text;
        m.v.hint = hint;
        m.v.kind = kind;
        m.v.checked = checked;
        items.push_back(m);
    };

    push(0, i18n::T(i18n::Str::SubMenuTitle), L"wmpv", 1, false);
    push(ID_SUB_OPEN, i18n::T(i18n::Str::SubLoad), L"Ctrl + S", 0, false);
    push(0, L"", L"", 2, false);
    push(0, std::wstring(i18n::T(i18n::Str::SubTracks)) + std::to_wstring(si.tracks.size()),
         L"", 1, false);
    push(ID_SUB_OFF, i18n::T(i18n::Str::SubOff), L"", 3, si.sid == 0);
    for (size_t i = 0; i < si.tracks.size(); i++) {
        const SubTrack &t = si.tracks[i];
        std::wstring label = t.label;
        if (label.size() > 30)
            label = label.substr(0, 28) + L"…";
        push(ID_SUB_TRACK + (UINT)i, label,
             t.external ? i18n::T(i18n::Str::SubExternal)
                        : i18n::T(i18n::Str::SubInternal),
             3, si.sid == t.id);
    }
    push(0, L"", L"", 2, false);
    push(ID_SUB_VISIBLE, i18n::T(i18n::Str::SubVisible), L"V", 4, si.visible);
    push(0, L"", L"", 2, false);
    wchar_t delay[32];
    swprintf(delay, 32, i18n::T(i18n::Str::DelayValue), si.delay);
    push(0, std::wstring(i18n::T(i18n::Str::SubDelay)) + delay, L"", 1, false);
    push(ID_SUB_DELAY_MINUS, i18n::T(i18n::Str::SubEarlier), L"[", 0, false);
    push(ID_SUB_DELAY_PLUS, i18n::T(i18n::Str::SubLater), L"]", 0, false);
    push(ID_SUB_DELAY_RESET, i18n::T(i18n::Str::SubResetDelay), L"", 0, false);
    push(0, L"", L"", 2, false);
    push(0, std::wstring(i18n::T(i18n::Str::SubFontSize)) + std::to_wstring((int)si.fontSize),
         L"", 1, false);
    push(ID_SUB_FONT_MINUS, i18n::T(i18n::Str::SubFontSmaller), L"-", 0, false);
    push(ID_SUB_FONT_PLUS, i18n::T(i18n::Str::SubFontLarger), L"+", 0, false);

    int cmd = runMenu(items, chrome.subtitleButtonRect());
    switch (cmd) {
    case ID_SUB_OPEN: openSubtitleFiles(); break;
    case ID_SUB_OFF: player.selectSubtitle(0); break;
    case ID_SUB_VISIBLE: player.setSubtitleVisible(!si.visible); break;
    case ID_SUB_DELAY_MINUS: player.shiftSubtitleDelay(-0.1); break;
    case ID_SUB_DELAY_PLUS: player.shiftSubtitleDelay(0.1); break;
    case ID_SUB_DELAY_RESET: player.resetSubtitleDelay(); break;
    case ID_SUB_FONT_MINUS: player.shiftSubtitleFontSize(-5); break;
    case ID_SUB_FONT_PLUS: player.shiftSubtitleFontSize(5); break;
    default:
        if (cmd >= ID_SUB_TRACK && cmd - ID_SUB_TRACK < (int)si.tracks.size())
            player.selectSubtitle(si.tracks[cmd - ID_SUB_TRACK].id);
        break;
    }
}

void App::showSpeedMenu() {
    PlayerState s = player.getState();
    static const double presets[] = {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0};
    std::wstring cur = util::FormatSpeed(s.speed);

    std::vector<MenuItem> items;
    auto push = [&](UINT id, const std::wstring &text, const std::wstring &hint,
                    int kind, bool checked) {
        MenuItem m;
        m.id = id;
        m.v.text = text;
        m.v.hint = hint;
        m.v.kind = kind;
        m.v.checked = checked;
        items.push_back(m);
    };

    push(0, std::wstring(i18n::T(i18n::Str::SpeedMenuTitle)) + cur, L"", 1, false);
    for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
        push(ID_SPEED_PRESET + (UINT)i, util::FormatSpeed(presets[i]),
             presets[i] == 1.0 ? i18n::T(i18n::Str::SpeedNormal) : L"",
             3, fabs(s.speed - presets[i]) < 0.001);
    }
    push(0, L"", L"", 2, false);
    push(ID_SPEED_MINUS, i18n::T(i18n::Str::SpeedSlower), L"-", 0, false);
    push(ID_SPEED_PLUS, i18n::T(i18n::Str::SpeedFaster), L"+", 0, false);
    push(ID_SPEED_RESET, i18n::T(i18n::Str::SpeedReset), L"\\", 0, false);

    int cmd = runMenu(items, chrome.speedButtonRect());
    if (cmd >= ID_SPEED_PRESET && cmd - ID_SPEED_PRESET < (int)(sizeof(presets) / sizeof(presets[0])))
        player.setSpeed(presets[cmd - ID_SPEED_PRESET]);
    else if (cmd == ID_SPEED_MINUS)
        player.setSpeed(s.speed - 0.25);
    else if (cmd == ID_SPEED_PLUS)
        player.setSpeed(s.speed + 0.25);
    else if (cmd == ID_SPEED_RESET)
        player.setSpeed(1.0);
}

void App::showLanguageMenu() {
    std::vector<MenuItem> items;
    auto push = [&](UINT id, const std::wstring &text, const std::wstring &hint,
                    int kind, bool checked) {
        MenuItem m;
        m.id = id;
        m.v.text = text;
        m.v.hint = hint;
        m.v.kind = kind;
        m.v.checked = checked;
        items.push_back(m);
    };

    i18n::Lang cur = i18n::Current();
    push(0, i18n::T(i18n::Str::LangMenuTitle), L"L", 1, false);
    push(0, L"", L"", 2, false);
    push(ID_LANG_AUTO, i18n::T(i18n::Str::LangAuto), L"", 3, cur == i18n::Lang::Auto);
    push(ID_LANG_ZH, L"简体中文", L"", 3, cur == i18n::Lang::ZhCN);
    push(ID_LANG_EN, L"English", L"", 3, cur == i18n::Lang::EnUS);

    int cmd = runMenu(items, chrome.speedButtonRect());
    i18n::Lang next = cur;
    switch (cmd) {
    case ID_LANG_AUTO: next = i18n::Lang::Auto; break;
    case ID_LANG_ZH:   next = i18n::Lang::ZhCN; break;
    case ID_LANG_EN:   next = i18n::Lang::EnUS; break;
    default: return;
    }
    if (next == cur)
        return;
    i18n::Set(next);
    i18n::Save(next);
    refreshLayers();
    InvalidateRect(hwnd, nullptr, TRUE);
}

// ---------- window procedures ----------

static LRESULT CALLBACK LayerWndProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_ERASEBKGND)
        return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        BeginPaint(w, &ps);
        EndPaint(w, &ps);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

static LRESULT CALLBACK VideoWndProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    static bool tracking = false;
    if (m == WM_MOUSEMOVE) {
        if (!tracking) {
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, w, 0};
            TrackMouseEvent(&t);
            tracking = true;
        }
        PostMessageW(GetParent(w), WM_APP_FWD_MOUSE, 0, lp);
        return 0;
    }
    if (m == WM_MOUSELEAVE) {
        tracking = false;
        PostMessageW(GetParent(w), WM_APP_MOUSE_LEAVE, 0, 0);
        return 0;
    }
    if (m == WM_SETCURSOR) {
        if (g_app && g_app->player.getState().hasFile) {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(w, &p);
            bool onTitle = p.y < g_app->chrome.titleBarHeight() && g_app->titleVisible;
            bool onCtrl = p.y >= g_app->chrome.controlsRect().top && g_app->ctrlVisible;
            Ctrl c = (onTitle || onCtrl) ? g_app->chrome.hitTest(p.x, p.y) : Ctrl::None;
            if (c != Ctrl::None && c != Ctrl::Drag) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        return DefWindowProcW(w, m, wp, lp);
    }
    if (m == WM_LBUTTONDOWN) {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (g_app) {
            bool media = g_app->player.getState().hasFile;
            bool onTitle = y < g_app->chrome.titleBarHeight() && g_app->titleVisible;
            bool onCtrl = y >= g_app->chrome.controlsRect().top && g_app->ctrlVisible && media;
            Ctrl c = g_app->chrome.hitTest(x, y);
            if (c == Ctrl::Drag && onTitle) {
                POINT p;
                GetCursorPos(&p);
                ReleaseCapture();
                SendMessageW(GetParent(w), WM_NCLBUTTONDOWN, HTCAPTION,
                             MAKELPARAM(p.x, p.y));
                return 0;
            }
            if ((onTitle || onCtrl) && c != Ctrl::None) {
                g_app->handleClick(x, y);
                return 0;
            }
            g_app->pendingClick = true;
            SetTimer(w, IDT_CLICK, 250, nullptr);
        }
        return 0;
    }
    if (m == WM_LBUTTONUP) {
        if (g_app)
            g_app->endDrag();
        return 0;
    }
    if (m == WM_LBUTTONDBLCLK) {
        int y = GET_Y_LPARAM(lp);
        if (g_app) {
            g_app->pendingClick = false;
            KillTimer(w, IDT_CLICK);
            bool onTitle = y < g_app->chrome.titleBarHeight() && g_app->titleVisible;
            bool onCtrl = y >= g_app->chrome.controlsRect().top && g_app->ctrlVisible;
            if (!onTitle && !onCtrl) {
                if (g_app->player.getState().hasFile)
                    g_app->toggleFullscreen();
                else
                    g_app->openFiles();
            }
        }
        return 0;
    }
    if (m == WM_MOUSEWHEEL) {
        if (g_app) {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            PlayerState s = g_app->player.getState();
            g_app->player.setVolume(s.volume + (delta > 0 ? 5 : -5));
        }
        return 0;
    }
    if (m == WM_TIMER) {
        if (wp == IDT_CLICK && g_app && g_app->pendingClick) {
            g_app->pendingClick = false;
            g_app->player.playPause();
        }
        return 0;
    }
    if (m == WM_ERASEBKGND)
        return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC h = BeginPaint(w, &ps);
        RECT r;
        GetClientRect(w, &r);
        FillRect(h, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
        EndPaint(w, &ps);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

static LRESULT CALLBACK WndProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    App *app = (App *)GetWindowLongPtrW(w, GWLP_USERDATA);
    switch (m) {
    case WM_CREATE: {
        app = (App *)((CREATESTRUCTW *)lp)->lpCreateParams;
        SetWindowLongPtrW(w, GWLP_USERDATA, (LONG_PTR)app);
        app->onCreate(w);
        return 0;
    }
    case WM_DESTROY:
        if (app->hBmp) DeleteObject(app->hBmp);
        if (app->hMem) DeleteDC(app->hMem);
        if (app->titleLayer) DestroyWindow(app->titleLayer);
        if (app->ctrlLayer) DestroyWindow(app->ctrlLayer);
        app->player.shutdown();
        PostQuitMessage(0);
        return 0;
    case WM_CLOSE:
        DestroyWindow(w);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        mm->ptMinTrackSize.x = util::Scale(760, app->dpi);
        mm->ptMinTrackSize.y = util::Scale(500, app->dpi);
        return 0;
    }
    case WM_SETTINGCHANGE:
        if (lp && !lstrcmpiW((LPCWSTR)lp, L"ImmersiveColorSet")) {
            app->theme = ui::GetSystemTheme();
            app->chrome.setTheme(app->theme);
            util::SetImmersiveDarkMode(w, app->theme.dark);
            app->refreshLayers();
            InvalidateRect(w, nullptr, TRUE);
        }
        return DefWindowProcW(w, m, wp, lp);
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT *ms = (MEASUREITEMSTRUCT *)lp;
        if (ms->CtlType == ODT_MENU && ms->itemData) {
            MenuItem *it = (MenuItem *)ms->itemData;
            ms->itemWidth = app->chrome.menuItemWidth();
            ms->itemHeight = app->chrome.menuItemHeight(it->v.kind);
            return TRUE;
        }
        return DefWindowProcW(w, m, wp, lp);
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *ds = (DRAWITEMSTRUCT *)lp;
        if (ds->CtlType == ODT_MENU && ds->itemData) {
            MenuItem *it = (MenuItem *)ds->itemData;
            app->chrome.paintMenuItem(ds->hDC, ds->rcItem, it->v,
                                      (ds->itemState & ODS_SELECTED) != 0,
                                      (ds->itemState & ODS_DISABLED) != 0);
            return TRUE;
        }
        return DefWindowProcW(w, m, wp, lp);
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(w, &ps);
        RECT rc;
        GetClientRect(w, &rc);
        int W = rc.right, H = rc.bottom;
        if (W > 0 && H > 0) {
            if (!app->hMem)
                app->hMem = CreateCompatibleDC(hdc);
            if (!app->hBmp || app->memW != W || app->memH != H) {
                if (app->hBmp) DeleteObject(app->hBmp);
                app->hBmp = CreateCompatibleBitmap(hdc, W, H);
                app->memW = W;
                app->memH = H;
            }
            HBRUSH bg = CreateSolidBrush(app->theme.panel);
            FillRect(app->hMem, &rc, bg);
            DeleteObject(bg);
            HBITMAP old = (HBITMAP)SelectObject(app->hMem, app->hBmp);
            app->onPaint(app->hMem);
            BitBlt(hdc, 0, 0, W, H, app->hMem, 0, 0, SRCCOPY);
            SelectObject(app->hMem, old);
        }
        EndPaint(w, &ps);
        return 0;
    }
    case WM_SIZE:
        app->onSize((int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
        app->chrome.setMaximized(IsZoomed(w) || app->fullscreen);
        util::SetCornerPreference(w, (IsZoomed(w) || app->fullscreen)
                                      ? DWMWCP_DONOTROUND : DWMWCP_ROUND);
        app->syncLayers();
        return 0;
    case WM_WINDOWPOSCHANGED:
        if (app)
            app->syncLayers();
        return DefWindowProcW(w, m, wp, lp);
    case WM_DPICHANGED: {
        RECT *pr = (RECT *)lp;
        SetWindowPos(w, nullptr, pr->left, pr->top, pr->right - pr->left,
                     pr->bottom - pr->top, SWP_NOZORDER | SWP_NOACTIVATE);
        app->dpi = util::GetDpi(w);
        app->chrome.setDpi(app->dpi);
        app->syncLayers();
        return 0;
    }
    case WM_SETCURSOR: {
        if (!app->player.getState().hasFile) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(w, &pt);
            if (app->chrome.hitEmptyButton(pt.x, pt.y)) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        return DefWindowProcW(w, m, wp, lp);
    }
    case WM_NCHITTEST: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(w, &pt);
        if (!app->player.getState().hasFile &&
            app->chrome.hitTest(pt.x, pt.y) == Ctrl::Drag)
            return HTCAPTION;
        return HTCLIENT;
    }
    case WM_MOUSEMOVE:
        app->handleMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_LBUTTONDOWN:
        app->handleClick(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_LBUTTONUP:
        app->endDrag();
        return 0;
    case WM_LBUTTONDBLCLK: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (!app->player.getState().hasFile &&
            app->chrome.hitTest(x, y) == Ctrl::Drag)
            app->openFiles();
        return 0;
    }
    case WM_KEYDOWN:
        switch (wp) {
        case VK_SPACE: app->player.playPause(); break;
        case VK_LEFT: app->player.seekRelative(-5.0); break;
        case VK_RIGHT: app->player.seekRelative(5.0); break;
        case VK_UP: { PlayerState s = app->player.getState(); app->player.setVolume(s.volume + 5); } break;
        case VK_DOWN: { PlayerState s = app->player.getState(); app->player.setVolume(s.volume - 5); } break;
        case 'F': case 'f': app->toggleFullscreen(); break;
        case 'M': case 'm': { PlayerState s = app->player.getState(); app->player.setMute(!s.mute); } break;
        case 'O': case 'o': app->openFiles(); break;
        case 'S': case 's':
            if (GetKeyState(VK_CONTROL) & 0x8000)
                app->openSubtitleFiles();
            else
                app->player.cycleSubtitle();
            break;
        case 'V': case 'v': {
            SubtitleInfo si = app->player.getSubtitleInfo();
            app->player.setSubtitleVisible(!si.visible);
            break;
        }
        case 'B': case 'b': app->showSubtitleMenu(); break;
        case 'P': case 'p': app->showSpeedMenu(); break;
        case 'L': case 'l': app->showLanguageMenu(); break;
        case VK_OEM_4: app->player.shiftSubtitleDelay(-0.1); break;
        case VK_OEM_6: app->player.shiftSubtitleDelay(0.1); break;
        case VK_OEM_MINUS: { PlayerState s = app->player.getState(); app->player.setSpeed(s.speed - 0.25); } break;
        case VK_OEM_PLUS: { PlayerState s = app->player.getState(); app->player.setSpeed(s.speed + 0.25); } break;
        case VK_OEM_5: app->player.setSpeed(1.0); break;
        case VK_ESCAPE: if (app->fullscreen) app->toggleFullscreen(); break;
        default: break;
        }
        return 0;
    case WM_APP_FWD_MOUSE:
        app->handleMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_APP_MOUSE_LEAVE:
        app->onMouseLeave();
        return 0;
    case WM_MOUSELEAVE: {
        POINT pt;
        GetCursorPos(&pt);
        RECT wr;
        GetWindowRect(w, &wr);
        if (pt.x < wr.left || pt.x >= wr.right || pt.y < wr.top || pt.y >= wr.bottom)
            app->onMouseLeave();
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        if (app->lastHover == Ctrl::Seek)
            app->player.seekRelative(delta > 0 ? 5.0 : -5.0);
        else {
            PlayerState s = app->player.getState();
            app->player.setVolume(s.volume + (delta > 0 ? 5 : -5));
        }
        return 0;
    }
    case WM_DROPFILES: {
        HDROP h = (HDROP)wp;
        int n = DragQueryFileW(h, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::wstring> files;
        wchar_t tmp[MAX_PATH];
        for (int i = 0; i < n; i++) {
            if (DragQueryFileW(h, i, tmp, MAX_PATH))
                files.push_back(std::wstring(tmp));
        }
        DragFinish(h);
        if (!files.empty())
            app->loadPaths(files);
        return 0;
    }
    case WM_MPV_STATE: {
        app->chrome.setState(app->player.getState());
        app->chrome.setSubtitle(app->player.getSubtitleInfo());
        app->repositionVideo();
        app->updateLayers(app->lastMouseX, app->lastMouseY);
        app->refreshLayers();
        if (!app->player.getState().hasFile)
            InvalidateRect(w, nullptr, TRUE);
        return 0;
    }
    case WM_MPV_FILELOADED:
        app->applyPendingSubs();
        app->player.refreshSubtitles();
        return 0;
    case WM_MPV_SUBS:
        app->player.refreshSubtitles();
        return 0;
    case WM_MPV_ENDFILE: {
        int reason = (int)wp;
        if (reason == MPV_END_FILE_REASON_EOF && app->cur + 1 < app->playlist.size()) {
            app->cur++;
            app->loadCurrent();
        } else {
            if (reason == MPV_END_FILE_REASON_ERROR)
                MessageBoxW(w, i18n::T(i18n::Str::PlayError), L"wmpv",
                            MB_OK | MB_ICONINFORMATION);
            app->player.clearFileState();
            app->chrome.setState(app->player.getState());
            app->chrome.setSubtitle(app->player.getSubtitleInfo());
            app->repositionVideo();
            app->updateLayers(app->lastMouseX, app->lastMouseY);
            app->refreshLayers();
            InvalidateRect(w, nullptr, TRUE);
        }
        return 0;
    }
    case WM_MPV_SHUTDOWN:
        return 0;
    default:
        return DefWindowProcW(w, m, wp, lp);
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    i18n::Init(GetCommandLineW());
    util::InitDwm();
    GdiplusStartupInput gpsi;
    ULONG_PTR gptoken = 0;
    GdiplusStartup(&gptoken, &gpsi, nullptr);

    WNDCLASSW vc = {0};
    vc.style = CS_DBLCLKS;
    vc.lpfnWndProc = VideoWndProc;
    vc.hInstance = hInst;
    vc.lpszClassName = L"WMPVVIDEO";
    vc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&vc);

    WNDCLASSW lc = {0};
    lc.lpfnWndProc = LayerWndProc;
    lc.hInstance = hInst;
    lc.lpszClassName = L"WMPVLAYER";
    lc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&lc);

    ui::Theme theme = ui::GetSystemTheme();
    HBRUSH bg = CreateSolidBrush(theme.panel);

    WNDCLASSW wc = {0};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"WMPV";
    wc.hbrBackground = bg;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    g_app = new App();

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_ACCEPTFILES,
        L"WMPV", L"wmpv",
        WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1180, 740,
        nullptr, nullptr, hInst, g_app);

    if (!hwnd) {
        delete g_app;
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    delete g_app;
    DeleteObject(bg);
    if (gptoken)
        GdiplusShutdown(gptoken);
    return (int)msg.wParam;
}
