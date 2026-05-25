#include "uri.h"
#include <stdio.h>
#include <string.h>

size_t rh_build_uri(char *out, size_t cap, const char *place_id, const char *link_code) {
    if (!out || cap == 0) return 0;
    out[0] = 0;
    int has_id   = (place_id  && place_id[0]);
    int has_code = (link_code && link_code[0]);
    int n = 0;
    if (has_id && has_code)
        n = snprintf(out, cap, "roblox://placeId=%s&linkCode=%s", place_id, link_code);
    else if (has_id)
        n = snprintf(out, cap, "roblox://placeId=%s", place_id);
    return (n < 0) ? 0 : (size_t)n;
}
