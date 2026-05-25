#include "crash_detect.h"
#include "process.h"
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

/* Substrings that mark a *real* unwanted disconnect in the Player log.
 *
 * These appear only at an actual disconnect (verified against live logs):
 *   [FLog::Network] Connection lost: ...
 *   [FLog::Network] Disconnection Notification. Reason: <code>
 *   [DFLog::RbxTransport...] Disconnected from server for reason: ...
 *
 * Do NOT list channel names such as "NetworkClient": that token is printed
 * in every healthy session ("NetworkClient:Create", "[DFLog::NetworkClient]
 * Transport selection..."), so matching it fired a false crash within ~2s of
 * every launch and produced an endless reconnect loop. Likewise the old
 * "GameClient.*disconnected" was a regex the literal strstr() never matched. */
static const char *kErrorSubstrings[] = {
    "Connection lost",
    "Disconnection Notification",
    "Disconnected from server",
    "TeleportFailed",
    "Teleport failed",
    NULL
};

void rh_crash_init(rh_crash_ctx *c) {
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->log_file = INVALID_HANDLE_VALUE;
}

void rh_crash_dispose(rh_crash_ctx *c) {
    if (!c) return;
    if (c->log_file && c->log_file != INVALID_HANDLE_VALUE) {
        CloseHandle(c->log_file);
        c->log_file = INVALID_HANDLE_VALUE;
    }
    c->log_offset.QuadPart = 0;
    c->log_path[0] = 0;
}

static int find_newest_log(char *out, int cap) {
    char appdata[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata)))
        return 0;
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\Roblox\\logs\\*Player*.log", appdata);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    FILETIME best = {0};
    char best_name[MAX_PATH] = "";
    do {
        if (CompareFileTime(&fd.ftLastWriteTime, &best) > 0) {
            best = fd.ftLastWriteTime;
            snprintf(best_name, sizeof(best_name), "%s\\Roblox\\logs\\%s",
                     appdata, fd.cFileName);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);

    if (!best_name[0]) return 0;
    snprintf(out, cap, "%s", best_name);
    return 1;
}

static int ensure_log_open(rh_crash_ctx *c) {
    if (c->log_file != INVALID_HANDLE_VALUE) return 1;
    char path[MAX_PATH];
    if (!find_newest_log(path, sizeof(path))) return 0;
    HANDLE h = CreateFileA(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER end = {0};
    SetFilePointerEx(h, end, &c->log_offset, FILE_END);
    c->log_file = h;
    snprintf(c->log_path, sizeof(c->log_path), "%s", path);
    return 1;
}

static int log_contains_error(rh_crash_ctx *c, char *reason, int rcap) {
    if (!ensure_log_open(c)) return 0;

    LARGE_INTEGER cur = c->log_offset;
    SetFilePointerEx(c->log_file, cur, NULL, FILE_BEGIN);

    char buf[4096];
    DWORD r = 0;
    int hit = 0;
    while (ReadFile(c->log_file, buf, sizeof(buf) - 1, &r, NULL) && r > 0) {
        buf[r] = 0;
        for (int i = 0; kErrorSubstrings[i]; i++) {
            char *p = strstr(buf, kErrorSubstrings[i]);
            if (p) {
                snprintf(reason, rcap, "log: %s", kErrorSubstrings[i]);
                hit = 1;
                break;
            }
        }
        c->log_offset.QuadPart += r;
        if (hit) break;
        if (r < sizeof(buf) - 1) break;
    }
    return hit;
}

typedef struct { int found; char title[128]; } enum_ctx;

static BOOL CALLBACK enum_window_cb(HWND h, LPARAM lp) {
    enum_ctx *e = (enum_ctx *)lp;
    char cls[64], txt[160];
    GetClassNameA(h, cls, sizeof(cls));
    GetWindowTextA(h, txt, sizeof(txt));
    if (strcmp(cls, "#32770") != 0) return TRUE; /* dialog class */
    if (txt[0] == 0) return TRUE;
    if (strstr(txt, "Roblox") || strstr(txt, "Disconnect") || strstr(txt, "Reconnect")) {
        e->found = 1;
        snprintf(e->title, sizeof(e->title), "%s", txt);
        return FALSE;
    }
    return TRUE;
}

static int dialog_present(char *reason, int rcap) {
    enum_ctx e = {0};
    EnumWindows(enum_window_cb, (LPARAM)&e);
    if (e.found) {
        snprintf(reason, rcap, "dialog: %s", e.title);
        return 1;
    }
    return 0;
}

int rh_crash_poll(rh_crash_ctx *c, rh_crash_event *out) {
    if (!c || !out) return 0;
    memset(out, 0, sizeof(*out));

    BOOL alive = rh_is_roblox_running();

    /* Log tail (works whether or not Roblox is alive). */
    char reason[128];
    if (log_contains_error(c, reason, sizeof(reason))) {
        out->kind = RH_CRASH_LOG_ERROR;
        snprintf(out->reason, sizeof(out->reason), "%s", reason);
        return 1;
    }

    /* Dialog fallback while Roblox process still exists. */
    if (alive && dialog_present(reason, sizeof(reason))) {
        out->kind = RH_CRASH_DIALOG_ERROR;
        snprintf(out->reason, sizeof(out->reason), "%s", reason);
        return 1;
    }

    /* Process gone with no specific reason known. */
    if (!alive) {
        out->kind = RH_CRASH_PROCESS_GONE;
        snprintf(out->reason, sizeof(out->reason), "process exited");
        /* Reset log handle so next session re-discovers the newest file. */
        if (c->log_file != INVALID_HANDLE_VALUE) {
            CloseHandle(c->log_file);
            c->log_file = INVALID_HANDLE_VALUE;
            c->log_offset.QuadPart = 0;
            c->log_path[0] = 0;
        }
        return 1;
    }

    return 0;
}
