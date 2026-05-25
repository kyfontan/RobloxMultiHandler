#include "favorites.h"
#include <string.h>

static void copy_field(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = 0; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

void rh_fav_clear(rh_fav_list *list) {
    if (!list) return;
    list->count = 0;
}

int rh_fav_add(rh_fav_list *list, const char *name, const char *place_id, const char *link_code) {
    if (!list || list->count >= RH_FAV_MAX) return 0;
    rh_favorite *f = &list->items[list->count++];
    copy_field(f->name,      sizeof(f->name),      name);
    copy_field(f->place_id,  sizeof(f->place_id),  place_id);
    copy_field(f->link_code, sizeof(f->link_code), link_code);
    return 1;
}

int rh_fav_remove(rh_fav_list *list, int index) {
    if (!list || index < 0 || index >= list->count) return 0;
    for (int i = index; i < list->count - 1; i++)
        list->items[i] = list->items[i + 1];
    list->count--;
    return 1;
}

rh_json *rh_fav_to_json(const rh_fav_list *list) {
    rh_json *arr = rh_json_new_arr();
    if (!arr || !list) return arr;
    for (int i = 0; i < list->count; i++) {
        rh_json *o = rh_json_new_obj();
        rh_json_add(o, "name",     rh_json_new_str(list->items[i].name));
        rh_json_add(o, "placeId",  rh_json_new_str(list->items[i].place_id));
        rh_json_add(o, "linkCode", rh_json_new_str(list->items[i].link_code));
        rh_json_add(arr, NULL, o);
    }
    return arr;
}

void rh_fav_from_json(rh_fav_list *list, const rh_json *arr) {
    if (!list) return;
    list->count = 0;
    if (!arr || arr->type != RH_J_ARR) return;
    for (rh_json *c = arr->child; c && list->count < RH_FAV_MAX; c = c->next) {
        const char *name = rh_json_str_or(rh_json_get(c, "name"),     "");
        const char *pid  = rh_json_str_or(rh_json_get(c, "placeId"),  "");
        const char *code = rh_json_str_or(rh_json_get(c, "linkCode"), "");
        if (name[0] && pid[0])
            rh_fav_add(list, name, pid, code);
    }
}
