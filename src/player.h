#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include <mpv/client.h>

// Custom messages posted from the mpv event thread to the UI thread.
#define WM_MPV_STATE     (WM_APP + 1)
#define WM_MPV_ENDFILE   (WM_APP + 2)
#define WM_MPV_SHUTDOWN  (WM_APP + 3)
#define WM_MPV_FILELOADED (WM_APP + 4)
#define WM_MPV_SUBS      (WM_APP + 5)   // subtitle tracks changed; re-query them

struct PlayerState {
    bool hasFile = false;
    bool paused = true;
    bool mute = false;
    double timePos = 0.0;
    double duration = 0.0;
    double volume = 100.0;
    double speed = 1.0;
    std::wstring title;
    std::wstring path;
};

// One selectable subtitle track (embedded or external).
struct SubTrack {
    int64_t id = 0;
    std::wstring label;
    bool external = false;
    bool selected = false;
};

struct SubtitleInfo {
    std::vector<SubTrack> tracks;
    int64_t sid = 0;            // 0 = subtitles off
    bool visible = true;
    double delay = 0.0;         // seconds
    double fontSize = 55.0;
};

class MpvPlayer {
public:
    MpvPlayer();
    ~MpvPlayer();

    bool init(HWND videoHost, HWND notify);
    void shutdown();

    bool loadFile(const std::wstring &path, bool append = false);
    void playPause();
    void setPaused(bool p);
    void seek(double seconds);       // absolute position
    void seekRelative(double delta);
    void setVolume(double v);         // 0..100
    void setMute(bool m);
    void setSpeed(double v);          // 0.1 .. 5.0
    void stop();

    // ---- subtitles -------------------------------------------------------
    // Re-reads track-list / sid / sub-* properties. Called on the UI thread.
    void refreshSubtitles();
    SubtitleInfo getSubtitleInfo();
    void addSubtitle(const std::wstring &path, bool select = true);
    void selectSubtitle(int64_t id);  // 0 turns subtitles off
    void cycleSubtitle();
    void setSubtitleVisible(bool visible);
    void shiftSubtitleDelay(double delta);
    void resetSubtitleDelay();
    void shiftSubtitleFontSize(double delta);

    bool isOk() const { return mpv_ != nullptr; }
    PlayerState getState();
    void clearFileState();   // back to the welcome page after the last file ends

private:
    static DWORD WINAPI eventLoopThunk(LPVOID self);
    void eventLoop();
    void processEvent(mpv_event *e);
    void pushUpdate();
    void updateTitleFromPath(const std::wstring &path);
    void command(const char *a, const char *b = nullptr, const char *c = nullptr);

    mpv_handle *mpv_ = nullptr;
    HWND notify_ = nullptr;
    HANDLE thread_ = nullptr;
    volatile bool running_ = false;

    PlayerState state_;
    SubtitleInfo subs_;
    CRITICAL_SECTION cs_;
};
