#ifndef RH_FAVORITES_H
#define RH_FAVORITES_H

#include "json.h"

#define RH_FAV_MAX        64
#define RH_FAV_NAME       64
#define RH_FAV_PLACE      32
#define RH_FAV_CODE       160

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[RH_FAV_NAME];
    char place_id[RH_FAV_PLACE];
    char link_code[RH_FAV_CODE];
} rh_favorite;

typedef struct {
    rh_favorite items[RH_FAV_MAX];
    int count;
} rh_fav_list;

void  rh_fav_clear(rh_fav_list *list);
int   rh_fav_add(rh_fav_list *list, const char *name, const char *place_id, const char *link_code);
int   rh_fav_remove(rh_fav_list *list, int index);
rh_json *rh_fav_to_json(const rh_fav_list *list);
void  rh_fav_from_json(rh_fav_list *list, const rh_json *arr);

#ifdef __cplusplus
}
#endif

#endif
