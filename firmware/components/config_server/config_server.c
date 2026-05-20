/**
 * config_server.c — HTTP server cho AP mode setup.
 *
 * Workflow:
 * 1. WiFi manager phát hiện không có NVS creds → start AP mode
 * 2. User connect AP "PaymentBox-Setup" (open)
 * 3. Phone tự bật captive portal hoặc user mở http://192.168.4.1
 * 4. Form HTML: SSID, PSK, phone IP, GAS URL
 * 5. POST /save → ghi NVS → reboot vào STA mode
 *
 * Bảo mật:
 * - AP mode chỉ active khi chưa có config
 * - Password không log
 * - Validate input length & charset (chống NVS overflow)
 */
#include "config_server.h"
#include "paymentbox_config.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "config_srv";
static httpd_handle_t s_server = NULL;

// =============== HTML page ===============
static const char *FORM_HTML =
"<!DOCTYPE html><html lang='vi'><head>"
"<meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>EcoSynTech PaymentBox Setup</title>"
"<style>"
"body{font-family:system-ui,sans-serif;max-width:500px;margin:20px auto;padding:0 16px}"
"h1{color:#0F5396;font-size:20px}"
"label{display:block;margin-top:12px;font-weight:600}"
"input{width:100%;padding:10px;font-size:16px;border:1px solid #ccc;border-radius:6px;margin-top:4px}"
"button{width:100%;padding:14px;font-size:16px;background:#0F5396;color:#fff;border:none;border-radius:6px;margin-top:20px;cursor:pointer}"
"button:hover{background:#0A3D6F}"
".hint{color:#666;font-size:13px;margin-top:4px}"
".status{padding:12px;background:#E5F0FF;border-radius:6px;margin-bottom:16px;font-size:14px}"
"</style></head><body>"
"<h1>EcoSynTech PaymentBox V8.2</h1>"
"<div class='status'>Thiết bị: <b id='did'>...</b><br>Trạng thái: <b>Đang chờ cấu hình</b></div>"
"<form action='/save' method='POST'>"
"<label>Tên WiFi (SSID)</label>"
"<input name='ssid' required maxlength='32' placeholder='VD: ECO_HOME_5G'>"
"<label>Mật khẩu WiFi</label>"
"<input name='psk' type='password' maxlength='64' placeholder='Để trống nếu WiFi mở'>"
"<label>IP của phone Android (cài BankNotify-App)</label>"
"<input name='phone_ip' required maxlength='15' placeholder='VD: 192.168.1.50'>"
"<p class='hint'>Mở app BankNotify, vào Settings → xem IP hiển thị</p>"
"<label>GAS Endpoint URL</label>"
"<input name='gas_url' maxlength='200' placeholder='https://script.google.com/macros/s/.../exec'>"
"<button type='submit'>Lưu &amp; Khởi động lại</button>"
"</form>"
"<p class='hint'>Sau khi lưu, thiết bị sẽ reboot và kết nối WiFi nhà.</p>"
"<script>fetch('/status').then(r=>r.json()).then(d=>{document.getElementById('did').textContent=d.device_id||'?'})</script>"
"</body></html>";

static const char *SAVED_HTML =
"<!DOCTYPE html><html lang='vi'><head><meta charset='utf-8'>"
"<title>Saved</title><meta http-equiv='refresh' content='5'>"
"<style>body{font-family:system-ui;max-width:500px;margin:50px auto;text-align:center}"
"h1{color:#7DC627}</style></head><body>"
"<h1>✓ Đã lưu cấu hình</h1>"
"<p>Thiết bị sẽ tự khởi động lại trong 2 giây...</p>"
"<p>Sau đó kết nối WiFi nhà của bạn.</p>"
"</body></html>";

// =============== Helpers ===============

/**
 * Validate IP-format string (xxx.xxx.xxx.xxx).
 * Returns true if valid.
 */
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

/**
 * URL-decode in-place. Replace + with space, %XX with byte.
 */
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

/**
 * Extract value of key from form-encoded body. Returns true if found.
 */
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

static esp_err_t save_to_nvs(const char *ssid, const char *psk,
                              const char *phone_ip, const char *gas_url) {
    nvs_handle_t h;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_str(h, "wifi.ssid", ssid);
    if (err != ESP_OK) goto out;
    err = nvs_set_str(h, "wifi.psk", psk ? psk : "");
    if (err != ESP_OK) goto out;
    err = nvs_set_str(h, "phone.ip", phone_ip);
    if (err != ESP_OK) goto out;
    if (gas_url && strlen(gas_url) > 0) {
        err = nvs_set_str(h, "gas.url", gas_url);
        if (err != ESP_OK) goto out;
    }
    err = nvs_commit(h);
out:
    nvs_close(h);
    return err;
}

// =============== Handlers ===============

static esp_err_t form_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req, FORM_HTML);
}

static esp_err_t status_get_handler(httpd_req_t *req) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char json[128];
    snprintf(json, sizeof(json),
        "{\"device_id\":\"%s%02X%02X%02X%02X\",\"hw\":\"%s\",\"fw\":\"%s\"}",
        DEVICE_ID_PREFIX, mac[2], mac[3], mac[4], mac[5],
        HW_VERSION, FW_VERSION);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static void delayed_restart_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Restarting now");
    esp_restart();
}

static esp_err_t save_post_handler(httpd_req_t *req) {
    // Đọc body — tối đa 1KB để chống DoS
    char buf[1024];
    int total = 0;
    int remaining = req->content_len;
    if (remaining > (int)sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Body too large");
        return ESP_FAIL;
    }
    while (remaining > 0) {
        int r = httpd_req_recv(req, buf + total, remaining);
        if (r <= 0) {
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Recv timeout");
            return ESP_FAIL;
        }
        total += r;
        remaining -= r;
    }
    buf[total] = '\0';

    // Parse fields
    char ssid[33] = "", psk[65] = "", phone_ip[16] = "", gas_url[201] = "";
    bool ok = get_form_field(buf, "ssid", ssid, sizeof(ssid));
    get_form_field(buf, "psk", psk, sizeof(psk));
    ok = ok && get_form_field(buf, "phone_ip", phone_ip, sizeof(phone_ip));
    get_form_field(buf, "gas_url", gas_url, sizeof(gas_url));

    if (!ok || strlen(ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_FAIL;
    }
    if (!valid_ipv4(phone_ip)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid phone IP");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Saving config: ssid=%s phone_ip=%s gas_url_len=%d",
             ssid, phone_ip, (int)strlen(gas_url));
    // KHÔNG log password.

    esp_err_t err = save_to_nvs(ssid, psk, phone_ip, gas_url);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS save failed: %d", err);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS save failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req, SAVED_HTML);

    // Schedule restart
    xTaskCreate(delayed_restart_task, "rst", 2048, NULL, 1, NULL);
    return ESP_OK;
}

// Captive portal — bất kỳ host khác đều redirect về /
static esp_err_t captive_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_sendstr(req, "");
}

// =============== Server start/stop ===============

bool config_server_start(void) {
    if (s_server) return true;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return false;
    }

    httpd_uri_t form_uri = { .uri = "/", .method = HTTP_GET,
                              .handler = form_get_handler };
    httpd_register_uri_handler(s_server, &form_uri);

    httpd_uri_t save_uri = { .uri = "/save", .method = HTTP_POST,
                              .handler = save_post_handler };
    httpd_register_uri_handler(s_server, &save_uri);

    httpd_uri_t status_uri = { .uri = "/status", .method = HTTP_GET,
                                .handler = status_get_handler };
    httpd_register_uri_handler(s_server, &status_uri);

    // Wildcard catch-all cho captive portal
    httpd_uri_t cap_uri = { .uri = "/*", .method = HTTP_GET,
                             .handler = captive_handler };
    httpd_register_uri_handler(s_server, &cap_uri);

    ESP_LOGI(TAG, "HTTP config server started on 192.168.4.1:80");
    return true;
}

void config_server_stop(void) {
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP config server stopped");
    }
}
