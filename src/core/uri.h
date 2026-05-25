#ifndef RH_URI_H
#define RH_URI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Build a roblox:// launch URI. Either field may be empty/NULL.
   Returns number of chars written (excluding NUL), 0 if nothing built. */
size_t rh_build_uri(char *out, size_t cap,
                    const char *place_id,
                    const char *link_code);

#ifdef __cplusplus
}
#endif

#endif
