#include "player.h"
#include "util.h"
#include "i18n.h"
#include <process.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

MpvPlayer::MpvPlayer() {
    InitializeCriticalSection(&cs_);
}

MpvPlayer::~MpvPlayer() {
    shutdown();
    DeleteCriticalSection(&cs_);
}

bool MpvPlayer::init(HWND videoHost, HWND notify) {
    notify_ = notify;
    mpv_ = mpv_create();
    if (!mpv_) {
        return false;
    }

    int64_t wid = (int64_t)(uintptr_t)videoHost;
    mpv_set_option(mpv_, "wid", MPV_FORMAT_INT64, &wid);
    mpv_set_option_string(mpv_, "force-window", "yes");
    mpv_set_option_string(mpv_, "keep-open", "yes");
    mpv_set_option_string(mpv_, "osc", "no");
    mpv_set_option_string(mpv_, "input-default-bindings", "no");
    mpv_set_option_string(mpv_, "input-builtin-bindings", "no");
    mpv_set_option_string(mpv_, "cursor-autohide", "no");
    // Automatically pick up matching .srt/.ass files next to the media, while
    // still allowing explicit sub-add for anything else.
    mpv_set_option_string(mpv_, "sub-auto", "fuzzy");

    if (mpv_initialize(mpv_) < 0) {
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
        return false;
    }

    mpv_observe_property(mpv_, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "speed", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "media-title", MPV_FORMAT_STRING);
    // Only used as a "something changed" signal; the actual list is queried in
    // refreshSubtitles() so all parsing happens on one thread.
    mpv_observe_property(mpv_, 0, "track-list", MPV_FORMAT_NODE);

    running_ = true;
    thread_ = CreateThread(nullptr, 0, eventLoopThunk, this, 0, nullptr);
    return true;
}

void MpvPlayer::shutdown() {
    running_ = false;
    if (mpv_) {
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
    }
    if (thread_) {
        WaitForSingleObject(thread_, 4000);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
}

DWORD WINAPI MpvPlayer::eventLoopThunk(LPVOID self) {
    ((MpvPlayer *)self)->eventLoop();
    return 0;
}

void MpvPlayer::eventLoop() {
    while (running_) {
        mpv_event *e = mpv_wait_event(mpv_, -1);
        if (!e || e->event_id == MPV_EVENT_NONE)
            continue;
        processEvent(e);
        if (e->event_id == MPV_EVENT_SHUTDOWN)
            break;
    }
}

void MpvPlayer::processEvent(mpv_event *e) {
    switch (e->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE: {
        auto *p = (mpv_event_property *)e->data;
        if (!p)
            break;
        if (p->name && !strcmp(p->name, "track-list"))
            PostMessage(notify_, WM_MPV_SUBS, 0, 0);
        if (!p->data)
            break;
        if (p->format == MPV_FORMAT_DOUBLE) {
            double v = *(double *)p->data;
            EnterCriticalSection(&cs_);
            if (!strcmp(p->name, "time-pos"))
                state_.timePos = v;
            else if (!strcmp(p->name, "duration"))
                state_.duration = v;
            else if (!strcmp(p->name, "volume"))
                state_.volume = v;
            else if (!strcmp(p->name, "speed"))
                state_.speed = v;
            LeaveCriticalSection(&cs_);
        } else if (p->format == MPV_FORMAT_FLAG) {
            int v = *(int *)p->data;
            EnterCriticalSection(&cs_);
            if (!strcmp(p->name, "pause"))
                state_.paused = !!v;
            else if (!strcmp(p->name, "mute"))
                state_.mute = !!v;
            LeaveCriticalSection(&cs_);
        } else if (p->format == MPV_FORMAT_STRING) {
            char *s = *(char **)p->data;
            if (s && !strcmp(p->name, "media-title")) {
                EnterCriticalSection(&cs_);
                state_.title = util::Utf8ToWide(s);
                LeaveCriticalSection(&cs_);
            }
        }
        pushUpdate();
        break;
    }
    case MPV_EVENT_FILE_LOADED:
        EnterCriticalSection(&cs_);
        state_.hasFile = true;
        LeaveCriticalSection(&cs_);
        pushUpdate();
        PostMessage(notify_, WM_MPV_FILELOADED, 0, 0);
        PostMessage(notify_, WM_MPV_SUBS, 0, 0);
        break;
    case MPV_EVENT_END_FILE: {
        auto *ef = (mpv_event_end_file *)e->data;
        int reason = ef ? (int)ef->reason : 0;
        PostMessage(notify_, WM_MPV_ENDFILE, (WPARAM)reason, 0);
        break;
    }
    case MPV_EVENT_SHUTDOWN:
        running_ = false;
        PostMessage(notify_, WM_MPV_SHUTDOWN, 0, 0);
        break;
    default:
        break;
    }
}

void MpvPlayer::pushUpdate() {
    PostMessage(notify_, WM_MPV_STATE, 0, 0);
}

PlayerState MpvPlayer::getState() {
    PlayerState s;
    EnterCriticalSection(&cs_);
    s = state_;
    LeaveCriticalSection(&cs_);
    return s;
}

void MpvPlayer::clearFileState() {
    EnterCriticalSection(&cs_);
    state_.hasFile = false;
    state_.timePos = 0.0;
    state_.duration = 0.0;
    state_.title.clear();
    subs_ = SubtitleInfo();
    LeaveCriticalSection(&cs_);
    pushUpdate();
}

void MpvPlayer::updateTitleFromPath(const std::wstring &path) {
    EnterCriticalSection(&cs_);
    state_.path = path;
    state_.title = util::BaseName(path);
    state_.timePos = 0.0;
    state_.duration = 0.0;
    subs_ = SubtitleInfo();
    LeaveCriticalSection(&cs_);
}

bool MpvPlayer::loadFile(const std::wstring &path, bool /*append*/) {
    if (!mpv_)
        return false;
    std::string u8 = util::WideToUtf8(path);
    const char *args[] = {"loadfile", u8.c_str(), "replace", nullptr};
    updateTitleFromPath(path);
    bool ok = mpv_command(mpv_, args) >= 0;
    pushUpdate();
    return ok;
}

void MpvPlayer::playPause() {
    if (!mpv_)
        return;
    const char *args[] = {"cycle", "pause", nullptr};
    mpv_command(mpv_, args);
}

void MpvPlayer::setPaused(bool p) {
    if (!mpv_)
        return;
    int v = p ? 1 : 0;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &v);
}

void MpvPlayer::seek(double seconds) {
    if (!mpv_)
        return;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.3f", seconds);
    const char *args[] = {"seek", buf, "absolute", nullptr};
    mpv_command(mpv_, args);
}

void MpvPlayer::seekRelative(double delta) {
    if (!mpv_)
        return;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.3f", delta);
    const char *args[] = {"seek", buf, "relative", nullptr};
    mpv_command(mpv_, args);
}

void MpvPlayer::setVolume(double v) {
    if (!mpv_)
        return;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    double d = v;
    mpv_set_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &d);
}

void MpvPlayer::setMute(bool m) {
    if (!mpv_)
        return;
    int v = m ? 1 : 0;
    mpv_set_property(mpv_, "mute", MPV_FORMAT_FLAG, &v);
}

void MpvPlayer::setSpeed(double v) {
    if (!mpv_)
        return;
    if (v < 0.1) v = 0.1;
    if (v > 5.0) v = 5.0;
    double d = v;
    mpv_set_property(mpv_, "speed", MPV_FORMAT_DOUBLE, &d);
    EnterCriticalSection(&cs_);
    state_.speed = d;
    LeaveCriticalSection(&cs_);
    pushUpdate();
}

void MpvPlayer::stop() {
    if (!mpv_)
        return;
    const char *args[] = {"stop", nullptr};
    mpv_command(mpv_, args);
}

// ---------- subtitles ------------------------------------------------------

static const mpv_node *NodeField(const mpv_node &map, const char *key) {
    if (map.format != MPV_FORMAT_NODE_MAP || !map.u.list)
        return nullptr;
    mpv_node_list *list = map.u.list;
    for (int i = 0; i < list->num; i++) {
        if (list->keys && list->keys[i] && !strcmp(list->keys[i], key))
            return &list->values[i];
    }
    return nullptr;
}

static std::wstring NodeText(const mpv_node *n) {
    if (n && n->format == MPV_FORMAT_STRING && n->u.string)
        return util::Utf8ToWide(n->u.string);
    return L"";
}

void MpvPlayer::command(const char *a, const char *b, const char *c) {
    if (!mpv_)
        return;
    const char *args[4] = {a, b, c, nullptr};
    mpv_command(mpv_, args);
}

void MpvPlayer::refreshSubtitles() {
    if (!mpv_)
        return;
    SubtitleInfo info;

    mpv_node root;
    if (mpv_get_property(mpv_, "track-list", MPV_FORMAT_NODE, &root) >= 0) {
        if (root.format == MPV_FORMAT_NODE_ARRAY && root.u.list) {
            mpv_node_list *tracks = root.u.list;
            for (int i = 0; i < tracks->num; i++) {
                const mpv_node &tn = tracks->values[i];
                if (NodeText(NodeField(tn, "type")) != L"sub")
                    continue;
                SubTrack s;
                const mpv_node *id = NodeField(tn, "id");
                if (id && id->format == MPV_FORMAT_INT64)
                    s.id = id->u.int64;
                const mpv_node *ext = NodeField(tn, "external");
                s.external = ext && ext->format == MPV_FORMAT_FLAG && ext->u.flag;
                const mpv_node *sel = NodeField(tn, "selected");
                s.selected = sel && sel->format == MPV_FORMAT_FLAG && sel->u.flag;

                std::wstring title = NodeText(NodeField(tn, "title"));
                std::wstring file = NodeText(NodeField(tn, "external-filename"));
                std::wstring lang = NodeText(NodeField(tn, "lang"));
                if (!title.empty())
                    s.label = title;
                else if (!file.empty())
                    s.label = util::BaseName(file);
                else if (!lang.empty())
                    s.label = lang;
                else {
                    wchar_t buf[64];
                    swprintf(buf, 64, i18n::T(i18n::Str::SubTrackFallback), (long long)s.id);
                    s.label = buf;
                }
                info.tracks.push_back(s);
            }
        }
        mpv_free_node_contents(&root);
    }

    char *sid = mpv_get_property_string(mpv_, "sid");
    if (sid) {
        if (sid[0] >= '0' && sid[0] <= '9')
            info.sid = _atoi64(sid);
        mpv_free(sid);
    }
    if (info.sid == 0) {
        for (const SubTrack &s : info.tracks) {
            if (s.selected) {
                info.sid = s.id;
                break;
            }
        }
    }
    int flag = 1;
    if (mpv_get_property(mpv_, "sub-visibility", MPV_FORMAT_FLAG, &flag) >= 0)
        info.visible = flag != 0;
    double d = 0.0;
    if (mpv_get_property(mpv_, "sub-delay", MPV_FORMAT_DOUBLE, &d) >= 0)
        info.delay = d;
    if (mpv_get_property(mpv_, "sub-font-size", MPV_FORMAT_DOUBLE, &d) >= 0)
        info.fontSize = d;

    EnterCriticalSection(&cs_);
    subs_ = info;
    LeaveCriticalSection(&cs_);
    pushUpdate();
}

SubtitleInfo MpvPlayer::getSubtitleInfo() {
    SubtitleInfo s;
    EnterCriticalSection(&cs_);
    s = subs_;
    LeaveCriticalSection(&cs_);
    return s;
}

void MpvPlayer::addSubtitle(const std::wstring &path, bool select) {
    if (!mpv_)
        return;
    std::string u8 = util::WideToUtf8(path);
    command("sub-add", u8.c_str(), select ? "select" : "auto");
    refreshSubtitles();
}

void MpvPlayer::selectSubtitle(int64_t id) {
    if (!mpv_)
        return;
    std::string v = id > 0 ? std::to_string(id) : "no";
    mpv_set_property_string(mpv_, "sid", v.c_str());
    refreshSubtitles();
}

void MpvPlayer::cycleSubtitle() {
    command("cycle", "sid");
    refreshSubtitles();
}

void MpvPlayer::setSubtitleVisible(bool visible) {
    if (!mpv_)
        return;
    int v = visible ? 1 : 0;
    mpv_set_property(mpv_, "sub-visibility", MPV_FORMAT_FLAG, &v);
    refreshSubtitles();
}

void MpvPlayer::shiftSubtitleDelay(double delta) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f", delta);
    command("add", "sub-delay", buf);
    refreshSubtitles();
}

void MpvPlayer::resetSubtitleDelay() {
    command("set", "sub-delay", "0");
    refreshSubtitles();
}

void MpvPlayer::shiftSubtitleFontSize(double delta) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f", delta);
    command("add", "sub-font-size", buf);
    refreshSubtitles();
}
