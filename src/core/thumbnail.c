/*
 * Roblox place thumbnail fetcher (bytes-out, no GDI+ decode).
 *   GET https://thumbnails.roblox.com/v1/places/gameicons?placeIds=ID&size=150x150&format=Png&isCircular=false
 *   -> { "data": [ { "imageUrl": "https://..." } ] }
 *   Download imageUrl, return raw PNG bytes; caller decodes.
 */
#include "thumbnail.h"
#include "json.h"

#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char *data;
    size_t         len;
    size_t         cap;
} buf_t;

static int buf_append(buf_t *b, const void *src, size_t n) {
    if (b->len + n > b->cap) {
        size_t nc = b->cap ? b->cap : 4096;
        while (nc < b->len + n) nc *= 2;
        unsigned char *p = (unsigned char *)realloc(b->data, nc);
        if (!p) return 0;
        b->data = p;
        b->cap  = nc;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return 1;
}

static int http_get(const char *url, buf_t *out) {
    HINTERNET hInet = InternetOpenA("RobloxHandler/3.0",
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet) return 0;
    HINTERNET hUrl = InternetOpenUrlA(hInet, url, NULL, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_UI, 0);
    int ok = 0;
    if (hUrl) {
        unsigned char chunk[4096];
        DWORD r = 0;
        ok = 1;
        for (;;) {
            r = 0;
            if (!InternetReadFile(hUrl, chunk, sizeof(chunk), &r)) { ok = 0; break; }
            if (r == 0) break;
            if (!buf_append(out, chunk, r)) { ok = 0; break; }
        }
        if (ok && out->len == 0) ok = 0;
        InternetCloseHandle(hUrl);
    }
    InternetCloseHandle(hInet);
    return ok;
}

static int cache_path(const char *place_id, char *out, size_t cap) {
    char appdata[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata)))
        return 0;
    char dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%s\\RobloxHandler\\thumbs", appdata);
    SHCreateDirectoryExA(NULL, dir, NULL);
    snprintf(out, cap, "%s\\%s.png", dir, place_id);
    return 1;
}

static int load_file(const char *path, unsigned char **out_bytes, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return 0; }
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = (unsigned char *)malloc((size_t)sz);
    if (!buf) { fclose(f); return 0; }
    size_t r = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (r != (size_t)sz) { free(buf); return 0; }
    *out_bytes = buf;
    *out_len   = (size_t)sz;
    return 1;
}

static void save_file(const char *path, const unsigned char *bytes, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(bytes, 1, n, f);
    fclose(f);
}

int rh_thumbnail_fetch_bytes(const char *place_id,
                             unsigned char **out_bytes,
                             size_t *out_len) {
    if (!place_id || !place_id[0] || !out_bytes || !out_len) return 0;

    char cp[MAX_PATH] = "";
    int have_cache = cache_path(place_id, cp, sizeof(cp));
    if (have_cache && GetFileAttributesA(cp) != INVALID_FILE_ATTRIBUTES) {
        if (load_file(cp, out_bytes, out_len)) return 1;
    }

    char meta_url[256];
    snprintf(meta_url, sizeof(meta_url),
        "https://thumbnails.roblox.com/v1/places/gameicons?placeIds=%s&size=150x150&format=Png&isCircular=false",
        place_id);

    buf_t meta = {0};
    if (!http_get(meta_url, &meta)) { free(meta.data); return 0; }

    /* Parse JSON; need NUL terminator. */
    if (!buf_append(&meta, "", 1)) { free(meta.data); return 0; }

    rh_json *root = rh_json_parse((const char *)meta.data);
    free(meta.data);
    if (!root) return 0;

    const char *image_url = NULL;
    rh_json *data = rh_json_get(root, "data");
    if (data && data->type == RH_J_ARR && data->child)
        image_url = rh_json_str_or(rh_json_get(data->child, "imageUrl"), NULL);
    if (!image_url) { rh_json_free(root); return 0; }

    buf_t png = {0};
    int ok = http_get(image_url, &png);
    rh_json_free(root);
    if (!ok) { free(png.data); return 0; }

    if (have_cache) save_file(cp, png.data, png.len);

    *out_bytes = png.data;
    *out_len   = png.len;
    return 1;
}
