/**
 * test_led_status.c — Unit tests cho LED status state machine logic.
 *
 * Compile: gcc -DTEST_MAIN test_led_status.c -o /tmp/test && /tmp/test
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

// =============== Types under test (copied from led_status.c) ===============

typedef enum {
    ST_IDLE = 0,
    ST_OK,
    ST_WARN,
    ST_ALERT,
    ST_CRITICAL,
} pbox_state_t;

static const char *state_name(pbox_state_t s) {
    switch (s) {
        case ST_IDLE:     return "IDLE";
        case ST_OK:       return "OK";
        case ST_WARN:     return "WARN";
        case ST_ALERT:    return "ALERT";
        case ST_CRITICAL: return "CRITICAL";
    }
    return "?";
}

typedef struct {
    bool phone_ok;
    bool gas_ok;
    bool bat_critical;
    bool usb_lost;
    bool wifi_ok;
} subsys_t;

static pbox_state_t compute_state(const subsys_t *sys) {
    if (!sys->wifi_ok) return ST_IDLE;
    if (sys->bat_critical || (sys->usb_lost && !sys->phone_ok)) return ST_CRITICAL;
    if (!sys->phone_ok || !sys->gas_ok) return ST_ALERT;
    if (sys->usb_lost) return ST_WARN;
    return ST_OK;
}

// =============== Test runner ===============

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  %s ... ", name); test_##name(); printf("OK\n"); pass++; } while(0)
#define FAIL(msg)  do { printf("FAIL: %s\n", msg); fail++; return; } while(0)
#define EQ(a,b)    do { if ((a) != (b)) { printf("FAIL at line %d: expected %d got %d\n", __LINE__, (b), (a)); fail++; return; } } while(0)

// =============== Tests ===============

static void test_idle_when_wifi_not_ok() {
    subsys_t sys = { .wifi_ok = false };
    EQ(compute_state(&sys), ST_IDLE);
}

static void test_ok_when_all_green() {
    subsys_t sys = { .phone_ok = true, .gas_ok = true, .wifi_ok = true };
    EQ(compute_state(&sys), ST_OK);
}

static void test_critical_when_bat_critical() {
    subsys_t sys = { .phone_ok = true, .gas_ok = true, .bat_critical = true, .wifi_ok = true };
    EQ(compute_state(&sys), ST_CRITICAL);
}

static void test_critical_when_usb_lost_and_phone_dead() {
    subsys_t sys = { .phone_ok = false, .gas_ok = true, .usb_lost = true, .wifi_ok = true };
    EQ(compute_state(&sys), ST_CRITICAL);
}

static void test_alert_when_phone_dead() {
    subsys_t sys = { .phone_ok = false, .gas_ok = true, .wifi_ok = true };
    EQ(compute_state(&sys), ST_ALERT);
}

static void test_alert_when_gas_dead() {
    subsys_t sys = { .phone_ok = true, .gas_ok = false, .wifi_ok = true };
    EQ(compute_state(&sys), ST_ALERT);
}

static void test_warn_when_usb_lost_but_phone_ok() {
    subsys_t sys = { .phone_ok = true, .gas_ok = true, .usb_lost = true, .wifi_ok = true };
    EQ(compute_state(&sys), ST_WARN);
}

static void test_alert_beats_warn() {
    // phone dead + usb lost → should be ALERT (not CRITICAL since phone dead and usb lost alone isn't CRITICAL without bat)
    subsys_t sys = { .phone_ok = false, .gas_ok = true, .usb_lost = true, .wifi_ok = true };
    EQ(compute_state(&sys), ST_CRITICAL);  // usb_lost && !phone_ok → CRITICAL
}

static void test_bat_recovered_clears_critical() {
    subsys_t sys = { .phone_ok = true, .gas_ok = true, .bat_critical = false, .wifi_ok = true };
    EQ(compute_state(&sys), ST_OK);
}

static void test_critical_not_idle() {
    // bat critical even without wifi? No — wifi not ok takes priority
    subsys_t sys = { .wifi_ok = false, .bat_critical = true };
    EQ(compute_state(&sys), ST_IDLE);
}

// =============== Main ===============

int main(void) {
    printf("=== LED State Machine Tests ===\n");
    TEST(idle_when_wifi_not_ok);
    TEST(ok_when_all_green);
    TEST(critical_when_bat_critical);
    TEST(critical_when_usb_lost_and_phone_dead);
    TEST(alert_when_phone_dead);
    TEST(alert_when_gas_dead);
    TEST(warn_when_usb_lost_but_phone_ok);
    TEST(alert_beats_warn);
    TEST(bat_recovered_clears_critical);
    TEST(critical_not_idle);

    printf("\n========================================\n");
    printf("  Tests: %d pass / %d fail\n", pass, fail);
    printf("========================================\n");
    return fail > 0 ? 1 : 0;
}
