#pragma once
#include <windows.h>
#include <string>
#include <cwchar>

// =============================================================================
//  轻量多语言（header-only，文案直接编译进 exe）
// -----------------------------------------------------------------------------
//  方案取舍：界面文案只有 ~40 条、两种语言合计不足 8 KB（UTF-16），而工程定位
//  是"静态链接、单文件自包含"。外置语言包会带来部署文件、路径解析、编码/BOM、
//  缺失回退、版本同步等一整套问题，收益几乎为零，因此这里用一个集中的字符串表
//  硬编码在源码里；调用点统一走 T(Str::X)，不散落任何字面量。
//
//  将来若语言变多或交给翻译维护，只需把 T() 的实现换成从文件/资源读取，
//  调用点无需改动；也可以把本文件拆成 i18n.cpp + 外置包。
//
//  语言优先级： --lang=xx  >  环境变量 WMPV_LANG  >  用户上次选择(注册表)
//              >  系统 UI 语言（默认）
//  未做中文翻译的系统一律回退英文。新增语言时：扩 Lang / Str 表 / AllSet 检查。
// =============================================================================
namespace i18n {

enum class Lang { Auto = 0, ZhCN, EnUS };

enum class Str {
    AppTitle, CaptionPlaying, CaptionIdle,
    WelcomeTitle, WelcomeSubtitle, OpenFile,
    ChipSpace, ChipOpen, ChipDrop, FormatsHint,
    FilterMediaSubs, FilterMedia, FilterSubs, FilterAll,
    InitError, PlayError,
    SubMenuTitle, SubLoad, SubTracks, SubOff, SubExternal, SubInternal,
    SubVisible, SubDelay, DelayValue, SubEarlier, SubLater, SubResetDelay,
    SubFontSize, SubFontSmaller, SubFontLarger, SubTrackFallback,
    SpeedMenuTitle, SpeedNormal, SpeedSlower, SpeedFaster, SpeedReset,
    LangMenuTitle, LangAuto,
    Count
};

constexpr size_t kCount = static_cast<size_t>(Str::Count);

inline constexpr const wchar_t *kZh[kCount] = {
    L"wmpv 播放器",                                        // AppTitle
    L"正在播放  ·  mpv 播放引擎",                          // CaptionPlaying
    L"拖入媒体文件，即刻开始播放",                          // CaptionIdle
    L"每一帧，都值得更好的呈现",                            // WelcomeTitle
    L"拖入视频或音频文件，即刻开始播放",                    // WelcomeSubtitle
    L"打开文件",                                            // OpenFile
    L"空格  播放 / 暂停",                                   // ChipSpace
    L"O  打开文件",                                         // ChipOpen
    L"拖放  快速播放",                                      // ChipDrop
    L"支持 MP4 · MKV · AVI · MOV · MP3 · FLAC    |    字幕 SRT · ASS · SSA · VTT",
    L"媒体与字幕",                                          // FilterMediaSubs
    L"媒体文件",                                            // FilterMedia
    L"字幕文件",                                            // FilterSubs
    L"所有文件",                                            // FilterAll
    L"无法初始化 mpv 播放内核。\n请确认已使用与本机构建的静态 libmpv 重新编译 wmpv.exe。",
    L"无法播放该文件或解码出错。",                          // PlayError
    L"字幕",                                                // SubMenuTitle
    L"加载字幕文件…",                                       // SubLoad
    L"字幕轨道  ·  ",                                       // SubTracks
    L"关闭字幕",                                            // SubOff
    L"外挂",                                                // SubExternal
    L"内嵌",                                                // SubInternal
    L"显示字幕",                                            // SubVisible
    L"字幕延迟  ",                                          // SubDelay
    L"%+.1f 秒",                                           // DelayValue
    L"提前 0.1 秒",                                         // SubEarlier
    L"延后 0.1 秒",                                         // SubLater
    L"重置延迟",                                            // SubResetDelay
    L"字幕字号  ",                                          // SubFontSize
    L"减小字号",                                            // SubFontSmaller
    L"增大字号",                                            // SubFontLarger
    L"字幕轨道 %lld",                                       // SubTrackFallback
    L"播放速度  ·  ",                                       // SpeedMenuTitle
    L"正常",                                                // SpeedNormal
    L"减速 0.25",                                           // SpeedSlower
    L"加速 0.25",                                           // SpeedFaster
    L"恢复正常速度",                                        // SpeedReset
    L"界面语言",                                            // LangMenuTitle
    L"跟随系统",                                            // LangAuto
};

inline constexpr const wchar_t *kEn[kCount] = {
    L"wmpv Player",
    L"Now playing  ·  mpv engine",
    L"Drop a media file to start playing",
    L"Every frame deserves better",
    L"Drop a video or audio file to start playing",
    L"Open File",
    L"Space  Play / Pause",
    L"O  Open File",
    L"Drop  Quick Play",
    L"MP4 · MKV · AVI · MOV · MP3 · FLAC    |    Subtitles SRT · ASS · SSA · VTT",
    L"Media & Subtitles",
    L"Media Files",
    L"Subtitle Files",
    L"All Files",
    L"Failed to initialize the mpv playback engine.\nRebuild wmpv.exe with the matching static libmpv.",
    L"Cannot play this file or a decoding error occurred.",
    L"Subtitles",
    L"Load subtitle file…",
    L"Subtitle tracks  ·  ",
    L"Subtitles off",
    L"External",
    L"Embedded",
    L"Show subtitles",
    L"Subtitle delay  ",
    L"%+.1f s",
    L"Earlier 0.1 s",
    L"Later 0.1 s",
    L"Reset delay",
    L"Subtitle size  ",
    L"Decrease size",
    L"Increase size",
    L"Subtitle track %lld",
    L"Playback speed  ·  ",
    L"Normal",
    L"Slower 0.25",
    L"Faster 0.25",
    L"Reset to normal speed",
    L"Language",
    L"Follow system",
};

// 编译期捕获"某个 key 忘了翻译"（漏写的聚合项会被零初始化成空指针）。
constexpr bool AllSet(const wchar_t *const (&table)[kCount]) {
    for (size_t i = 0; i < kCount; ++i)
        if (!table[i] || !table[i][0])
            return false;
    return true;
}
static_assert(AllSet(kZh), "Chinese translation table is incomplete");
static_assert(AllSet(kEn), "English translation table is incomplete");

inline Lang &Current() {
    static Lang value = Lang::Auto;
    return value;
}

inline Lang DetectSystem() {
    static const Lang detected = [] {
        LANGID id = GetUserDefaultUILanguage();
        if (id == 0)
            id = GetSystemDefaultUILanguage();
        return PRIMARYLANGID(id) == LANG_CHINESE ? Lang::ZhCN : Lang::EnUS;
    }();
    return detected;
}

inline Lang Effective() {
    Lang v = Current();
    return v == Lang::Auto ? DetectSystem() : v;
}

inline const wchar_t *T(Str id) {
    const size_t i = static_cast<size_t>(id);
    if (i >= kCount)
        return L"";
    return (Effective() == Lang::ZhCN ? kZh : kEn)[i];
}

inline bool IsChinese() { return Effective() == Lang::ZhCN; }
inline void Set(Lang l) { Current() = l; }

inline const wchar_t *Code(Lang l) {
    switch (l) {
    case Lang::ZhCN: return L"zh-CN";
    case Lang::EnUS: return L"en-US";
    default:         return L"auto";
    }
}

inline Lang Parse(const std::wstring &s) {
    if (s == L"zh" || s == L"zh-CN" || s == L"zh-cn" || s == L"chinese")
        return Lang::ZhCN;
    if (s == L"en" || s == L"en-US" || s == L"en-us" || s == L"english")
        return Lang::EnUS;
    return Lang::Auto;
}

// ---- 用户上次的选择（HKCU\Software\wmpv\Language，REG_SZ）------------------
inline constexpr const wchar_t *kRegPath = L"Software\\wmpv";
inline constexpr const wchar_t *kRegName = L"Language";

inline Lang LoadSaved() {
    HKEY hk = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &hk) != ERROR_SUCCESS)
        return Lang::Auto;
    wchar_t buf[32] = {0};
    DWORD size = sizeof(buf), type = 0;
    Lang result = Lang::Auto;
    if (RegQueryValueExW(hk, kRegName, nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS &&
        type == REG_SZ)
        result = Parse(buf);
    RegCloseKey(hk);
    return result;
}

inline void Save(Lang l) {
    HKEY hk = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &hk, nullptr) != ERROR_SUCCESS)
        return;
    const wchar_t *v = Code(l);
    RegSetValueExW(hk, kRegName, 0, REG_SZ, (const BYTE *)v,
                   (DWORD)((wcslen(v) + 1) * sizeof(wchar_t)));
    RegCloseKey(hk);
}

// cmdLine: GetCommandLineW() 的返回值；只做一次轻量的 "--lang=" 扫描。
inline void Init(const wchar_t *cmdLine) {
    Lang value = Lang::Auto;
    bool explicitSet = false;

    wchar_t env[32] = {0};
    if (GetEnvironmentVariableW(L"WMPV_LANG", env, 32) > 0) {
        value = Parse(env);
        explicitSet = true;
    }
    if (cmdLine) {
        if (const wchar_t *p = wcsstr(cmdLine, L"--lang=")) {
            std::wstring arg(p + 7);
            size_t end = arg.find_first_of(L" \t\"");
            if (end != std::wstring::npos)
                arg.resize(end);
            value = Parse(arg);
            explicitSet = true;
        }
    }
    if (!explicitSet)
        value = LoadSaved();
    Set(value);
}

} // namespace i18n
