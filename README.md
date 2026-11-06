# wmpv

> 基于 **mpv** 的 Windows 原生播放器 —— 单文件、静态链接、无边框自绘界面。

![wmpv 播放界面](assets/screenshots/playback.png)

wmpv 把 mpv 播放内核封装进一个 Win32 无边框窗口：标题栏与控制栏是浮在视频之上的 GDI+ 分层窗口，字幕 / 倍速 / 语言菜单是真正带 per-pixel alpha 的圆角半透明弹窗。整个程序静态链接为一个 `wmpv.exe`，无需安装，也没有需要随附的 DLL。

---

## 特性

- **mpv 0.41 播放内核** — 静态链接 `libmpv.a`，支持 MP4 / MKV / AVI / MOV / MP3 / FLAC 等常见格式，以及内嵌与外挂字幕（SRT / ASS / SSA / VTT）。
- **单文件绿色发布** — mpv、FFmpeg、libass、libplacebo 等全部静态链接；运行时只需要系统自带的 UCRT 与显卡驱动提供的 `vulkan-1.dll`，无 side-by-side DLL。
- **自绘无边框窗口** — Win32 + GDI+（`PixelFormat32bppPARGB` / `UpdateLayeredWindow`），跟随系统深浅色主题、Windows 11 圆角与 Mica 背景；播放视频时控制栏自动显隐。
- **半透明圆角弹窗** — 自定义 `WS_EX_LAYERED` popup，圆角、柔和阴影、细边框、hover 圆角高亮与 accent 竖条；超长列表限制在窗口内并支持滚轮 / 方向键滚动。
- **字幕控制** — 加载外挂字幕、切换内外挂轨道、显隐字幕、±0.1 秒延迟、字号增减。
- **倍速控制** — 0.25× ~ 3.0× 预设，以及 ±0.25 微调与一键恢复正常速度。
- **双语界面** — 简体中文 / English，默认跟随系统 UI 语言；语言菜单即时切换并记住选择。中文优先 `Microsoft YaHei UI`，英文使用 `Segoe UI`。
- **顺手的交互** — 拖放打开文件、播放列表自动续播、进度条与音量条拖拽、静音、全屏（进出全屏使用不同方向的全屏图标）、滚轮调音量。

> 截图中的画面是用于演示 UI 的渐变测试图，不是真实影片。

---

## 截图

<p align="center">
  <img src="assets/screenshots/welcome.png" width="46%" alt="欢迎页">
  <img src="assets/screenshots/menu-speed.png" width="46%" alt="倍速菜单">
</p>

<p align="center">
  <img src="assets/screenshots/menu-subtitles.png" width="46%" alt="字幕菜单">
  <img src="assets/screenshots/language.png" width="46%" alt="语言菜单">
</p>

---

## 快捷键

| 按键 | 功能 |
| --- | --- |
| `Space` | 播放 / 暂停 |
| `←` / `→` | 后退 / 前进 5 秒 |
| `↑` / `↓` | 音量 +5 / −5 |
| `O` | 打开文件 |
| `Ctrl + S` | 加载外挂字幕文件 |
| `S` | 循环切换字幕轨道 |
| `V` | 显示 / 隐藏字幕 |
| `B` / `P` / `L` | 字幕 / 倍速 / 语言菜单 |
| `[` / `]` | 字幕提前 / 延后 0.1 秒 |
| `-` / `+` | 倍速 −0.25 / +0.25 |
| `\` | 恢复正常速度 |
| `M` | 静音切换 |
| `F` | 进入 / 退出全屏 |
| `Esc` | 退出全屏（弹窗内为关闭弹窗） |
| 滚轮 | 音量 ±5；悬停在进度条上时快进 / 后退 5 秒 |

弹窗内另支持 `↑` / `↓` 选择、`Enter` 执行、`Home` / `End` / `PgUp` / `PgDn` 翻页、滚轮滚动。

也可以直接拖放文件到窗口，或在命令行传入多个文件组成播放列表。

### 语言覆盖

优先级：`--lang=xx` > 环境变量 `WMPV_LANG` > 用户上次选择（`HKCU\Software\wmpv\Language`）> 系统 UI 语言。

```bat
wmpv.exe --lang=en-US
set WMPV_LANG=zh-CN && wmpv.exe
```

接受 `zh-CN` / `zh` / `chinese`、`en-US` / `en` / `english`、`auto`（跟随系统）。

---

## 构建

**环境要求**

- Windows 10/11 x64
- Visual Studio（含 `clang-cl` / `lld-link`）、CMake ≥ 3.20、Ninja
- 以 `gpl=true` 构建的**静态** mpv（本项目验证版本 0.41.0）及其 vcpkg `x64-windows-static` 依赖

> 必须使用 `clang-cl` + `lld-link`：`libmpv.a` / `libplacebo.a` 是 GNU `ar` 格式的 COFF 归档，MSVC 的 `link.exe` 无法读取，而 `lld-link` 可以。工程也强制使用静态 CRT（`/MT`）以匹配 mpv 的运行时。

**依赖路径**统一在 `cmake/config.cmake` 中配置，默认值相对于源码树（`<parent>/mpv`、`<grandparent>/vcpkg`），不含机器绝对路径。可用以下任一方式覆盖：

```bat
build.bat -DWMPV_MPV_ROOT=D:/work/mpv -DWMPV_VCPKG_ROOT=D:/work/vcpkg
```

或创建 `cmake/local.cmake`（已被 `.gitignore` 忽略）：

```cmake
set(WMPV_MPV_ROOT   "D:/work/mpv"   CACHE PATH "" FORCE)
set(WMPV_VCPKG_ROOT "D:/work/vcpkg" CACHE PATH "" FORCE)
```

然后 `build.bat -C cmake\local.cmake`。

**一键构建**

```bat
build.bat
```

脚本自动探测工具链、配置 CMake、编译并安装到 `install/wmpv.exe`（同时复制 `icon.ico`）。`tools/deploy.bat` 只执行安装步骤，不重新编译。

工具链路径也可用环境变量覆盖：`WMPV_VCVARS`、`WMPV_LLVM_BIN`、`WMPV_CMAKE`、`WMPV_NINJA`。

---

## 版本

当前版本：**1.0.0**。

版本号的唯一来源是 `CMakeLists.txt` 中的 `project(wmpv VERSION 1.0.0 LANGUAGES CXX)`。CMake 在配置阶段用 `src/version.h.in` 生成 `build/generated/version.h`，C++ 代码与 `src/wmpv.rc` 的 `VS_VERSION_INFO` 资源都引用它。升级版本只需改这一处，重新构建后下面三项会自动同步：

- exe 的「属性 → 详细信息」（`FileVersion` / `ProductVersion` / `LegalCopyright` 等）
- 欢迎页右下角的 `v1.0.0` 角标
- `--version` / `-V` 输出

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

`wmpv.exe` 是 GUI 子系统程序，没有自带的 stdout：从终端运行时会借用父进程的控制台输出，双击运行（无可用控制台）时改为弹出消息框。

---

## 目录结构

```
wmpv/
├─ src/                  Win32 窗口、mpv 封装、GDI+ 自绘、多语言字符串表
│  ├─ main.cpp           主窗口、弹窗、快捷键、命令行
│  ├─ player.cpp         MpvPlayer（轨道 / 字幕 / 倍速 / 状态）
│  ├─ ui.cpp             标题栏、控制栏、欢迎页、图标与弹窗绘制
│  ├─ util.cpp           DPI / 字体 / 主题工具
│  ├─ i18n.h             header-only 中英文文案表
│  ├─ theme.h            深浅主题配色
│  ├─ version.h.in       版本头模板（CMake 生成 build/generated/version.h）
│  ├─ wmpv.rc            图标 + VERSIONINFO 版本资源
│  └─ wmpv.manifest      DPI / 主题 / Windows 11 应用清单
├─ assets/               icon.ico 与 README 截图
├─ cmake/config.cmake    唯一的依赖路径配置入口
├─ tools/                图标生成、部署脚本
├─ CMakeLists.txt        构建配置 + 版本号（唯一来源）
└─ build.bat             一键构建 + 安装
```

---

## License

**本项目采用 GPL-2.0-or-later（`GPL-2.0-or-later`，[全文见 LICENSE](LICENSE)）。**

### 为什么是 GPLv2+

wmpv **静态链接**了 mpv 0.41.0 的 `libmpv.a`，而该构建开启了 `gpl = true`（见 mpv 源码的 `Copyright` 与 `README.md`：默认 GPLv2 "or later"，只有 `-Dgpl=false` 才是 LGPLv2.1 "or later"）。静态链接会把 mpv 及其带入的 FFmpeg / libass / libplacebo 等目标代码合并进同一个可执行文件，构成 GPL 意义上的单一衍生作品，因此整个程序必须以 **GPLv2+** 发布——MIT、Apache-2.0 等宽松许可证与此不兼容，LGPL 也不适用于当前这个 GPL 构建。

### 分发二进制时的义务

- 随附 [LICENSE](LICENSE)（GPLv2 全文）与版权声明；
- 按 GPLv2 第 3 条提供**完整对应源代码**（或等效书面要约），包括本仓库源码、构建脚本与第三方依赖的获取 / 构建方式；
- 不得对接收者施加额外限制。

### 第三方组件

| 组件 | 许可证（以实际构建为准） |
| --- | --- |
| mpv 0.41.0 (`gpl=true`) | GPL-2.0-or-later |
| FFmpeg (`avcodec` / `avformat` / `avfilter` / …) | LGPL-2.1-or-later；若构建启用了 GPL 组件则为 GPL-2.0-or-later |
| libplacebo | LGPL-2.1-or-later |
| libass / harfbuzz / freetype / fribidi / SPIRV-Tools 等 | 各自的 BSD / MIT / FTL / Apache-2.0 等，详见各项目源码头 |
| Windows SDK / UCRT | 随 Windows / Visual Studio 提供，受 Microsoft 许可约束 |

第三方许可证文本与署名请一并随二进制分发；更换依赖构建选项后请重新核对上表。若改用 `-Dgpl=false` 的 LGPL mpv 构建，理论上可以考虑 LGPL-2.1-or-later，但静态链接仍需满足 LGPL 第 6 条（提供可重链接的目标文件或等价材料），实际分发并不比 GPL 更省事。

---

## 致谢

[mpv](https://mpv.io/) · [FFmpeg](https://ffmpeg.org/) · [libass](https://github.com/libass/libass) · [libplacebo](https://github.com/haasn/libplacebo) 以及所有上游贡献者。
