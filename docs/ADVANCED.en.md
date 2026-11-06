# wmpv Advanced Guide

**English** | [简体中文](ADVANCED.md) · [← Back to README](../README.en.md)

This guide is for users who need command-line options, custom builds, or who redistribute the binary. If you only want to watch videos, the [README](../README.en.md) is all you need.

---

## Command line

### Open media directly

```bat
wmpv.exe "D:\Videos\movie.mkv"
wmpv.exe "D:\Videos\a.mp4" "D:\Videos\b.mkv"
```

Several files become a playlist that advances automatically. Arguments starting with `--` are never treated as file paths.

### Override the interface language

Priority: `--lang=xx` > environment variable `WMPV_LANG` > last user choice (`HKCU\Software\wmpv\Language`) > system UI language.

```bat
wmpv.exe --lang=en-US
set WMPV_LANG=zh-CN && wmpv.exe
```

Accepted values: `zh-CN` / `zh` / `chinese`, `en-US` / `en` / `english`, `auto` (follow system). A choice made in the language menu (`L`) is written to the registry and takes effect immediately, without a restart.

### Query the version

```bat
wmpv.exe --version
wmpv.exe -V
```

```
wmpv 1.0.0
wmpv - mpv-based media player for Windows
Copyright (C) 2026 wmpv contributors
License: GPL-2.0-or-later
```

`wmpv.exe` is a GUI-subsystem application with no stdout of its own: when started from a terminal it borrows the parent console, and when launched by double-clicking (no console available) it shows a message box instead. The same version is embedded in the exe's `VS_VERSION_INFO` resource and is visible under **Properties → Details**.

---

## Full shortcut list

| Key | Action |
| --- | --- |
| `Space` | Play / pause |
| `←` / `→` | Seek −5 / +5 seconds |
| `↑` / `↓` | Volume +5 / −5 |
| `O` | Open file |
| `Ctrl + S` | Load an external subtitle file |
| `S` | Cycle subtitle tracks |
| `V` | Show / hide subtitles |
| `B` / `P` / `L` | Subtitle / speed / language menu |
| `[` / `]` | Subtitle delay −0.1 / +0.1 s |
| `-` / `+` | Speed −0.25 / +0.25 |
| `\` | Reset to normal speed |
| `M` | Toggle mute |
| `F` | Enter / exit fullscreen |
| `Esc` | Exit fullscreen (or close an open popup) |
| Mouse wheel | Volume ±5; when hovering the seek bar, seek ±5 seconds |

Inside a popup: `↑` / `↓` to move, `Enter` to activate, `Home` / `End` / `PgUp` / `PgDn` to page, and the wheel to scroll. Files can also be dropped directly onto the window.

---

## Building from source

**Requirements**

- Windows 10/11 x64
- Visual Studio (with `clang-cl` / `lld-link`), CMake ≥ 3.20, Ninja
- A **static** mpv build with `gpl=true` (validated with 0.41.0) and its vcpkg `x64-windows-static` dependencies

> `clang-cl` + `lld-link` are mandatory: `libmpv.a` / `libplacebo.a` are GNU `ar` archives of COFF objects, which MSVC's `link.exe` cannot read but `lld-link` can. The project also forces the static CRT (`/MT`) to match mpv's runtime.

All dependency paths live in `cmake/config.cmake`; the defaults are relative to the source tree (`<parent>/mpv`, `<grandparent>/vcpkg`) and contain no machine-specific absolute paths. Override them with either of the following:

```bat
build.bat -DWMPV_MPV_ROOT=D:/work/mpv -DWMPV_VCPKG_ROOT=D:/work/vcpkg
```

Or create `cmake/local.cmake` (already ignored by `.gitignore`):

```cmake
set(WMPV_MPV_ROOT   "D:/work/mpv"   CACHE PATH "" FORCE)
set(WMPV_VCPKG_ROOT "D:/work/vcpkg" CACHE PATH "" FORCE)
```

Then run `build.bat -C cmake\local.cmake`.

**One-step build**

```bat
build.bat
```

The script detects the toolchain, configures CMake, compiles and installs to `install/wmpv.exe` (alongside `icon.ico`). `tools/deploy.bat` only performs the install step without rebuilding. The result is a single statically linked executable that can be shipped on its own.

Toolchain paths can also be overridden with `WMPV_VCVARS`, `WMPV_LLVM_BIN`, `WMPV_CMAKE` and `WMPV_NINJA`.

---

## Version management

Current version: **1.0.0**.

The single source of truth is `project(wmpv VERSION 1.0.0 LANGUAGES CXX)` in `CMakeLists.txt`. During configuration CMake generates `build/generated/version.h` from `src/version.h.in`, and both the C++ code and the `VS_VERSION_INFO` resource in `src/wmpv.rc` include it. Changing that one line and rebuilding updates all of the following automatically:

- The exe's **Properties → Details** tab (`FileVersion` / `ProductVersion` / `LegalCopyright`, etc.)
- The `v1.0.0` badge in the bottom-right corner of the welcome screen
- `--version` / `-V` output

> The version resource must have ID `1`, otherwise `GetFileVersionInfo()` and Explorer's Details tab cannot find it; `src/wmpv.rc` therefore uses an explicit `1 VERSIONINFO`.

---

## Project layout

```
wmpv/
├─ src/                  Win32 window, mpv wrapper, GDI+ drawing, i18n strings
│  ├─ main.cpp           main window, popups, shortcuts, command line
│  ├─ player.cpp         MpvPlayer (tracks / subtitles / speed / state)
│  ├─ ui.cpp             title bar, control bar, welcome screen, icons, popups
│  ├─ util.cpp           DPI / font / theme helpers
│  ├─ i18n.h             header-only Chinese/English string table
│  ├─ theme.h            light/dark theme palette
│  ├─ version.h.in       version header template (generates build/generated/version.h)
│  ├─ wmpv.rc            icon + VERSIONINFO resource
│  └─ wmpv.manifest      DPI / theme / Windows 11 app manifest
├─ assets/               icon.ico and README screenshots
├─ cmake/config.cmake    single entry point for dependency paths
├─ docs/                 advanced guides (Chinese / English)
├─ tools/                icon generator, deploy script
├─ CMakeLists.txt        build config + version number (single source)
├─ LICENSE               full GPLv2 text
├─ README.md / .en.md    user-facing docs (Chinese / English)
└─ build.bat             one-step build + install
```

---

## License and redistribution

**This project is licensed under GPL-2.0-or-later (`GPL-2.0-or-later`; [full text in LICENSE](../LICENSE)).**

### Why GPLv2+

wmpv **statically links** `libmpv.a` from mpv 0.41.0, built with `gpl = true` (see `Copyright` and `README.md` in the mpv source: GPLv2 "or later" by default, LGPLv2.1 "or later" only with `-Dgpl=false`). Static linking merges mpv and the FFmpeg / libass / libplacebo object code it pulls in into a single executable, forming one derivative work under the GPL. The whole program must therefore be distributed under **GPLv2+** — permissive licenses such as MIT or Apache-2.0 are incompatible, and the LGPL does not apply to this GPL build.

### Obligations when distributing binaries

- Ship [LICENSE](../LICENSE) (the full GPLv2 text) together with the copyright notices;
- Provide the **complete corresponding source code** (or an equivalent written offer) as required by Section 3 of GPLv2, including this repository, the build scripts and instructions for obtaining/building third-party dependencies;
- Impose no further restrictions on recipients.

### Third-party components

| Component | License (verify against your actual build) |
| --- | --- |
| mpv 0.41.0 (`gpl=true`) | GPL-2.0-or-later |
| FFmpeg (`avcodec` / `avformat` / `avfilter` / …) | LGPL-2.1-or-later, or GPL-2.0-or-later if built with GPL-enabled components |
| libplacebo | LGPL-2.1-or-later |
| libass / harfbuzz / freetype / fribidi / SPIRV-Tools, etc. | Their respective BSD / MIT / FTL / Apache-2.0 licenses; see each project's source headers |
| Windows SDK / UCRT | Shipped with Windows / Visual Studio under Microsoft licenses |

Include third-party license texts and attributions when redistributing binaries, and re-check the table above if dependency build options change. Switching to an LGPL mpv build (`-Dgpl=false`) would in theory allow LGPL-2.1-or-later, but static linking still requires satisfying Section 6 of the LGPL (supplying relinkable object files or equivalent material), which is not meaningfully easier in practice.

---

[← Back to README](../README.en.md) · [中文高级使用说明](ADVANCED.md)
