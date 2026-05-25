#ifndef RH_SLEEP_H
#define RH_SLEEP_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sleep in 100 ms slices, bail early if *flag stops being 1.
   Returns FALSE if interrupted, TRUE if full duration elapsed. */
static __inline BOOL rh_interruptible_sleep(volatile LONG *flag, int total_ms) {
    int slept = 0;
    while (slept < total_ms) {
        if (InterlockedCompareExchange(flag, 1, 1) != 1) return FALSE;
        int step = (total_ms - slept > 100) ? 100 : (total_ms - slept);
        Sleep(step);
        slept += step;
    }
    return TRUE;
}

#ifdef __cplusplus
}
#endif

#endif
