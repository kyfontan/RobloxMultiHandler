#ifndef RH_ANTI_AFK_H
#define RH_ANTI_AFK_H

#include <windows.h>
#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    HANDLE         thread;
    volatile LONG  running;
    int            interval_sec;
    rh_afk_mode    mode;
} rh_afk_ctx;

void rh_afk_init(rh_afk_ctx *ctx);
int  rh_afk_start(rh_afk_ctx *ctx, int interval_sec, rh_afk_mode mode);
void rh_afk_stop(rh_afk_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif
