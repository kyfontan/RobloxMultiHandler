#ifndef RH_THUMBNAIL_H
#define RH_THUMBNAIL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fetches a 150x150 PNG thumbnail for the given Roblox place ID.
   On success: *out_bytes is set to a malloc'd buffer of *out_len PNG bytes
   (caller frees with free()), and returns 1. On failure, returns 0 and
   leaves the out pointers untouched. Uses an on-disk cache under
   %LOCALAPPDATA%\RobloxHandler\thumbs\. Safe to call on a worker thread. */
int rh_thumbnail_fetch_bytes(const char *place_id,
                             unsigned char **out_bytes,
                             size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif
