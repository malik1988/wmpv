#pragma once
#include <windows.h>
#include <string>

namespace util {

// DWM attribute constants that may be missing from older SDK headers.
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWCP_DONOTROUND
#define DWMWCP_DONOTROUND 1
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_SYSTEMBACKDROP_PREFERENCE
#define DWMWA_SYSTEMBACKDROP_PREFERENCE 38
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif

void InitDwm();
bool SetCornerPreference(HWND hwnd, DWORD pref);
bool SetImmersiveDarkMode(HWND hwnd, bool dark);
bool SetMicaBackdrop(HWND hwnd, DWORD type);

UINT GetDpi(HWND hwnd);
// Convert a value expressed in 96-DPI logical units to physical pixels.
int Scale(int x, UINT dpi);

COLORREF GetAccentColor();

// Preferred UI font face (CJK-aware when available, otherwise Segoe UI).
const wchar_t *UiFont();

std::wstring Utf8ToWide(const std::string &s);
std::string WideToUtf8(const std::wstring &s);

// File-name portion of a path and a helper used to route dropped/CLI files.
std::wstring BaseName(const std::wstring &path);
bool IsSubtitlePath(const std::wstring &path);

// Format seconds as H:MM:SS or M:SS depending on length.
std::wstring FormatTime(double seconds);
// Playback speed label: 1.0× / 1.25× / 0.5× ...
std::wstring FormatSpeed(double speed);

} // namespace util
