#ifndef RH_CRASH_DETECT_H
#define RH_CRASH_DETECT_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RH_CRASH_NONE        = 0,
    RH_CRASH_PROCESS_GONE,
    RH_CRASH_LOG_ERROR,
    RH_CRASH_DIALOG_ERROR
} rh_crash_kind;

typedef struct {
    rh_crash_kind kind;
    char          reason[128];   /* short human-readable label */
} rh_crash_event;

typedef struct {
    HANDLE   log_file;
    LARGE_INTEGER log_offset;
    char     log_path[MAX_PATH];
} rh_crash_ctx;

void rh_crash_init(rh_crash_ctx *c);
void rh_crash_dispose(rh_crash_ctx *c);

/* Polls process/log/dialog state. Caller invokes ~1 Hz. Returns 1 if a crash
   event was produced (filled into *out), 0 otherwise. */
int  rh_crash_poll(rh_crash_ctx *c, rh_crash_event *out);

#ifdef __cplusplus
}
#endif

#endif
