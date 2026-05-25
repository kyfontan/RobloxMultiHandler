#ifndef RH_PROCESS_H
#define RH_PROCESS_H

#include <windows.h>

#define RH_ROBLOX_EXE "RobloxPlayerBeta.exe"

#ifdef __cplusplus
extern "C" {
#endif

BOOL  rh_is_roblox_running(void);
int   rh_kill_roblox(void);                    /* returns # of processes terminated */
int   rh_enum_roblox_pids(DWORD *out, int cap); /* fills out, returns count */

#ifdef __cplusplus
}
#endif

#endif
