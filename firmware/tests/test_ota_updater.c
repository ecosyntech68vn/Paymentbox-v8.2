/**
 * test_ota_updater.c — Unit tests cho JSON parser + version cmp.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

// Copy of functions under test (verbatim from ota_updater.c)

static bool json_get_string(const char *json, const char *key, char *out, size_t out_len) {
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

static bool json_get_bool(const char *json, const char *key) {
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

static float json_get_float(const char *json, const char *key, float def) {
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

static int version_cmp(const char *a, const char *b) {
    int va[3] = {0}, vb[3] = {0};
    sscanf(a, "%d.%d.%d", &va[0], &va[1], &va[2]);
    sscanf(b, "%d.%d.%d", &vb[0], &vb[1], &vb[2]);
    for (int i = 0; i < 3; i++) {
        if (va[i] != vb[i]) return va[i] - vb[i];
    }
    return 0;
}

// ============== TESTS ==============

static int pass = 0, fail = 0;
#define ASSERT_EQ(a, b) do { \
    if ((a) == (b)) { pass++; } \
    else { fail++; printf("  FAIL %s:%d: expected %ld got %ld\n", __FILE__, __LINE__, (long)(b), (long)(a)); } \
} while(0)
#define ASSERT_STR(a, b) do { \
    if (strcmp((a),(b)) == 0) { pass++; } \
    else { fail++; printf("  FAIL %s:%d: expected '%s' got '%s'\n", __FILE__, __LINE__, b, a); } \
} while(0)
#define ASSERT_TRUE(a) ASSERT_EQ(!!(a), 1)
#define ASSERT_FALSE(a) ASSERT_EQ(!!(a), 0)
#define ASSERT_FLOAT(a, b) do { \
    if ((a) >= (b) - 0.01f && (a) <= (b) + 0.01f) { pass++; } \
    else { fail++; printf("  FAIL %s:%d: expected %.2f got %.2f\n", __FILE__, __LINE__, b, a); } \
} while(0)

static void test_json_string_basic(void) {
    printf("test_json_string_basic:\n");
    const char *j = "{\"version\":\"1.2.3\",\"url\":\"https://example.com/fw.bin\"}";
    char buf[64];
    ASSERT_TRUE(json_get_string(j, "version", buf, sizeof(buf)));
    ASSERT_STR(buf, "1.2.3");
    ASSERT_TRUE(json_get_string(j, "url", buf, sizeof(buf)));
    ASSERT_STR(buf, "https://example.com/fw.bin");
}

static void test_json_string_with_spaces(void) {
    printf("test_json_string_with_spaces:\n");
    const char *j = "{ \"version\" : \"2.0.0\" }";
    char buf[32];
    ASSERT_TRUE(json_get_string(j, "version", buf, sizeof(buf)));
    ASSERT_STR(buf, "2.0.0");
}

static void test_json_string_missing(void) {
    printf("test_json_string_missing:\n");
    const char *j = "{\"version\":\"1.0.0\"}";
    char buf[32];
    ASSERT_FALSE(json_get_string(j, "url", buf, sizeof(buf)));
}

static void test_json_string_truncation(void) {
    printf("test_json_string_truncation:\n");
    const char *j = "{\"url\":\"https://very-long-url-example.com/firmware.bin\"}";
    char buf[10];
    json_get_string(j, "url", buf, sizeof(buf));
    ASSERT_EQ(strlen(buf), 9);
}

static void test_json_string_empty(void) {
    printf("test_json_string_empty:\n");
    const char *j = "{\"version\":\"\"}";
    char buf[32];
    ASSERT_TRUE(json_get_string(j, "version", buf, sizeof(buf)));
    ASSERT_STR(buf, "");
}

static void test_json_bool_true(void) {
    printf("test_json_bool_true:\n");
    const char *j = "{\"update_available\":true,\"version\":\"1.1.0\"}";
    ASSERT_TRUE(json_get_bool(j, "update_available"));
}

static void test_json_bool_false(void) {
    printf("test_json_bool_false:\n");
    const char *j = "{\"update_available\":false}";
    ASSERT_FALSE(json_get_bool(j, "update_available"));
}

static void test_json_bool_missing(void) {
    printf("test_json_bool_missing:\n");
    const char *j = "{\"version\":\"1.0.0\"}";
    ASSERT_FALSE(json_get_bool(j, "update_available"));
}

static void test_json_float_basic(void) {
    printf("test_json_float_basic:\n");
    const char *j = "{\"min_battery_v\":3.5,\"max_temp\":80}";
    ASSERT_FLOAT(json_get_float(j, "min_battery_v", 0), 3.5f);
    ASSERT_FLOAT(json_get_float(j, "max_temp", 0), 80.0f);
}

static void test_json_float_default(void) {
    printf("test_json_float_default:\n");
    const char *j = "{\"version\":\"1.0.0\"}";
    ASSERT_FLOAT(json_get_float(j, "min_battery_v", 3.5f), 3.5f);
}

static void test_version_cmp_basic(void) {
    printf("test_version_cmp_basic:\n");
    ASSERT_TRUE(version_cmp("1.1.0", "1.0.0") > 0);
    ASSERT_TRUE(version_cmp("1.0.0", "1.1.0") < 0);
    ASSERT_EQ(version_cmp("1.0.0", "1.0.0"), 0);
}

static void test_version_cmp_patch(void) {
    printf("test_version_cmp_patch:\n");
    ASSERT_TRUE(version_cmp("1.0.5", "1.0.4") > 0);
    ASSERT_TRUE(version_cmp("1.0.4", "1.0.5") < 0);
}

static void test_version_cmp_major(void) {
    printf("test_version_cmp_major:\n");
    ASSERT_TRUE(version_cmp("2.0.0", "1.99.99") > 0);
    ASSERT_TRUE(version_cmp("1.99.99", "2.0.0") < 0);
}

static void test_version_cmp_partial(void) {
    printf("test_version_cmp_partial:\n");
    // "1.0" vs "1.0.0" — sscanf chỉ đọc được 2 numbers → patch=0
    ASSERT_EQ(version_cmp("1.0", "1.0.0"), 0);
    ASSERT_TRUE(version_cmp("1.1", "1.0.5") > 0);
}

// ============== Real GAS response simulation ==============

static void test_full_response_update_available(void) {
    printf("test_full_response_update_available:\n");
    const char *resp =
        "{\n"
        "  \"update_available\": true,\n"
        "  \"version\": \"1.1.0\",\n"
        "  \"url\": \"https://drive.google.com/uc?id=ABC/firmware.bin\",\n"
        "  \"sha256\": \"abc123def456\",\n"
        "  \"min_battery_v\": 3.5\n"
        "}";

    ASSERT_TRUE(json_get_bool(resp, "update_available"));
    char ver[16];
    ASSERT_TRUE(json_get_string(resp, "version", ver, sizeof(ver)));
    ASSERT_STR(ver, "1.1.0");
    char url[256];
    ASSERT_TRUE(json_get_string(resp, "url", url, sizeof(url)));
    ASSERT_STR(url, "https://drive.google.com/uc?id=ABC/firmware.bin");
    ASSERT_FLOAT(json_get_float(resp, "min_battery_v", 0), 3.5f);
}

static void test_full_response_no_update(void) {
    printf("test_full_response_no_update:\n");
    const char *resp = "{\"update_available\":false}";
    ASSERT_FALSE(json_get_bool(resp, "update_available"));
}

// ============== Downgrade protection ==============

static void test_downgrade_protection(void) {
    printf("test_downgrade_protection:\n");
    // Current FW: 1.0.0
    // Server sends 0.9.0 (should reject)
    ASSERT_TRUE(version_cmp("0.9.0", "1.0.0") < 0);
    // Server sends 1.0.0 (same — should reject)
    ASSERT_EQ(version_cmp("1.0.0", "1.0.0"), 0);
    // Server sends 1.0.1 (newer — accept)
    ASSERT_TRUE(version_cmp("1.0.1", "1.0.0") > 0);
}

int main(void) {
    test_json_string_basic();
    test_json_string_with_spaces();
    test_json_string_missing();
    test_json_string_truncation();
    test_json_string_empty();
    test_json_bool_true();
    test_json_bool_false();
    test_json_bool_missing();
    test_json_float_basic();
    test_json_float_default();
    test_version_cmp_basic();
    test_version_cmp_patch();
    test_version_cmp_major();
    test_version_cmp_partial();
    test_full_response_update_available();
    test_full_response_no_update();
    test_downgrade_protection();

    printf("\n========================================\n");
    printf("  Tests: %d pass / %d fail\n", pass, fail);
    printf("========================================\n");
    return fail > 0 ? 1 : 0;
}
