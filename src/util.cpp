#include "util.h"
#include "i18n.h"
#include <cwchar>
#include <cmath>
#include <algorithm>
#include <shlwapi.h>

namespace util {

typedef HRESULT(WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
typedef HRESULT(WINAPI *PFN_DwmGetColorizationColor)(DWORD *, BOOL *);
static PFN_DwmSetWindowAttribute s_pDwmSetWindowAttribute = nullptr;
static PFN_DwmGetColorizationColor s_pDwmGetColorizationColor = nullptr;
static bool s_dwmInit = false;

void InitDwm() {
    if (s_dwmInit)
        return;
    HMODULE h = LoadLibraryW(L"dwmapi.dll");
    if (h) {
        s_pDwmSetWindowAttribute =
            (PFN_DwmSetWindowAttribute)GetProcAddress(h, "DwmSetWindowAttribute");
        s_pDwmGetColorizationColor =
            (PFN_DwmGetColorizationColor)GetProcAddress(h, "DwmGetColorizationColor");
    }
    s_dwmInit = true;
}

bool SetCornerPreference(HWND hwnd, DWORD pref) {
    InitDwm();
    if (!s_pDwmSetWindowAttribute)
        return false;
    return SUCCEEDED(s_pDwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                                             &pref, sizeof(pref)));
}

bool SetImmersiveDarkMode(HWND hwnd, bool dark) {
    InitDwm();
    if (!s_pDwmSetWindowAttribute)
        return false;
    BOOL v = dark ? TRUE : FALSE;
    // Try the value used on Windows 11 (20); fall back to 19 on Win10 20H1.
    HRESULT r = s_pDwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &v, sizeof(v));
    if (FAILED(r) && DWMWA_USE_IMMERSIVE_DARK_MODE != 19) {
        DWORD old = 19;
        r = s_pDwmSetWindowAttribute(hwnd, old, &v, sizeof(v));
    }
    return SUCCEEDED(r);
}

bool SetMicaBackdrop(HWND hwnd, DWORD type) {
    InitDwm();
    if (!s_pDwmSetWindowAttribute)
        return false;
    return SUCCEEDED(s_pDwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_PREFERENCE,
                                             &type, sizeof(type)));
}

UINT GetDpi(HWND hwnd) {
    typedef UINT(WINAPI * PFN_GetDpiForWindow)(HWND);
    static PFN_GetDpiForWindow p = nullptr;
    static bool init = false;
    if (!init) {
        HMODULE u = GetModuleHandleW(L"user32.dll");
        p = u ? (PFN_GetDpiForWindow)GetProcAddress(u, "GetDpiForWindow") : nullptr;
        init = true;
    }
    if (p)
        return p(hwnd);
    HDC dc = GetDC(nullptr);
    UINT dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc)
        ReleaseDC(nullptr, dc);
    return dpi;
}

int Scale(int x, UINT dpi) {
    return (int)((long long)x * dpi / 96);
}

COLORREF GetAccentColor() {
    InitDwm();
    DWORD color = 0;
    BOOL opaque = TRUE;
    if (s_pDwmGetColorizationColor && SUCCEEDED(s_pDwmGetColorizationColor(&color, &opaque))) {
        // color is 0xAARRGGBB
        BYTE r = (color >> 16) & 0xFF;
        BYTE g = (color >> 8) & 0xFF;
        BYTE b = color & 0xFF;
        return RGB(r, g, b);
    }
    return RGB(0x3B, 0x82, 0xF6); // fallback accent
}

static int CALLBACK FindUiFontProc(const LOGFONTW *lf, const TEXTMETRICW *, DWORD, LPARAM param) {
    if (lf && lstrcmpiW(lf->lfFaceName, L"Microsoft YaHei UI") == 0) {
        *(bool *)param = true;
        return 0;
    }
    return 1;
}

const wchar_t *UiFont() {
    static bool probed = false;
    static bool hasCjk = false;
    if (!probed) {
        HDC dc = GetDC(nullptr);
        if (dc) {
            LOGFONTW lf = {0};
            lf.lfCharSet = DEFAULT_CHARSET;
            lstrcpyW(lf.lfFaceName, L"Microsoft YaHei UI");
            EnumFontFamiliesExW(dc, &lf, FindUiFontProc, (LPARAM)&hasCjk, 0);
            ReleaseDC(nullptr, dc);
        }
        probed = true;
    }
    // 英文界面用 Segoe UI 更地道；中文界面优先微软雅黑 UI，缺字时回退。
    if (!i18n::IsChinese())
        return L"Segoe UI";
    return hasCjk ? L"Microsoft YaHei UI" : L"Segoe UI";
}

std::wstring BaseName(const std::wstring &path) {
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? path : path.substr(p + 1);
}

bool IsSubtitlePath(const std::wstring &path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos)
        return false;
    std::wstring ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) {
        return (c >= L'A' && c <= L'Z') ? (wchar_t)(c - L'A' + L'a') : c;
    });
    static const wchar_t *exts[] = {
        L"srt", L"ass", L"ssa", L"sub", L"vtt", L"smi", L"rt", L"mpl2",
        L"idx", L"sbv", L"lrc", L"pjs", L"jss", L"scc", L"stl", L"dks", L"aqt"
    };
    for (const wchar_t *e : exts)
        if (ext == e)
            return true;
    return false;
}

std::wstring Utf8ToWide(const std::string &s) {
    if (s.empty())
        return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
    return out;
}

std::string WideToUtf8(const std::wstring &s) {
    if (s.empty())
        return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
    return out;
}

std::wstring FormatSpeed(double speed) {
    wchar_t buf[32];
    int hundredths = (int)(speed * 100 + 0.5);
    if (hundredths % 25 == 0 && hundredths % 50 != 0)
        swprintf(buf, 32, L"%.2f×", speed);
    else
        swprintf(buf, 32, L"%.1f×", speed);
    return std::wstring(buf);
}

std::wstring FormatTime(double seconds) {
    if (seconds < 0 || !std::isfinite(seconds))
        seconds = 0;
    long long s = (long long)seconds;
    long long h = s / 3600;
    long long m = (s % 3600) / 60;
    long long sec = s % 60;
    wchar_t buf[32];
    if (h > 0)
        swprintf(buf, 32, L"%lld:%02lld:%02lld", h, m, sec);
    else
        swprintf(buf, 32, L"%lld:%02lld", m, sec);
    return std::wstring(buf);
}

} // namespace util
