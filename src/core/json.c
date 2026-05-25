#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *r = (char *)malloc(n);
    if (r) memcpy(r, s, n);
    return r;
}

/* ---------- Constructors ---------- */

static rh_json *node_new(rh_json_type t) {
    rh_json *n = (rh_json *)calloc(1, sizeof(rh_json));
    if (n) n->type = t;
    return n;
}

rh_json *rh_json_new_obj(void)  { return node_new(RH_J_OBJ);  }
rh_json *rh_json_new_arr(void)  { return node_new(RH_J_ARR);  }
rh_json *rh_json_new_null(void) { return node_new(RH_J_NULL); }
rh_json *rh_json_new_str(const char *s) {
    rh_json *n = node_new(RH_J_STR);
    if (n) n->str = xstrdup(s ? s : "");
    return n;
}
rh_json *rh_json_new_num(double v)  { rh_json *n = node_new(RH_J_NUM);  if (n) n->num = v;     return n; }
rh_json *rh_json_new_bool(int b)    { rh_json *n = node_new(RH_J_BOOL); if (n) n->boolean = b ? 1 : 0; return n; }

void rh_json_add(rh_json *parent, const char *key, rh_json *child) {
    if (!parent || !child) return;
    if (parent->type == RH_J_OBJ && key) {
        free(child->key);
        child->key = xstrdup(key);
    }
    if (!parent->child) {
        parent->child = child;
    } else {
        rh_json *p = parent->child;
        while (p->next) p = p->next;
        p->next = child;
    }
}

rh_json *rh_json_get(const rh_json *obj, const char *key) {
    if (!obj || obj->type != RH_J_OBJ || !key) return NULL;
    for (rh_json *c = obj->child; c; c = c->next)
        if (c->key && strcmp(c->key, key) == 0) return c;
    return NULL;
}

const char *rh_json_str_or(const rh_json *n, const char *fb) {
    return (n && n->type == RH_J_STR && n->str) ? n->str : fb;
}
double rh_json_num_or(const rh_json *n, double fb) {
    return (n && n->type == RH_J_NUM) ? n->num : fb;
}
int rh_json_bool_or(const rh_json *n, int fb) {
    if (!n) return fb;
    if (n->type == RH_J_BOOL) return n->boolean;
    if (n->type == RH_J_NUM)  return n->num != 0.0;
    return fb;
}

void rh_json_free(rh_json *n) {
    while (n) {
        rh_json *nx = n->next;
        rh_json_free(n->child);
        free(n->key);
        free(n->str);
        free(n);
        n = nx;
    }
}

/* ---------- Parser ---------- */

typedef struct {
    const char *p;
    const char *end;
} src_t;

static void skip_ws(src_t *s) {
    while (s->p < s->end && isspace((unsigned char)*s->p)) s->p++;
}

static rh_json *parse_value(src_t *s);

static char *parse_string(src_t *s) {
    if (s->p >= s->end || *s->p != '"') return NULL;
    s->p++;
    size_t cap = 32, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    while (s->p < s->end && *s->p != '"') {
        char c = *s->p++;
        if (c == '\\' && s->p < s->end) {
            char esc = *s->p++;
            switch (esc) {
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                default:   c = esc;  break; /* tolerant */
            }
        }
        if (len + 2 > cap) { cap *= 2; char *nb = (char *)realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
        buf[len++] = c;
    }
    if (s->p < s->end && *s->p == '"') s->p++;
    buf[len] = 0;
    return buf;
}

static rh_json *parse_number(src_t *s) {
    char tmp[64];
    size_t i = 0;
    while (s->p < s->end && i < sizeof(tmp)-1) {
        char c = *s->p;
        if (c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E' ||
            (c >= '0' && c <= '9')) {
            tmp[i++] = c;
            s->p++;
        } else break;
    }
    tmp[i] = 0;
    rh_json *n = node_new(RH_J_NUM);
    if (n) n->num = atof(tmp);
    return n;
}

static rh_json *parse_object(src_t *s) {
    s->p++; /* '{' */
    rh_json *obj = node_new(RH_J_OBJ);
    if (!obj) return NULL;
    skip_ws(s);
    if (s->p < s->end && *s->p == '}') { s->p++; return obj; }
    for (;;) {
        skip_ws(s);
        char *key = parse_string(s);
        if (!key) { rh_json_free(obj); return NULL; }
        skip_ws(s);
        if (s->p >= s->end || *s->p != ':') { free(key); rh_json_free(obj); return NULL; }
        s->p++;
        rh_json *v = parse_value(s);
        if (!v) { free(key); rh_json_free(obj); return NULL; }
        v->key = key;
        if (!obj->child) obj->child = v;
        else { rh_json *p = obj->child; while (p->next) p = p->next; p->next = v; }
        skip_ws(s);
        if (s->p < s->end && *s->p == ',') { s->p++; continue; }
        if (s->p < s->end && *s->p == '}') { s->p++; break; }
        rh_json_free(obj); return NULL;
    }
    return obj;
}

static rh_json *parse_array(src_t *s) {
    s->p++; /* '[' */
    rh_json *arr = node_new(RH_J_ARR);
    if (!arr) return NULL;
    skip_ws(s);
    if (s->p < s->end && *s->p == ']') { s->p++; return arr; }
    for (;;) {
        rh_json *v = parse_value(s);
        if (!v) { rh_json_free(arr); return NULL; }
        if (!arr->child) arr->child = v;
        else { rh_json *p = arr->child; while (p->next) p = p->next; p->next = v; }
        skip_ws(s);
        if (s->p < s->end && *s->p == ',') { s->p++; continue; }
        if (s->p < s->end && *s->p == ']') { s->p++; break; }
        rh_json_free(arr); return NULL;
    }
    return arr;
}

static rh_json *parse_value(src_t *s) {
    skip_ws(s);
    if (s->p >= s->end) return NULL;
    char c = *s->p;
    if (c == '{') return parse_object(s);
    if (c == '[') return parse_array(s);
    if (c == '"') {
        char *str = parse_string(s);
        if (!str) return NULL;
        rh_json *n = node_new(RH_J_STR);
        if (n) n->str = str; else free(str);
        return n;
    }
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.')
        return parse_number(s);
    if (s->end - s->p >= 4 && strncmp(s->p, "true", 4) == 0) {
        s->p += 4; return rh_json_new_bool(1);
    }
    if (s->end - s->p >= 5 && strncmp(s->p, "false", 5) == 0) {
        s->p += 5; return rh_json_new_bool(0);
    }
    if (s->end - s->p >= 4 && strncmp(s->p, "null", 4) == 0) {
        s->p += 4; return rh_json_new_null();
    }
    return NULL;
}

rh_json *rh_json_parse(const char *src) {
    if (!src) return NULL;
    src_t s = { src, src + strlen(src) };
    return parse_value(&s);
}

/* ---------- Printer ---------- */

typedef struct {
    char *buf;
    size_t len, cap;
} sb_t;

static void sb_grow(sb_t *b, size_t add) {
    if (b->len + add + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 256;
        while (b->len + add + 1 > nc) nc *= 2;
        char *nb = (char *)realloc(b->buf, nc);
        if (!nb) return;
        b->buf = nb; b->cap = nc;
    }
}

static void sb_add(sb_t *b, const char *s, size_t n) {
    sb_grow(b, n);
    if (!b->buf) return;
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = 0;
}
static void sb_addc(sb_t *b, char c) { sb_add(b, &c, 1); }
static void sb_addz(sb_t *b, const char *s) { sb_add(b, s, strlen(s)); }

static void sb_add_indent(sb_t *b, int d) {
    for (int i = 0; i < d; i++) sb_addz(b, "  ");
}

static void sb_add_jstring(sb_t *b, const char *s) {
    sb_addc(b, '"');
    if (s) for (; *s; s++) {
        char c = *s;
        switch (c) {
            case '"':  sb_addz(b, "\\\""); break;
            case '\\': sb_addz(b, "\\\\"); break;
            case '\n': sb_addz(b, "\\n");  break;
            case '\r': sb_addz(b, "\\r");  break;
            case '\t': sb_addz(b, "\\t");  break;
            default:
                if ((unsigned char)c < 0x20) {
                    char tmp[8]; snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)c);
                    sb_addz(b, tmp);
                } else sb_addc(b, c);
        }
    }
    sb_addc(b, '"');
}

static void print_node(sb_t *b, const rh_json *n, int depth) {
    if (!n) { sb_addz(b, "null"); return; }
    switch (n->type) {
        case RH_J_NULL: sb_addz(b, "null"); break;
        case RH_J_BOOL: sb_addz(b, n->boolean ? "true" : "false"); break;
        case RH_J_NUM: {
            char tmp[64];
            double d = n->num;
            double r = (double)(long)d;
            if (d == r && d >= -2147483648.0 && d <= 2147483647.0)
                snprintf(tmp, sizeof(tmp), "%ld", (long)d);
            else
                snprintf(tmp, sizeof(tmp), "%.10g", d);
            sb_addz(b, tmp);
            break;
        }
        case RH_J_STR: sb_add_jstring(b, n->str); break;
        case RH_J_ARR: {
            sb_addc(b, '[');
            if (n->child) sb_addc(b, '\n');
            for (rh_json *c = n->child; c; c = c->next) {
                sb_add_indent(b, depth + 1);
                print_node(b, c, depth + 1);
                if (c->next) sb_addc(b, ',');
                sb_addc(b, '\n');
            }
            if (n->child) sb_add_indent(b, depth);
            sb_addc(b, ']');
            break;
        }
        case RH_J_OBJ: {
            sb_addc(b, '{');
            if (n->child) sb_addc(b, '\n');
            for (rh_json *c = n->child; c; c = c->next) {
                sb_add_indent(b, depth + 1);
                sb_add_jstring(b, c->key ? c->key : "");
                sb_addz(b, ": ");
                print_node(b, c, depth + 1);
                if (c->next) sb_addc(b, ',');
                sb_addc(b, '\n');
            }
            if (n->child) sb_add_indent(b, depth);
            sb_addc(b, '}');
            break;
        }
    }
}

char *rh_json_print(const rh_json *node) {
    sb_t b = {0};
    print_node(&b, node, 0);
    return b.buf;
}

/* ---------- File IO ---------- */

rh_json *rh_json_load_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[r] = 0;
    rh_json *root = rh_json_parse(buf);
    free(buf);
    return root;
}

int rh_json_save_file(const char *path, const rh_json *root) {
    char *txt = rh_json_print(root);
    if (!txt) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) { free(txt); return 0; }
    size_t n = strlen(txt);
    size_t w = fwrite(txt, 1, n, f);
    fclose(f);
    free(txt);
    return w == n;
}
