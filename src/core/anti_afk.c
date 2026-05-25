#include "anti_afk.h"
#include "process.h"
#include "sleep.h"
#include <stdlib.h>
#include <string.h>

static void send_vk(WORD vk) {
    INPUT in[2];
    memset(in, 0, sizeof(in));
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = vk;
    in[0].ki.dwFlags = 0;
    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wVk = vk;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

static void send_mouse_jiggle(void) {
    INPUT in[2];
    memset(in, 0, sizeof(in));
    in[0].type = INPUT_MOUSE;
    in[0].mi.dx = 1; in[0].mi.dy = 0;
    in[0].mi.dwFlags = MOUSEEVENTF_MOVE;
    in[1].type = INPUT_MOUSE;
    in[1].mi.dx = -1; in[1].mi.dy = 0;
    in[1].mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(2, in, sizeof(INPUT));
}

static int roblox_is_foreground(void) {
    HWND fg = GetForegroundWindow();
    if (!fg) return 0;
    char cls[64];
    GetClassNameA(fg, cls, sizeof(cls));
    /* Roblox windows are typically class "WINDOWSCLIENT" or "ROBLOX". Be permissive. */
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (!pid) return 0;
    DWORD pids[64];
    int n = rh_enum_roblox_pids(pids, 64);
    for (int i = 0; i < n; i++)
        if (pids[i] == pid) return 1;
    return 0;
}

static DWORD WINAPI afk_worker(LPVOID p) {
    rh_afk_ctx *c = (rh_afk_ctx *)p;
    while (InterlockedCompareExchange(&c->running, 1, 1) == 1) {
        if (!rh_interruptible_sleep(&c->running, c->interval_sec * 1000)) break;
        if (!rh_is_roblox_running()) continue;
        if (!roblox_is_foreground()) continue;

        switch (c->mode) {
            case RH_AFK_FKEY: {
                static const WORD keys[] = { VK_F22, VK_F23, VK_F24 };
                send_vk(keys[rand() % 3]);
                break;
            }
            case RH_AFK_WASD: {
                static const WORD keys[] = { 'W', 'A', 'S', 'D' };
                send_vk(keys[rand() % 4]);
                break;
            }
            case RH_AFK_MOUSE:
                send_mouse_jiggle();
                break;
        }
    }
    return 0;
}

void rh_afk_init(rh_afk_ctx *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

int rh_afk_start(rh_afk_ctx *c, int interval_sec, rh_afk_mode mode) {
    if (!c) return 0;
    if (InterlockedCompareExchange(&c->running, 1, 0) != 0) return 0;
    c->interval_sec = interval_sec < 5 ? 5 : interval_sec;
    c->mode = mode;
    DWORD tid;
    c->thread = CreateThread(NULL, 0, afk_worker, c, 0, &tid);
    return c->thread != NULL;
}

void rh_afk_stop(rh_afk_ctx *c) {
    if (!c) return;
    if (InterlockedCompareExchange(&c->running, 0, 1) != 1) return;
    if (c->thread) {
        WaitForSingleObject(c->thread, 3000);
        CloseHandle(c->thread);
        c->thread = NULL;
    }
}
