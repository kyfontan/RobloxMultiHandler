#ifndef RH_SETTINGS_H
#define RH_SETTINGS_H

#include "favorites.h"
#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { RH_TU_SEC = 0, RH_TU_MIN = 1, RH_TU_HOUR = 2 } rh_time_unit;
typedef enum { RH_AFK_FKEY = 0, RH_AFK_WASD = 1, RH_AFK_MOUSE = 2 } rh_afk_mode;

typedef struct {
    /* Auto-Crash */
    int           ac_value;
    rh_time_unit  ac_unit;

    /* Crash Detect */
    int           cd_restart_delay;  /* seconds */
    int           cd_use_place_id;
    char          cd_place_id[32];
    int           cd_use_priv;
    char          cd_priv_code[160];

    /* Anti-AFK */
    int           afk_enabled;
    int           afk_interval;      /* seconds */
    rh_afk_mode   afk_mode;

    /* Multi-instance */
    int           mi_enabled;

    rh_fav_list   favorites;
} rh_settings;

void rh_settings_defaults(rh_settings *s);

/* Returns 1 on success. Path defaults to %LOCALAPPDATA%\RobloxHandler\settings.json
   when path is NULL. */
int  rh_settings_load(rh_settings *s, const char *path_or_null);
int  rh_settings_save(const rh_settings *s, const char *path_or_null);

/* Resolves the default settings path into out (cap bytes). Returns 1 on success. */
int  rh_settings_default_path(char *out, int cap);

#ifdef __cplusplus
}
#endif

#endif
