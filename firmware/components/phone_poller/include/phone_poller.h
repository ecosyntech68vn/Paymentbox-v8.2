#pragma once

/**
 * Start phone polling task.
 * Polls phone /api/v1/health every PHONE_POLL_INTERVAL_MS.
 * Publishes events to event_bus.
 */
void phone_poller_start(void);

/**
 * Update phone IP at runtime (e.g., after mDNS discovery or NVS load).
 */
void phone_poller_set_ip(const char *ip);
