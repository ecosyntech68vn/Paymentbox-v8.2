/**
 * test_config_server.c — Unit tests cho pure logic functions trong config_server.c
 *
 * Test: valid_ipv4, url_decode, get_form_field
 * Compile: gcc -DTEST_MAIN test_config_server.c -o /tmp/test && /tmp/test
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

// Copy of functions under test (verbatim from config_server.c)

static bool valid_ipv4(const char *s) {
    if (!s) return false;
    int dots = 0, octet = 0, digits = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '.') {
            if (digits == 0 || octet > 255) return false;
            dots++;
            octet = 0;
            digits = 0;
        } else if (*p >= '0' && *p <= '9') {
            octet = octet * 10 + (*p - '0');
            digits++;
            if (digits > 3) return false;
        } else {
            return false;
        }
    }
    return dots == 3 && digits > 0 && octet <= 255;
}

static void url_decode(char *s) {
    char *out = s;
    while (*s) {
        if (*s == '+') {
            *out++ = ' ';
            s++;
        } else if (*s == '%' && s[1] && s[2]) {
            char hex[3] = { s[1], s[2], 0 };
            *out++ = (char)strtol(hex, NULL, 16);
            s += 3;
        } else {
            *out++ = *s++;
        }
    }
    *out = '\0';
}

static bool get_form_field(const char *body, const char *key, char *out, size_t out_len) {
    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    const char *p = strstr(body, search);
    if (!p) return false;
    p += strlen(search);
    const char *end = strchr(p, '&');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len >= out_len) len = out_len - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    url_decode(out);
    return true;
}

// ============== TESTS ==============

static int pass = 0, fail = 0;

#define TEST(name) static void test_##name(void)
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

TEST(ipv4_valid) {
    printf("test_ipv4_valid:\n");
    ASSERT_TRUE(valid_ipv4("192.168.1.50"));
    ASSERT_TRUE(valid_ipv4("0.0.0.0"));
    ASSERT_TRUE(valid_ipv4("255.255.255.255"));
    ASSERT_TRUE(valid_ipv4("10.0.0.1"));
    ASSERT_TRUE(valid_ipv4("172.16.50.123"));
}

TEST(ipv4_invalid) {
    printf("test_ipv4_invalid:\n");
    ASSERT_FALSE(valid_ipv4(""));
    ASSERT_FALSE(valid_ipv4(NULL));
    ASSERT_FALSE(valid_ipv4("192.168.1"));        // missing octet
    ASSERT_FALSE(valid_ipv4("192.168.1.50.5"));   // too many
    ASSERT_FALSE(valid_ipv4("256.0.0.0"));        // octet overflow
    ASSERT_FALSE(valid_ipv4("192.168.a.1"));      // non-digit
    ASSERT_FALSE(valid_ipv4("192.168..1"));       // empty octet
    ASSERT_FALSE(valid_ipv4("192.168.1.0a"));     // trailing junk
    ASSERT_FALSE(valid_ipv4("1234.1.1.1"));       // octet too many digits
}

TEST(url_decode_basic) {
    printf("test_url_decode_basic:\n");
    char s[] = "hello+world";
    url_decode(s);
    ASSERT_STR(s, "hello world");
}

TEST(url_decode_percent) {
    printf("test_url_decode_percent:\n");
    char s[] = "ECO%5FHOME";
    url_decode(s);
    ASSERT_STR(s, "ECO_HOME");
}

TEST(url_decode_unicode_vn) {
    printf("test_url_decode_unicode_vn:\n");
    // Vietnamese "Mật khẩu" URL-encoded UTF-8: M%E1%BA%ADt+kh%E1%BA%A9u
    char s[] = "M%E1%BA%ADt+kh%E1%BA%A9u";
    url_decode(s);
    // Sau decode, sẽ là UTF-8 bytes của "Mật khẩu"
    ASSERT_STR(s, "M\xE1\xBA\xADt kh\xE1\xBA\xA9u");
}

TEST(url_decode_empty) {
    printf("test_url_decode_empty:\n");
    char s[] = "";
    url_decode(s);
    ASSERT_STR(s, "");
}

TEST(form_field_basic) {
    printf("test_form_field_basic:\n");
    const char *body = "ssid=MyWifi&psk=secret123&phone_ip=192.168.1.50";
    char buf[64];
    ASSERT_TRUE(get_form_field(body, "ssid", buf, sizeof(buf)));
    ASSERT_STR(buf, "MyWifi");
    ASSERT_TRUE(get_form_field(body, "psk", buf, sizeof(buf)));
    ASSERT_STR(buf, "secret123");
    ASSERT_TRUE(get_form_field(body, "phone_ip", buf, sizeof(buf)));
    ASSERT_STR(buf, "192.168.1.50");
}

TEST(form_field_url_encoded) {
    printf("test_form_field_url_encoded:\n");
    const char *body = "ssid=Eco+WiFi+5G&psk=p%40ssw%21rd";
    char buf[64];
    get_form_field(body, "ssid", buf, sizeof(buf));
    ASSERT_STR(buf, "Eco WiFi 5G");
    get_form_field(body, "psk", buf, sizeof(buf));
    ASSERT_STR(buf, "p@ssw!rd");
}

TEST(form_field_missing) {
    printf("test_form_field_missing:\n");
    const char *body = "ssid=WiFi";
    char buf[64];
    ASSERT_FALSE(get_form_field(body, "gas_url", buf, sizeof(buf)));
}

TEST(form_field_empty_value) {
    printf("test_form_field_empty_value:\n");
    const char *body = "ssid=WiFi&psk=&phone_ip=10.0.0.1";
    char buf[64];
    get_form_field(body, "psk", buf, sizeof(buf));
    ASSERT_STR(buf, "");
    get_form_field(body, "phone_ip", buf, sizeof(buf));
    ASSERT_STR(buf, "10.0.0.1");
}

TEST(form_field_truncation_safe) {
    printf("test_form_field_truncation_safe:\n");
    // Body có giá trị dài hơn buffer
    const char *body = "ssid=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    char buf[8];
    get_form_field(body, "ssid", buf, sizeof(buf));
    ASSERT_EQ(strlen(buf), 7);  // truncated to len-1
    ASSERT_STR(buf, "AAAAAAA");
}

TEST(form_field_last_field) {
    printf("test_form_field_last_field:\n");
    const char *body = "a=1&b=2&c=last_value";
    char buf[32];
    ASSERT_TRUE(get_form_field(body, "c", buf, sizeof(buf)));
    ASSERT_STR(buf, "last_value");
}

int main(void) {
    test_ipv4_valid();
    test_ipv4_invalid();
    test_url_decode_basic();
    test_url_decode_percent();
    test_url_decode_unicode_vn();
    test_url_decode_empty();
    test_form_field_basic();
    test_form_field_url_encoded();
    test_form_field_missing();
    test_form_field_empty_value();
    test_form_field_truncation_safe();
    test_form_field_last_field();

    printf("\n========================================\n");
    printf("  Tests: %d pass / %d fail\n", pass, fail);
    printf("========================================\n");
    return fail > 0 ? 1 : 0;
}
