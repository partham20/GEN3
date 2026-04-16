/**
 * @file timer_driver.c
 * @brief Simple CPU Timer0-based software timer system
 */

#include "timer_driver.h"
#include "can_driver.h"

// Timer tick configuration - change this to match your timer frequency
#define TIMER_TICK_TIME_MS    1000    // 1000ms = 1 second per tick

// External variables from can_driver.h
extern uint16_t receivedFramesMask;
extern bool allFramesReceived;
extern bool sequenceStarted;
extern uint8_t frameCounter;
extern uint8_t retryCounter;
extern bool bufferFull;
extern bool requestRetransmission;
extern void ClearRxFIFOBuffer(void);

// Global variables
bool timer10SecExpired = false;
bool timerExpired = false;
bool retryTimerExpired = false;
bool buDataTimeoutOccurred = false;

// Monotonic millisecond tick used by fw_bu_master and anyone else
// needing coarse elapsed-time measurements. Incremented in msTickISR()
// below (CPU Timer 1 firing at 1 kHz).
volatile uint32_t g_systemTickMs = 0;

// Software timer array
software_timer_t softwareTimers[MAX_SOFTWARE_TIMERS];

/**
 * @brief Initialize the timer system
 */
void initTimerSystem(void)
{
    int i;
    for (i = 0; i < MAX_SOFTWARE_TIMERS; i++) {
        softwareTimers[i].reload_value = 0;
        softwareTimers[i].current_value = 0;
        softwareTimers[i].active = false;
        softwareTimers[i].expired = false;
        softwareTimers[i].auto_reload = false;
    }

    timer10SecExpired = false;
    timerExpired = false;
    retryTimerExpired = false;
    buDataTimeoutOccurred = false;
}

/**
 * @brief Timer system tick - call this from CPU Timer0 ISR
 */
void timerSystemTick(void)
{
    int i;

    // Update all active software timers
    for (i = 0; i < MAX_SOFTWARE_TIMERS; i++) {
        if (softwareTimers[i].active) {
            if (softwareTimers[i].current_value > 0) {
                softwareTimers[i].current_value--;
            }

            if (softwareTimers[i].current_value == 0) {
                softwareTimers[i].expired = true;

                if (softwareTimers[i].auto_reload) {
                    softwareTimers[i].current_value = softwareTimers[i].reload_value;
                } else {
                    softwareTimers[i].active = false;
                }
            }
        }
    }

    // Update legacy flags
    timer10SecExpired = softwareTimers[TIMER_10_SEC].expired;
    timerExpired = softwareTimers[TIMER_FRAME_TRACKING].expired;
    requestRetransmission = softwareTimers[TIMER_FRAME_TRACKING].expired;
    retryTimerExpired = softwareTimers[TIMER_RETRY].expired;

    buDataTimeoutOccurred = softwareTimers[TIMER_BU_DATA_COLLECTION].expired;
}

/**
 * @brief Start a software timer
 */
void startSoftwareTimer(software_timer_id_t timer_id, uint32_t timeout_ms, bool auto_reload)
{
    if (timer_id < MAX_SOFTWARE_TIMERS) {
        // Convert ms to timer ticks using the macro
        uint32_t ticks = timeout_ms / TIMER_TICK_TIME_MS;
        if (ticks == 0) ticks = 1; // Minimum 1 tick

        softwareTimers[timer_id].reload_value = ticks;
        softwareTimers[timer_id].current_value = ticks;
        softwareTimers[timer_id].active = true;
        softwareTimers[timer_id].expired = false;
        softwareTimers[timer_id].auto_reload = auto_reload;
    }
}

/**
 * @brief Stop a software timer
 */
void stopSoftwareTimer(software_timer_id_t timer_id)
{
    if (timer_id < MAX_SOFTWARE_TIMERS) {
        softwareTimers[timer_id].active = false;
        softwareTimers[timer_id].expired = false;
    }
}

/**
 * @brief Check if a software timer has expired
 */
bool isSoftwareTimerExpired(software_timer_id_t timer_id)
{
    if (timer_id < MAX_SOFTWARE_TIMERS) {
        if (softwareTimers[timer_id].expired) {
            softwareTimers[timer_id].expired = false; // Clear flag
            return true;
        }
    }
    return false;
}

/**
 * @brief Initialize the timer system
 */
void timer_system_init(void)
{
    initTimerSystem();
}

// ==============================================================================
// Legacy API compatibility functions
// ==============================================================================

void initFrameTracking(void)
{
    startSoftwareTimer(TIMER_FRAME_TRACKING, 2000, false);
}

void startFrameSequenceTimer(void)
{
    timerExpired = false;
    startSoftwareTimer(TIMER_FRAME_TRACKING, 2000, false);
}


/**
 * @brief Reset frame tracking with complete cleanup
 * This function ensures no spurious retransmission requests are sent
 */
void resetFrameTracking(void)
{
    // Disable interrupts during critical section
    DINT;

    // Clear ALL frame tracking variables
    receivedFramesMask = 0;
    allFramesReceived = false;
    sequenceStarted = false;
    frameCounter = 0;
    timerExpired = false;
    bufferFull = false;
    requestRetransmission = false;
    retryCounter = 0;  // Critical: Reset retry counter to prevent unwanted messages

    // Stop all frame tracking related timers
    stopSoftwareTimer(TIMER_FRAME_TRACKING);
    stopSoftwareTimer(TIMER_RETRY);

    // Clear any pending software timer flags that could trigger retransmission
    softwareTimers[TIMER_FRAME_TRACKING].expired = false;
    softwareTimers[TIMER_RETRY].expired = false;

    // Clear the global timer flags
    timerExpired = false;
    retryTimerExpired = false;

    // ADDITIONAL FIX: Reset transmission flags to prevent continuous retry loops
    extern bool sendToBUBoard;
    extern bool sendToMBoard;
    sendToBUBoard = false;
    sendToMBoard = false;

    // Re-enable interrupts
    EINT;

    // Do NOT automatically restart the timer here - let the calling function decide
}
void restart10SecTimer(void)
{
    timer10SecExpired = false;
    startSoftwareTimer(TIMER_10_SEC, 10000, false);
}

void stop10SecTimer(void)
{
    stopSoftwareTimer(TIMER_10_SEC);
    timer10SecExpired = false;
}

void startBUDataCollection(void)
{
    buDataTimeoutOccurred = false;
    startSoftwareTimer(TIMER_BU_DATA_COLLECTION, 10000, false);
}

void stopBUDataCollection(void)
{
    stopSoftwareTimer(TIMER_BU_DATA_COLLECTION);
    buDataTimeoutOccurred = false;
}

void startRetryTimer(void)
{
    retryTimerExpired = false;
    startSoftwareTimer(TIMER_RETRY, 2000, false);
}

void stopRetryTimer(void)
{
    stopSoftwareTimer(TIMER_RETRY);
    retryTimerExpired = false;
}

// ═══════════════════════════════════════════════════════════════
//  1 kHz MILLISECOND TICK (CPU Timer 1)
//
//  Used by fw_bu_master.c for phase timeouts (and anyone else who
//  needs a monotonic millisecond clock). Completely independent of
//  CPU Timer 0 (existing 1 Hz timerSystemTick) and CPU Timer 2
//  (existing CAN TX watchdog in can_driver.c) — so it does not
//  interfere with either of them.
// ═══════════════════════════════════════════════════════════════

/**
 * @brief  CPU Timer 1 ISR — fires once per millisecond and
 *         increments g_systemTickMs.
 *
 *  CPU Timer 1 uses the direct INT13 interrupt line (not routed
 *  through PIE), so there is no PIE group to acknowledge here.
 */
__interrupt void msTickISR(void)
{
    g_systemTickMs++;
}

/**
 * @brief  Configure CPU Timer 1 to fire at 1 kHz and install the
 *         ms-tick ISR. Call once from startup() before EINT/ERTM.
 */
void initMsTick(void)
{
    // Period = SYSCLK / 1000  → one interrupt per millisecond.
    // CPU Timer register is loaded with (period - 1).
    CPUTimer_stopTimer(CPUTIMER1_BASE);
    CPUTimer_setPeriod(CPUTIMER1_BASE, (DEVICE_SYSCLK_FREQ / 1000UL) - 1UL);
    CPUTimer_setPreScaler(CPUTIMER1_BASE, 0U);
    CPUTimer_reloadTimerCounter(CPUTIMER1_BASE);
    CPUTimer_setEmulationMode(CPUTIMER1_BASE,
                              CPUTIMER_EMULATIONMODE_STOPAFTERNEXTDECREMENT);
    CPUTimer_enableInterrupt(CPUTIMER1_BASE);

    Interrupt_register(INT_TIMER1, &msTickISR);
    Interrupt_enable(INT_TIMER1);

    CPUTimer_startTimer(CPUTIMER1_BASE);

    g_systemTickMs = 0U;
}
