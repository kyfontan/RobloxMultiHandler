#include "Tabs.hpp"
#include "Texture.hpp"

#include "../third_party/imgui/imgui.h"

extern "C" {
#include "../core/process.h"
#include "../core/uri.h"
#include "../core/sleep.h"
#include "../core/thumbnail.h"
}

#include <shellapi.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace rh::gui {

/* ============================================================
   Lifecycle
   ============================================================ */

void app_state_init(AppState &s) {
    InitializeCriticalSection(&s.cd_status_cs);
    InitializeCriticalSection(&s.th_cs);
    rh_crash_init(&s.cd_crash);
    rh_afk_init(&s.afk);
    rh_singleton_init(&s.mi);
    rh_settings_defaults(&s.settings);
    rh_settings_load(&s.settings, NULL);
}

static void stop_thumb_thread(AppState &s) {
    if (InterlockedCompareExchange(&s.th_running, 0, 1) == 1 && s.th_thread) {
        WaitForSingleObject(s.th_thread, 4000);
        CloseHandle(s.th_thread);
        s.th_thread = nullptr;
    }
}

static void stop_auto_crash(AppState &s);
static void stop_crash_detect(AppState &s);

void stop_all_workers(AppState &s) {
    stop_auto_crash(s);
    stop_crash_detect(s);
    rh_afk_stop(&s.afk);
    rh_singleton_stop(&s.mi);
    stop_thumb_thread(s);
}

void app_state_destroy(AppState &s) {
    stop_all_workers(s);
    if (s.th_srv) { s.th_srv->Release(); s.th_srv = nullptr; }
    rh_crash_dispose(&s.cd_crash);
    DeleteCriticalSection(&s.cd_status_cs);
    DeleteCriticalSection(&s.th_cs);
}

/* ============================================================
   Settings persistence
   ============================================================ */

void persist_settings(AppState &s) {
    rh_settings_save(&s.settings, NULL);
}

void import_settings(AppState &s) {
    char file[MAX_PATH] = "";
    OPENFILENAMEA ofn = { sizeof(ofn) };
    ofn.hwndOwner = s.host_hwnd;
    ofn.lpstrFilter = "JSON\0*.json\0All\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file);
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) return;
    if (!rh_settings_load(&s.settings, file))
        MessageBoxA(s.host_hwnd, "Failed to load JSON file.", "Import", MB_OK | MB_ICONERROR);
}

void export_settings(AppState &s) {
    char file[MAX_PATH] = "settings.json";
    OPENFILENAMEA ofn = { sizeof(ofn) };
    ofn.hwndOwner = s.host_hwnd;
    ofn.lpstrFilter = "JSON\0*.json\0All\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file);
    ofn.lpstrDefExt = "json";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameA(&ofn)) return;
    if (!rh_settings_save(&s.settings, file))
        MessageBoxA(s.host_hwnd, "Failed to save JSON file.", "Export", MB_OK | MB_ICONERROR);
}

/* ============================================================
   Auto-Crash worker
   ============================================================ */

static DWORD WINAPI auto_crash_worker(LPVOID p) {
    AppState *s = (AppState *)p;
    while (InterlockedCompareExchange(&s->ac_running, 1, 1) == 1) {
        if (rh_is_roblox_running()) {
            rh_kill_roblox();
            InterlockedIncrement(&s->ac_count);
        }
        if (!rh_interruptible_sleep(&s->ac_running, s->ac_delay_ms)) break;
    }
    return 0;
}

static int compute_delay_ms(int value, rh_time_unit unit) {
    if (value < 1) value = 1;
    if (unit == RH_TU_MIN)  return value * 60 * 1000;
    if (unit == RH_TU_HOUR) return value * 3600 * 1000;
    return value * 1000;
}

static void start_auto_crash(AppState &s) {
    if (InterlockedCompareExchange(&s.ac_running, 1, 0) != 0) return;
    s.ac_delay_ms = compute_delay_ms(s.settings.ac_value, s.settings.ac_unit);
    s.ac_count = 0;
    DWORD tid;
    s.ac_thread = CreateThread(NULL, 0, auto_crash_worker, &s, 0, &tid);
}

static void stop_auto_crash(AppState &s) {
    if (InterlockedCompareExchange(&s.ac_running, 0, 1) != 1) return;
    if (s.ac_thread) {
        WaitForSingleObject(s.ac_thread, 4000);
        CloseHandle(s.ac_thread);
        s.ac_thread = nullptr;
    }
}

/* ============================================================
   Crash Detect worker
   ============================================================ */

static void cd_set_status(AppState &s, const char *txt) {
    EnterCriticalSection(&s.cd_status_cs);
    snprintf(s.cd_status, sizeof(s.cd_status), "%s", txt);
    LeaveCriticalSection(&s.cd_status_cs);
}

static DWORD WINAPI crash_detect_worker(LPVOID p) {
    AppState *s = (AppState *)p;
    cd_set_status(*s, "Watching for Roblox process...");
    while (InterlockedCompareExchange(&s->cd_running, 1, 1) == 1) {
        rh_crash_event ev;
        if (rh_crash_poll(&s->cd_crash, &ev)) {
            InterlockedIncrement(&s->cd_count);
            int secs = s->cd_delay_ms / 1000;
            for (int i = secs; i > 0; i--) {
                if (InterlockedCompareExchange(&s->cd_running, 1, 1) != 1) return 0;
                char buf[160];
                snprintf(buf, sizeof(buf), "%s. Restart in %d s...", ev.reason, i);
                cd_set_status(*s, buf);
                Sleep(1000);
            }
            if (InterlockedCompareExchange(&s->cd_running, 1, 1) != 1) return 0;
            cd_set_status(*s, "Launching Roblox...");
            ShellExecuteA(NULL, "open",
                s->cd_uri[0] ? s->cd_uri : "roblox://", NULL, NULL, SW_SHOWNORMAL);
            int waited = 0;
            while (waited < 20000 && InterlockedCompareExchange(&s->cd_running, 1, 1) == 1) {
                Sleep(500);
                waited += 500;
                if (rh_is_roblox_running()) break;
            }
            cd_set_status(*s, "Watching for Roblox process...");
        }
        if (!rh_interruptible_sleep(&s->cd_running, 1000)) break;
    }
    cd_set_status(*s, "Stopped.");
    return 0;
}

static void build_cd_uri(AppState &s) {
    const char *pid  = s.settings.cd_use_place_id ? s.settings.cd_place_id : "";
    const char *code = s.settings.cd_use_priv     ? s.settings.cd_priv_code : "";
    rh_build_uri(s.cd_uri, sizeof(s.cd_uri), pid, code);
}

static void start_crash_detect(AppState &s) {
    if (InterlockedCompareExchange(&s.cd_running, 1, 0) != 0) return;
    int d = s.settings.cd_restart_delay;
    if (d < 1) d = 1;
    s.cd_delay_ms = d * 1000;
    s.cd_count = 0;
    build_cd_uri(s);
    DWORD tid;
    s.cd_thread = CreateThread(NULL, 0, crash_detect_worker, &s, 0, &tid);
}

static void stop_crash_detect(AppState &s) {
    if (InterlockedCompareExchange(&s.cd_running, 0, 1) != 1) return;
    if (s.cd_thread) {
        WaitForSingleObject(s.cd_thread, 5000);
        CloseHandle(s.cd_thread);
        s.cd_thread = nullptr;
    }
}

/* ============================================================
   Thumbnail pipeline
   ============================================================ */

static DWORD WINAPI thumb_worker(LPVOID p) {
    AppState *s = (AppState *)p;
    char want[32];
    EnterCriticalSection(&s->th_cs);
    snprintf(want, sizeof(want), "%s", s->th_request_place);
    LeaveCriticalSection(&s->th_cs);

    unsigned char *bytes = nullptr;
    size_t         len   = 0;
    if (rh_thumbnail_fetch_bytes(want, &bytes, &len)) {
        EnterCriticalSection(&s->th_cs);
        s->th_pending_bytes.assign(bytes, bytes + len);
        snprintf(s->th_loaded_place, sizeof(s->th_loaded_place), "%s", want);
        LeaveCriticalSection(&s->th_cs);
        free(bytes);
    }
    InterlockedExchange(&s->th_running, 0);
    return 0;
}

static void request_thumbnail(AppState &s, const char *place_id) {
    if (!place_id || !place_id[0]) return;
    EnterCriticalSection(&s.th_cs);
    bool same_as_loaded = strcmp(place_id, s.th_loaded_place) == 0;
    bool same_as_request = strcmp(place_id, s.th_request_place) == 0;
    LeaveCriticalSection(&s.th_cs);
    if (same_as_loaded) return;
    if (same_as_request && InterlockedCompareExchange(&s.th_running, 1, 1) == 1) return;

    stop_thumb_thread(s);

    EnterCriticalSection(&s.th_cs);
    snprintf(s.th_request_place, sizeof(s.th_request_place), "%s", place_id);
    LeaveCriticalSection(&s.th_cs);

    InterlockedExchange(&s.th_running, 1);
    DWORD tid;
    s.th_thread = CreateThread(NULL, 0, thumb_worker, &s, 0, &tid);
}

static void drain_thumbnail(AppState &s) {
    std::vector<unsigned char> bytes;
    EnterCriticalSection(&s.th_cs);
    if (!s.th_pending_bytes.empty()) bytes.swap(s.th_pending_bytes);
    LeaveCriticalSection(&s.th_cs);
    if (bytes.empty()) return;

    int w = 0, h = 0;
    ID3D11ShaderResourceView *srv =
        create_texture_from_image_bytes(s.device, bytes.data(), bytes.size(), &w, &h);
    if (!srv) return;
    if (s.th_srv) s.th_srv->Release();
    s.th_srv = srv;
    s.th_w = w;
    s.th_h = h;
}

/* ============================================================
   Helpers
   ============================================================ */

static bool is_running(volatile LONG *flag) {
    return InterlockedCompareExchange(flag, 1, 1) == 1;
}

static void section_header(const char *txt) {
    ImGui::Spacing();
    ImGui::SeparatorText(txt);
}

/* ============================================================
   Auto-Crash tab
   ============================================================ */

static void draw_auto_crash_tab(AppState &s) {
    ImGui::TextWrapped("Periodically force-closes Roblox at a regular interval.");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("What this does", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped(
            "Every N seconds (or minutes/hours), this kills the RobloxPlayerBeta.exe "
            "process. Useful for grinds that need a forced restart, for stress-testing "
            "the Crash Detect tab, or to leave a free-server farm running unattended.");
    }
    if (ImGui::CollapsingHeader("How to use", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Set the interval below.");
        ImGui::BulletText("Click 'Start auto-crash'. Roblox is killed every interval.");
        ImGui::BulletText("Click 'Stop' to halt the loop.");
        ImGui::TextDisabled("Tip: save your in-game progress first; the kill is instant.");
    }

    section_header("Settings");
    bool running = is_running(&s.ac_running);
    ImGui::BeginDisabled(running);
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Crash every", &s.settings.ac_value);
    if (s.settings.ac_value < 1) s.settings.ac_value = 1;
    static const char *units[] = { "seconds", "minutes", "hours" };
    int unit = (int)s.settings.ac_unit;
    ImGui::SetNextItemWidth(120);
    if (ImGui::Combo("Unit", &unit, units, 3)) s.settings.ac_unit = (rh_time_unit)unit;
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::BeginDisabled(running);
    if (ImGui::Button("Start auto-crash", ImVec2(140, 26))) start_auto_crash(s);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!running);
    if (ImGui::Button("Stop", ImVec2(80, 26))) stop_auto_crash(s);
    ImGui::EndDisabled();

    section_header("Status");
    ImGui::Text("Killed:  %ld", (long)s.ac_count);
    ImGui::Text("Status:  %s", running ? "Running" : "Stopped");
}

/* ============================================================
   Crash Detect tab
   ============================================================ */

static void draw_crash_detect_tab(AppState &s) {
    ImGui::TextWrapped("Watches Roblox; when it crashes/disconnects/AFK-kicks, relaunches it.");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("How it works", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped(
            "While running, the tool checks the Roblox process and tails the Roblox log "
            "file. On any crash/disconnect, it waits the restart delay below, then "
            "relaunches Roblox - optionally rejoining a specific place or private server.");
    }

    section_header("Restart settings");
    bool running = is_running(&s.cd_running);
    ImGui::BeginDisabled(running);
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Restart delay (seconds)", &s.settings.cd_restart_delay);
    if (s.settings.cd_restart_delay < 1) s.settings.cd_restart_delay = 1;
    ImGui::EndDisabled();

    section_header("Rejoin a specific game (optional)");
    bool use_id = s.settings.cd_use_place_id != 0;
    if (ImGui::Checkbox("Open with a specific Place ID", &use_id))
        s.settings.cd_use_place_id = use_id ? 1 : 0;

    ImGui::BeginDisabled(!use_id);
    ImGui::SetNextItemWidth(180);
    ImGui::InputText("Place ID", s.settings.cd_place_id, sizeof(s.settings.cd_place_id));
    ImGui::TextDisabled("(the number in the URL of a Roblox game page)");
    ImGui::EndDisabled();

    bool use_priv = s.settings.cd_use_priv != 0;
    if (ImGui::Checkbox("Private server (VIP) link code", &use_priv))
        s.settings.cd_use_priv = use_priv ? 1 : 0;
    ImGui::BeginDisabled(!use_priv);
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("Code", s.settings.cd_priv_code, sizeof(s.settings.cd_priv_code));
    ImGui::EndDisabled();

    /* Thumbnail */
    if (use_id && s.settings.cd_place_id[0]) {
        request_thumbnail(s, s.settings.cd_place_id);
    }
    drain_thumbnail(s);

    section_header("Preview");
    if (s.th_srv) {
        ImGui::Image((ImTextureID)(intptr_t)s.th_srv, ImVec2(150, 150));
    } else {
        ImGui::TextDisabled("(no thumbnail loaded -- enter a Place ID above)");
    }

    section_header("Favorites");
    {
        rh_fav_list &favs = s.settings.favorites;
        static int sel = -1;
        ImGui::BeginChild("##favlist", ImVec2(280, 110), ImGuiChildFlags_Border);
        for (int i = 0; i < favs.count; i++) {
            char line[256];
            if (favs.items[i].link_code[0])
                snprintf(line, sizeof(line), "%s  -  %s  [VIP]##%d",
                         favs.items[i].name, favs.items[i].place_id, i);
            else
                snprintf(line, sizeof(line), "%s  -  %s##%d",
                         favs.items[i].name, favs.items[i].place_id, i);
            if (ImGui::Selectable(line, sel == i, ImGuiSelectableFlags_AllowDoubleClick)) {
                sel = i;
                if (ImGui::IsMouseDoubleClicked(0)) {
                    snprintf(s.settings.cd_place_id, sizeof(s.settings.cd_place_id),
                             "%s", favs.items[i].place_id);
                    snprintf(s.settings.cd_priv_code, sizeof(s.settings.cd_priv_code),
                             "%s", favs.items[i].link_code);
                    s.settings.cd_use_place_id = 1;
                    s.settings.cd_use_priv = favs.items[i].link_code[0] ? 1 : 0;
                }
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginGroup();
        static char favname[RH_FAV_NAME] = "";
        ImGui::SetNextItemWidth(170);
        ImGui::InputText("Name", favname, sizeof(favname));
        if (ImGui::Button("Add", ImVec2(80, 22))) {
            if (favname[0] && s.settings.cd_place_id[0]) {
                rh_fav_add(&favs, favname, s.settings.cd_place_id,
                           s.settings.cd_use_priv ? s.settings.cd_priv_code : "");
                favname[0] = 0;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load", ImVec2(80, 22)) && sel >= 0 && sel < favs.count) {
            snprintf(s.settings.cd_place_id, sizeof(s.settings.cd_place_id),
                     "%s", favs.items[sel].place_id);
            snprintf(s.settings.cd_priv_code, sizeof(s.settings.cd_priv_code),
                     "%s", favs.items[sel].link_code);
            s.settings.cd_use_place_id = 1;
            s.settings.cd_use_priv = favs.items[sel].link_code[0] ? 1 : 0;
        }
        if (ImGui::Button("Remove", ImVec2(80, 22)) && sel >= 0 && sel < favs.count) {
            rh_fav_remove(&favs, sel);
            sel = -1;
        }
        ImGui::EndGroup();
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(running);
    if (ImGui::Button("Start watching", ImVec2(140, 26))) start_crash_detect(s);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!running);
    if (ImGui::Button("Stop", ImVec2(80, 26))) stop_crash_detect(s);
    ImGui::EndDisabled();

    section_header("Status");
    ImGui::Text("Restarts:  %ld", (long)s.cd_count);
    char status[200];
    EnterCriticalSection(&s.cd_status_cs);
    snprintf(status, sizeof(status), "%s", s.cd_status);
    LeaveCriticalSection(&s.cd_status_cs);
    ImGui::TextWrapped("Status:  %s", status);
}

/* ============================================================
   Anti-AFK tab
   ============================================================ */

static void apply_afk(AppState &s) {
    rh_afk_stop(&s.afk);
    if (s.settings.afk_enabled) {
        int iv = s.settings.afk_interval;
        if (iv < 5) iv = 5;
        rh_afk_start(&s.afk, iv, s.settings.afk_mode);
    }
}

static void draw_anti_afk_tab(AppState &s) {
    ImGui::TextWrapped("Defeats Roblox's 20-minute AFK kick by sending a tiny input on a timer.");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("How it works", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped(
            "Every N seconds the tool sends a single key press (or mouse jiggle) to "
            "Windows. The input only fires while the Roblox window is the foreground "
            "window, so it never disturbs other apps.");
    }
    if (ImGui::CollapsingHeader("Modes", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Random F-key (F22..F24): silent, no in-game effect. Recommended.");
        ImGui::BulletText("WASD jiggle: taps a movement key; your avatar may twitch.");
        ImGui::BulletText("Mouse jiggle: nudges the cursor by 1 pixel.");
    }

    section_header("Settings");
    bool enabled = s.settings.afk_enabled != 0;
    if (ImGui::Checkbox("Enable Anti-AFK", &enabled)) {
        s.settings.afk_enabled = enabled ? 1 : 0;
        apply_afk(s);
    }
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputInt("Interval (seconds)", &s.settings.afk_interval)) {
        if (s.settings.afk_interval < 5) s.settings.afk_interval = 5;
        if (s.settings.afk_enabled) apply_afk(s);
    }
    static const char *modes[] = {
        "Random F-key (F22..F24)", "WASD jiggle", "Mouse jiggle"
    };
    int m = (int)s.settings.afk_mode;
    ImGui::SetNextItemWidth(220);
    if (ImGui::Combo("Mode", &m, modes, 3)) {
        s.settings.afk_mode = (rh_afk_mode)m;
        if (s.settings.afk_enabled) apply_afk(s);
    }

    section_header("Status");
    ImGui::Text("Status:  %s",
        s.settings.afk_enabled ? "Active (only fires while Roblox is focused)"
                               : "Disabled");
}

/* ============================================================
   Multi-Instance tab
   ============================================================ */

static void apply_mi(AppState &s) {
    rh_singleton_stop(&s.mi);
    if (s.settings.mi_enabled) rh_singleton_start(&s.mi, 2000);
}

static void draw_multi_instance_tab(AppState &s) {
    ImGui::TextWrapped("Lets you run multiple Roblox windows on the same PC.");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("What this does", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped(
            "By default, Roblox only allows ONE running window per Windows session, "
            "enforced via two named kernel objects:");
        ImGui::BulletText("ROBLOX_singletonEvent");
        ImGui::BulletText("ROBLOX_singletonMutex");
        ImGui::TextWrapped(
            "This tool closes both so additional Roblox launches succeed. Enable the "
            "continuous mode if you plan to start more sessions later.");
    }
    if (ImGui::CollapsingHeader("How to use", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Launch your FIRST Roblox window normally.");
        ImGui::BulletText("Click 'Run once now' (or tick continuous mode).");
        ImGui::BulletText("Launch each additional Roblox window.");
    }

    section_header("Settings");
    bool enabled = s.settings.mi_enabled != 0;
    if (ImGui::Checkbox("Enable continuous multi-instance closer (every 2 s)", &enabled)) {
        s.settings.mi_enabled = enabled ? 1 : 0;
        apply_mi(s);
    }
    if (ImGui::Button("Run once now", ImVec2(120, 22))) {
        s.mi_total_closed += rh_singleton_close_once();
    }

    section_header("Status");
    ImGui::Text("Handles closed:  %ld", s.mi_total_closed);
    ImGui::Text("Status:  %s",
        s.settings.mi_enabled ? "Running (closes singleton objects every 2 s)"
                              : "Disabled");
}

/* ============================================================
   Menu bar + About
   ============================================================ */

static void draw_menu_bar(AppState &s) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Import settings...", "Ctrl+I")) import_settings(s);
            if (ImGui::MenuItem("Export settings...", "Ctrl+E")) export_settings(s);
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) PostMessage(s.host_hwnd, WM_CLOSE, 0, 0);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) s.want_about = true;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

static void draw_about_popup(AppState &s) {
    if (s.want_about) {
        ImGui::OpenPopup("About##rh");
        s.want_about = false;
    }
    ImVec2 vc = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(vc, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("About##rh", NULL,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Roblox Handler v3");
        ImGui::Separator();
        ImGui::TextWrapped("Auto-crash and crash-recovery utility for Roblox.");
        ImGui::TextWrapped("Frontend: Dear ImGui + DirectX 11");
        ImGui::TextWrapped("Backend:  pure C modules (process, JSON, crash detect, ...)");
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(80, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

/* ============================================================
   Root
   ============================================================ */

void draw_root(AppState &s) {
    ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("Roblox Handler##root", NULL, flags);
    draw_menu_bar(s);

    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("Auto-Crash"))     { draw_auto_crash_tab(s);     ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Crash Detect"))   { draw_crash_detect_tab(s);   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Anti-AFK"))       { draw_anti_afk_tab(s);       ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Multi-Instance")) { draw_multi_instance_tab(s); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    draw_about_popup(s);

    /* Hotkeys */
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_I)) import_settings(s);
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_E)) export_settings(s);

    ImGui::End();
}

} /* namespace rh::gui */
