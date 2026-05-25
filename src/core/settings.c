#include "settings.h"
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

static void copy_field(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = 0; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

void rh_settings_defaults(rh_settings *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->ac_value         = 30;
    s->ac_unit          = RH_TU_SEC;
    s->cd_restart_delay = 3;
    s->afk_interval     = 300;
    s->afk_mode         = RH_AFK_FKEY;
}

int rh_settings_default_path(char *out, int cap) {
    if (!out || cap <= 0) return 0;
    char appdata[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata)))
        return 0;
    char dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%s\\RobloxHandler", appdata);
    CreateDirectoryA(dir, NULL);
    snprintf(out, cap, "%s\\settings.json", dir);
    return 1;
}

int rh_settings_load(rh_settings *s, const char *path_or_null) {
    if (!s) return 0;
    rh_settings_defaults(s);
    char buf[MAX_PATH];
    const char *path = path_or_null;
    if (!path) {
        if (!rh_settings_default_path(buf, sizeof(buf))) return 0;
        path = buf;
    }
    rh_json *root = rh_json_load_file(path);
    if (!root) return 0;

    s->ac_value         = (int)rh_json_num_or(rh_json_get(root, "ac_value"),         s->ac_value);
    s->ac_unit          = (rh_time_unit)(int)rh_json_num_or(rh_json_get(root, "ac_unit"), s->ac_unit);
    s->cd_restart_delay = (int)rh_json_num_or(rh_json_get(root, "cd_restart_delay"), s->cd_restart_delay);
    s->cd_use_place_id  = rh_json_bool_or(rh_json_get(root, "cd_use_place_id"),      s->cd_use_place_id);
    copy_field(s->cd_place_id,  sizeof(s->cd_place_id),
               rh_json_str_or(rh_json_get(root, "cd_place_id"), s->cd_place_id));
    s->cd_use_priv      = rh_json_bool_or(rh_json_get(root, "cd_use_priv"),          s->cd_use_priv);
    copy_field(s->cd_priv_code, sizeof(s->cd_priv_code),
               rh_json_str_or(rh_json_get(root, "cd_priv_code"), s->cd_priv_code));
    s->afk_enabled      = rh_json_bool_or(rh_json_get(root, "afk_enabled"),          s->afk_enabled);
    s->afk_interval     = (int)rh_json_num_or(rh_json_get(root, "afk_interval"),     s->afk_interval);
    s->afk_mode         = (rh_afk_mode)(int)rh_json_num_or(rh_json_get(root, "afk_mode"), s->afk_mode);
    s->mi_enabled       = rh_json_bool_or(rh_json_get(root, "mi_enabled"),           s->mi_enabled);
    rh_fav_from_json(&s->favorites, rh_json_get(root, "favorites"));

    rh_json_free(root);
    return 1;
}

int rh_settings_save(const rh_settings *s, const char *path_or_null) {
    if (!s) return 0;
    char buf[MAX_PATH];
    const char *path = path_or_null;
    if (!path) {
        if (!rh_settings_default_path(buf, sizeof(buf))) return 0;
        path = buf;
    }
    rh_json *root = rh_json_new_obj();
    if (!root) return 0;
    rh_json_add(root, "ac_value",         rh_json_new_num(s->ac_value));
    rh_json_add(root, "ac_unit",          rh_json_new_num((int)s->ac_unit));
    rh_json_add(root, "cd_restart_delay", rh_json_new_num(s->cd_restart_delay));
    rh_json_add(root, "cd_use_place_id",  rh_json_new_bool(s->cd_use_place_id));
    rh_json_add(root, "cd_place_id",      rh_json_new_str(s->cd_place_id));
    rh_json_add(root, "cd_use_priv",      rh_json_new_bool(s->cd_use_priv));
    rh_json_add(root, "cd_priv_code",     rh_json_new_str(s->cd_priv_code));
    rh_json_add(root, "afk_enabled",      rh_json_new_bool(s->afk_enabled));
    rh_json_add(root, "afk_interval",     rh_json_new_num(s->afk_interval));
    rh_json_add(root, "afk_mode",         rh_json_new_num((int)s->afk_mode));
    rh_json_add(root, "mi_enabled",       rh_json_new_bool(s->mi_enabled));
    rh_json_add(root, "favorites",        rh_fav_to_json(&s->favorites));
    int ok = rh_json_save_file(path, root);
    rh_json_free(root);
    return ok;
}
