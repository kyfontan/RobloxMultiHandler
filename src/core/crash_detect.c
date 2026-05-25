#include "crash_detect.h"
#include "process.h"
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Roblox logs a disconnect on EVERY server transition -- including voluntary
 * ones (leaving, teleporting, the loading hand-off between servers). Matching
 * generic text like "Connection lost" / "Disconnection Notification" therefore
 * fires while a game is merely loading, and since we then relaunch -> rejoin ->
 * the new load disconnects again, it loops forever.
 *
 * The reliable signal is the disconnect REASON CODE, verified against live logs:
 *   285 = clean client-initiated disconnect (leave / teleport / shutdown);
 *         always paired with "connectMode: Disconnect ASAP", AckTimeout 0.
 *         MUST be ignored -- this was the loop cause.
 *   277 = real network loss: "Connection lost - Cannot contact server/client",
 *         connectMode: Connected, AckTimeout 1. A genuine crash to recover from.
 *
 * So: treat a disconnect as a real crash only when its reason code is NOT a
 * known voluntary one, plus the unambiguous "Cannot contact server" text and a
 * failed teleport. Channel names such as "NetworkClient" are never matched
 * (they appear in every healthy session). */

/* Returns 1 if this disconnect reason code is voluntary/clean (do NOT restart). */
static int reason_is_voluntary(int code) {
    switch (code) {
    case 0:     /* no error                                      */
    case 285:   /* client-initiated clean disconnect (leave/teleport/shutdown) */
        return 1;
    default:
        return 0;
    }
}

/* Reads the first integer following a "reason:"/"Reason:" marker. */
static int parse_reason_code(const char *after_marker) {
    const char *p = after_marker;
    while (*p && (*p < '0' || *p > '9')) p++;
    return (*p) ? atoi(p) : -1;
}

/* Disconnect-with-reason markers; the code follows the last ':' on the line. */
static const char *kReasonMarkers[] = {
    "Disconnection Notification. Reason:",
    "disconnect with reason:",
    "Disconnected from server for reason: Player:",
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

        if (strstr(buf, "Cannot contact server")) {
            /* Real network loss (the 277 case). */
            snprintf(reason, rcap, "log: connection lost (cannot contact server)");
            hit = 1;
        } else if (strstr(buf, "TeleportFailed") || strstr(buf, "Teleport failed")) {
            snprintf(reason, rcap, "log: teleport failed");
            hit = 1;
        } else {
            /* A disconnect carrying a reason code: restart only if involuntary
               (e.g. ignore 285 = clean leave/teleport, which loops on loads). */
            for (int i = 0; kReasonMarkers[i]; i++) {
                char *p = strstr(buf, kReasonMarkers[i]);
                if (!p) continue;
                int code = parse_reason_code(p + strlen(kReasonMarkers[i]));
                if (code >= 0 && !reason_is_voluntary(code)) {
                    snprintf(reason, rcap, "log: disconnect (reason %d)", code);
                    hit = 1;
                }
                break;  /* a disconnect line was found; if voluntary, ignore it */
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
