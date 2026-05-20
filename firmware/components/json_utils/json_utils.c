#include "json_utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

bool json_get_string(const char *json, const char *key, char *out, size_t out_len) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p = strchr(p + strlen(search), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return false;
    p++;
    const char *end = strchr(p, '"');
    if (!end) return false;
    size_t len = end - p;
    if (len >= out_len) len = out_len - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

bool json_get_bool(const char *json, const char *key) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p = strchr(p + strlen(search), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return (strncmp(p, "true", 4) == 0);
}

int json_get_int(const char *json, const char *key, int def) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return def;
    p = strchr(p + strlen(search), ':');
    if (!p) return def;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return atoi(p);
}

float json_get_float(const char *json, const char *key, float def) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return def;
    p = strchr(p + strlen(search), ':');
    if (!p) return def;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return strtof(p, NULL);
}
