/*
 * Tiny JSON reader/writer for settings. ASCII strings only.
 * Supports: objects, arrays, strings, numbers (int/double), booleans, null.
 * Does NOT support: \u escapes beyond \" \\ \/ \n \r \t.
 */
#ifndef RH_JSON_H
#define RH_JSON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RH_J_NULL, RH_J_BOOL, RH_J_NUM, RH_J_STR, RH_J_ARR, RH_J_OBJ
} rh_json_type;

typedef struct rh_json {
    rh_json_type type;
    char  *key;          /* set when child of an object */
    char  *str;          /* RH_J_STR */
    double num;          /* RH_J_NUM */
    int    boolean;      /* RH_J_BOOL */
    struct rh_json *child;  /* RH_J_ARR / RH_J_OBJ first child */
    struct rh_json *next;
} rh_json;

rh_json *rh_json_parse(const char *src);              /* NULL on error */
char    *rh_json_print(const rh_json *node);          /* malloc'd; caller frees */
void     rh_json_free(rh_json *node);

rh_json *rh_json_new_obj(void);
rh_json *rh_json_new_arr(void);
rh_json *rh_json_new_str(const char *s);
rh_json *rh_json_new_num(double n);
rh_json *rh_json_new_bool(int b);
rh_json *rh_json_new_null(void);

void     rh_json_add(rh_json *parent, const char *key, rh_json *child);
rh_json *rh_json_get(const rh_json *obj, const char *key);
const char *rh_json_str_or(const rh_json *node, const char *fallback);
double      rh_json_num_or(const rh_json *node, double fallback);
int         rh_json_bool_or(const rh_json *node, int fallback);

/* File helpers */
rh_json *rh_json_load_file(const char *path);
int      rh_json_save_file(const char *path, const rh_json *root);

#ifdef __cplusplus
}
#endif

#endif
