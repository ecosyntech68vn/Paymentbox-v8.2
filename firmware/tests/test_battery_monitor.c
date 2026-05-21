/**
 * test_battery_monitor.c — Unit tests cho battery threshold + USB lost detection.
 *
 * Compile: gcc -DTEST_MAIN test_battery_monitor.c -o /tmp/test && /tmp/test
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// =============== Thresholds (from paymentbox_config.h) ===============

#define BAT_FULL_V       4.10f
#define BAT_LOW_V        3.40f
#define BAT_CRITICAL_V   3.00f
#define BAT_DIVIDER_RATIO 3.13f
#define HISTORY_LEN       5

// =============== Logic under test ===============

/**
 * Simulate battery threshold state machine.
 * Returns: 0 = normal, 1 = low, 2 = critical, 3 = recovered
 */
static int check_battery_thresholds(float v, bool *low_sent, bool *critical_sent) {
    if (v < BAT_CRITICAL_V && !*critical_sent) {
        *critical_sent = true;
        return 2;
    }
    if (v < BAT_LOW_V && v >= BAT_CRITICAL_V && !*low_sent) {
        *low_sent = true;
        return 1;
    }
    if (v >= BAT_FULL_V && (*low_sent || *critical_sent)) {
        *low_sent = false;
        *critical_sent = false;
        return 3;
    }
    return 0;
}

/**
 * Simulate USB lost detection (debounce 3/5 consecutive drops).
 * Returns: 0 = normal, 1 = lost, 2 = restored
 */
static int check_usb_lost(float last_v, float v, int *drop_count, bool *usb_lost_sent) {
    if (last_v <= 0 || v <= 0) return 0;

    float dv = v - last_v;
    if (dv < -0.01f && v < 4.0f) {
        (*drop_count)++;
    } else if (dv > 0.05f) {
        *drop_count = 0;
    }
    if (*drop_count >= 3 && !*usb_lost_sent) {
        *usb_lost_sent = true;
        *drop_count = 0;
        return 1;
    }
    if (dv > 0.05f && *usb_lost_sent) {
        *usb_lost_sent = false;
        return 2;
    }
    return 0;
}

// =============== Test runner ===============

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  %s ... ", name); test_##name(); printf("OK\n"); pass++; } while(0)
#define EQ(a,b)    do { if ((a) != (b)) { printf("FAIL at line %d: expected %d got %d\n", __LINE__, (b), (a)); fail++; return; } } while(0)
#define TRUTHY(a)  do { if (!(a)) { printf("FAIL at line %d: expected truthy\n", __LINE__); fail++; return; } } while(0)
#define FALSY(a)   do { if ((a)) { printf("FAIL at line %d: expected falsy\n", __LINE__); fail++; return; } } while(0)

// =============== Battery threshold tests ===============

static void test_normal_voltage_no_alarm() {
    bool low = false, crit = false;
    EQ(check_battery_thresholds(3.7f, &low, &crit), 0);
    FALSY(low);
    FALSY(crit);
}

static void test_low_voltage_triggers_low() {
    bool low = false, crit = false;
    EQ(check_battery_thresholds(3.3f, &low, &crit), 1);
    TRUTHY(low);
    FALSY(crit);
}

static void test_critical_voltage_triggers_critical() {
    bool low = false, crit = false;
    EQ(check_battery_thresholds(2.9f, &low, &crit), 2);
    FALSY(low);
    TRUTHY(crit);
}

static void test_low_voltage_not_sent_twice() {
    bool low = false, crit = false;
    EQ(check_battery_thresholds(3.3f, &low, &crit), 1);
    EQ(check_battery_thresholds(3.2f, &low, &crit), 0);
    TRUTHY(low);
}

static void test_recovered_after_full_charge() {
    bool low = true, crit = false;
    EQ(check_battery_thresholds(4.2f, &low, &crit), 3);
    FALSY(low);
}

static void test_critical_then_recovered() {
    bool low = false, crit = true;
    EQ(check_battery_thresholds(4.2f, &low, &crit), 3);
    FALSY(crit);
    FALSY(low);
}

static void test_voltage_in_no_mans_land() {
    // Between CRITICAL and LOW, with no flag set → no event
    bool low = false, crit = false;
    EQ(check_battery_thresholds(3.1f, &low, &crit), 0);
    FALSY(low);
    FALSY(crit);
}

// =============== USB lost detection tests ===============

static void test_usb_no_drop_no_event() {
    int drops = 0;
    bool lost = false;
    EQ(check_usb_lost(4.0f, 3.99f, &drops, &lost), 0);
    EQ(drops, 0);
}

static void test_usb_one_drop_not_enough() {
    int drops = 0;
    bool lost = false;
    EQ(check_usb_lost(4.0f, 3.98f, &drops, &lost), 0);
    EQ(drops, 1);
    FALSY(lost);
}

static void test_usb_three_drops_triggers_lost() {
    int drops = 0;
    bool lost = false;
    check_usb_lost(4.0f, 3.98f, &drops, &lost);  // drop 1
    check_usb_lost(3.98f, 3.96f, &drops, &lost); // drop 2
    EQ(check_usb_lost(3.96f, 3.94f, &drops, &lost), 1); // drop 3 → LOST
    TRUTHY(lost);
}

static void test_usb_rise_resets_drops() {
    int drops = 2;
    bool lost = false;
    EQ(check_usb_lost(3.9f, 4.0f, &drops, &lost), 0);
    EQ(drops, 0);
}

static void test_usb_restored_after_rise() {
    int drops = 0;
    bool lost = true;
    EQ(check_usb_lost(3.9f, 4.0f, &drops, &lost), 2);
    FALSY(lost);
}

// =============== Main ===============

int main(void) {
    printf("=== Battery Threshold Tests ===\n");
    TEST(normal_voltage_no_alarm);
    TEST(low_voltage_triggers_low);
    TEST(critical_voltage_triggers_critical);
    TEST(low_voltage_not_sent_twice);
    TEST(recovered_after_full_charge);
    TEST(critical_then_recovered);
    TEST(voltage_in_no_mans_land);

    printf("\n=== USB Lost Detection Tests ===\n");
    TEST(usb_no_drop_no_event);
    TEST(usb_one_drop_not_enough);
    TEST(usb_three_drops_triggers_lost);
    TEST(usb_rise_resets_drops);
    TEST(usb_restored_after_rise);

    printf("\n========================================\n");
    printf("  Tests: %d pass / %d fail\n", pass, fail);
    printf("========================================\n");
    return fail > 0 ? 1 : 0;
}
