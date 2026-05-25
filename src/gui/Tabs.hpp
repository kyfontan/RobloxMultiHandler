#pragma once
#include <windows.h>
#include <d3d11.h>
#include <vector>

extern "C" {
#include "../core/settings.h"
#include "../core/crash_detect.h"
#include "../core/anti_afk.h"
#include "../core/singleton.h"
}

namespace rh::gui {

struct AppState {
    HWND          host_hwnd = nullptr;
    ID3D11Device *device    = nullptr;

    rh_settings   settings{};

    /* Auto-Crash */
    HANDLE          ac_thread   = nullptr;
    volatile LONG   ac_running  = 0;
    volatile LONG   ac_count    = 0;
    int             ac_delay_ms = 0;

    /* Crash Detect */
    HANDLE             cd_thread   = nullptr;
    volatile LONG      cd_running  = 0;
    volatile LONG      cd_count    = 0;
    int                cd_delay_ms = 0;
    char               cd_uri[512]    = {};
    char               cd_status[160] = "Stopped.";
    CRITICAL_SECTION   cd_status_cs;
    rh_crash_ctx       cd_crash{};

    /* Thumbnail (worker-decoupled) */
    HANDLE                       th_thread       = nullptr;
    volatile LONG                th_running      = 0;
    CRITICAL_SECTION             th_cs;
    std::vector<unsigned char>   th_pending_bytes;
    char                         th_request_place[32] = {};
    char                         th_loaded_place[32]  = {};
    ID3D11ShaderResourceView    *th_srv          = nullptr;
    int                          th_w = 0, th_h = 0;

    /* Anti-AFK */
    rh_afk_ctx         afk{};

    /* Multi-instance */
    rh_singleton_ctx   mi{};
    long               mi_total_closed = 0;

    /* About popup flag */
    bool               want_about = false;
};

void app_state_init   (AppState &);
void app_state_destroy(AppState &);

/* Draws main menu bar + the four tabs inside the ImGui main window. */
void draw_root(AppState &);

/* Top-level helpers (called from menu, hotkeys, or close). */
void persist_settings (AppState &);
void import_settings  (AppState &);
void export_settings  (AppState &);

/* Worker controls (exposed for clean shutdown). */
void stop_all_workers (AppState &);

} /* namespace rh::gui */
