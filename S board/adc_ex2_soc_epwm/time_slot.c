/**
 * @file time_slot.c
 * @brief Time-Slicing Implementation using CPU Timer 2
 *
 * This file configures CPU Timer 2 to fire every 10ms and drives
 * a slot state machine for S Board communication scheduling.
 *
 * ==========================================================================
 * TICK-TO-SLOT MAPPING (configurable via SLOT_x_TICKS in time_slot.h):
 * ==========================================================================
 *   tick 0..9   -> SLOT_1_VOLTAGE_TX  (100ms voltage TX window)
 *   tick 10..19 -> SLOT_2_BU_RX       (100ms BU receive window)
 *   tick 20..29 -> SLOT_3_MBOARD_TX   (100ms M Board TX window)
 *   tick 30     -> wraps to 0, back to SLOT_1 (new cycle begins)
 * ==========================================================================
 */

#include "time_slot.h"

/* =========================================================================
 * GLOBAL VARIABLES (declared extern in time_slot.h)
 * ========================================================================= */

/* Current slot - starts at Slot 1 */
volatile TimeSlot_e g_currentSlot = SLOT_1_VOLTAGE_TX;

/* Set TRUE only when slot changes, cleared by main loop after handling */
volatile bool g_slotChanged = false;

/* Tick counter: 0 to TICKS_PER_CYCLE-1 then wraps to 0 */
volatile uint16_t g_tickCount = 0;

/* Number of complete cycles since startup */
volatile uint32_t g_cycleCount = 0;

/* =========================================================================
 * TimeSlot_init() - Configure CPU Timer 2 for 10ms period
 * ========================================================================= */
void TimeSlot_init(void)
{
    /* Stop timer before configuring */
    CPUTimer_stopTimer(CPUTIMER2_BASE);

    /*
     * Set Timer 2 clock source to SYSCLK (150MHz)
     *
     * CRITICAL: Unlike Timer 0 and Timer 1 (which always use SYSCLK),
     * Timer 2 has a configurable clock source. If not set explicitly,
     * it may default to INTOSC1 (10MHz) and the ISR will never fire
     * at the expected rate (or at all).
     */
    CPUTimer_selectClockSource(CPUTIMER2_BASE,
                               CPUTIMER_CLOCK_SOURCE_SYS,
                               CPUTIMER_CLOCK_PRESCALER_1);

    /* Set emulation mode to match Timer 0's SysConfig setting */
    CPUTimer_setEmulationMode(CPUTIMER2_BASE,
                              CPUTIMER_EMULATIONMODE_STOPAFTERNEXTDECREMENT);

    /*
     * Set timer period
     * At 150MHz SYSCLK: 1,500,000 clocks = 10ms
     */
    CPUTimer_setPeriod(CPUTIMER2_BASE, TIMESLOT_TIMER_PERIOD_CLOCKS);

    /* Set prescaler to 0 (no additional division) */
    CPUTimer_setPreScaler(CPUTIMER2_BASE, 0);

    /* Clear any pending overflow flag, then reload counter */
    CPUTimer_clearOverflowFlag(CPUTIMER2_BASE);
    CPUTimer_reloadTimerCounter(CPUTIMER2_BASE);

    /* Enable Timer 2 interrupt */
    CPUTimer_enableInterrupt(CPUTIMER2_BASE);

    /* Register ISR and enable CPU INT14 */
    Interrupt_register(INT_TIMER2, &cpuTimer2ISR);
    Interrupt_enable(INT_TIMER2);

    /* Initialize slot state */
    g_currentSlot = SLOT_1_VOLTAGE_TX;
    g_slotChanged = true;   /* Trigger initial Slot 1 work on first loop */
    g_tickCount = 0;
    g_cycleCount = 0;
}

/* =========================================================================
 * TimeSlot_start() - Start the timer
 * ========================================================================= */
void TimeSlot_start(void)
{
    CPUTimer_startTimer(CPUTIMER2_BASE);
}

/* =========================================================================
 * TimeSlot_stop() - Stop the timer
 * ========================================================================= */
void TimeSlot_stop(void)
{
    CPUTimer_stopTimer(CPUTIMER2_BASE);
}

/* =========================================================================
 * cpuTimer2ISR() - Timer 2 Interrupt Service Routine
 * ========================================================================= */
/**
 * Fires every 10ms. Maps tick ranges to slots using the configurable
 * SLOT_x_TICKS defines from time_slot.h.
 *
 * Slot boundaries (auto-calculated from SLOT_x_TICKS):
 *   tick 0             .. SLOT2_START_TICK-1 -> SLOT_1 (Voltage TX)
 *   tick SLOT2_START_TICK .. SLOT3_START_TICK-1 -> SLOT_2 (BU RX)
 *   tick SLOT3_START_TICK .. TICKS_PER_CYCLE-1  -> SLOT_3 (M Board TX)
 *
 * g_slotChanged is set TRUE only on slot transitions.
 */
#pragma CODE_SECTION(cpuTimer2ISR, ".TI.ramfunc");
__interrupt void cpuTimer2ISR(void)
{
    TimeSlot_e previousSlot = g_currentSlot;

    /* Increment tick counter and wrap at TICKS_PER_CYCLE */
    g_tickCount++;
    if (g_tickCount >= TIMESLOT_TICKS_PER_CYCLE)
    {
        g_tickCount = 0;
        g_cycleCount++;
    }

    /* Map tick to slot using configurable boundaries */
    if (g_tickCount < SLOT2_START_TICK)
    {
        g_currentSlot = SLOT_1_VOLTAGE_TX;
    }
    else if (g_tickCount < SLOT3_START_TICK)
    {
        g_currentSlot = SLOT_2_BU_RX;
    }
    else
    {
        g_currentSlot = SLOT_3_MBOARD_TX;
    }

    /* Set g_slotChanged only on actual slot transitions */
    if (g_currentSlot != previousSlot)
    {
        g_slotChanged = true;
    }
}
