#ifndef RH_SINGLETON_H
#define RH_SINGLETON_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Closes the ROBLOX_singletonEvent / ROBLOX_singletonMutex objects in
   \Sessions\<id>\BaseNamedObjects so additional Roblox instances can launch.
   Returns number of handles closed (0..n). */
int rh_singleton_close_once(void);

/* Background thread that re-runs close_once every interval_ms. */
typedef struct {
    HANDLE         thread;
    volatile LONG  running;
    int            interval_ms;
} rh_singleton_ctx;

void rh_singleton_init(rh_singleton_ctx *c);
int  rh_singleton_start(rh_singleton_ctx *c, int interval_ms);
void rh_singleton_stop(rh_singleton_ctx *c);

#ifdef __cplusplus
}
#endif

#endif
