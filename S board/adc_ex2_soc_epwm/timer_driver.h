/**
 * @file timer_driver.h
 * @brief Simple CPU Timer0-based software timer system
 */

#ifndef TIMER_DRIVER_H
#define TIMER_DRIVER_H

#include "common.h"

// Software timer IDs
typedef enum {
    TIMER_FRAME_TRACKING = 0,
    TIMER_10_SEC,
    TIMER_BU_DATA_COLLECTION,
    TIMER_RETRY,
    MAX_SOFTWARE_TIMERS
} software_timer_id_t;

// Software timer structure
typedef struct {
    uint32_t reload_value;
    uint32_t current_value;
    bool active;
    bool expired;
    bool auto_reload;
} software_timer_t;

// Main timer system functions
void initTimerSystem(void);
void timerSystemTick(void);  // Call this from CPU Timer0 ISR

// Software timer functions
void startSoftwareTimer(software_timer_id_t timer_id, uint32_t timeout_ms, bool auto_reload);
void stopSoftwareTimer(software_timer_id_t timer_id);
bool isSoftwareTimerExpired(software_timer_id_t timer_id);

// System initialization
void timer_system_init(void);

// Legacy API compatibility functions
void initFrameTracking(void);
void startFrameSequenceTimer(void);
void resetFrameTracking(void);

void restart10SecTimer(void);
void stop10SecTimer(void);

void startBUDataCollection(void);
void stopBUDataCollection(void);

void startRetryTimer(void);
void stopRetryTimer(void);

// Global variables
extern bool timer10SecExpired;
extern bool timerExpired;
extern bool retryTimerExpired;
extern bool buDataTimeoutOccurred;
extern int TIMER_TICK_TIME_MS;

// ── Millisecond tick (1 kHz) ───────────────────────────────────
// Incremented by msTickISR() (CPU Timer 1, one tick per millisecond).
// fw_bu_master.c uses this for phase timeouts. Also usable by any
// other code that needs a coarse monotonic millisecond clock.
extern volatile uint32_t g_systemTickMs;

// Call once during startup to configure CPU Timer 1 @ 1 kHz and
// register msTickISR. Safe to call from the simplified startup()
// path; does not touch CPU Timer 0 or CPU Timer 2.
void initMsTick(void);

#endif // TIMER_DRIVER_H
