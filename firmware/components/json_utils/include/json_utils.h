#pragma once
#include <stdbool.h>
#include <stddef.h>

bool json_get_string(const char *json, const char *key, char *out, size_t out_len);
bool json_get_bool(const char *json, const char *key);
int  json_get_int(const char *json, const char *key, int def);
float json_get_float(const char *json, const char *key, float def);
