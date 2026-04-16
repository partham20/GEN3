/**
 * @file time_slot.h
 * @brief Time-Slicing Architecture for S Board Communication
 *
 * Implements a CPU Timer 2 based time-division system for non-blocking
 * communication between S Board, BU Boards, and M Board.
 *
 * ==========================================================================
 * TIMING DIAGRAM (300ms cycle, repeats ~3.3 times per second):
 * ==========================================================================
 *
 *  |--- Slot 1 ---|--- Slot 2 ---|--- Slot 3 ---|
 *  |    100ms     |    100ms     |    100ms     |
 *  |  (10 ticks)  |  (10 ticks)  |  (10 ticks)  |
 *  |              |              |              |
 *  | VOLTAGE TX   | BU BOARD RX  | M BOARD TX   |
 *  | Send 27      | Receive 216  | Send 220     |
 *  | frames to    | branch frames| parameter    |
 *  | 12 BUs       | from 12 BUs  | frames to MB |
 *  |              |              |              |
 *  tick=0        tick=10       tick=20        tick=30 (wrap)
 *
 * ==========================================================================
 * HOW IT WORKS:
 * ==========================================================================
 *  1. CPU Timer 2 fires every 10ms (1,500,000 clocks at 150MHz)
 *  2. ISR increments a tick counter, wraps at TICKS_PER_CYCLE
 *  3. Tick ranges map to slots (configurable via SLOT_x_TICKS defines):
 *       tick 0..9   = Slot 1 (Voltage TX)    - 10 ticks = 100ms
 *       tick 10..19 = Slot 2 (BU RX)         - 10 ticks = 100ms
 *       tick 20..29 = Slot 3 (M Board TX)    - 10 ticks = 100ms
 *  4. Main loop checks g_currentSlot and g_slotChanged to decide what to do
 *  5. g_slotChanged is set TRUE only on slot transitions (not every tick)
 *
 * ==========================================================================
 * TUNING: Change SLOT_x_TICKS to adjust each slot's duration independently.
 *         Each tick = 10ms. Total cycle = sum of all three slot ticks.
 * ==========================================================================
 */

#ifndef TIME_SLOT_H
#define TIME_SLOT_H

#include "common.h"

/* =========================================================================
 * SLOT DEFINITIONS
 * ========================================================================= */

typedef enum {
    SLOT_1_VOLTAGE_TX  = 0,   /* Voltage TX to BU boards  */
    SLOT_2_BU_RX       = 1,   /* Receive branch data from BUs */
    SLOT_3_MBOARD_TX   = 2    /* Send parameters to M Board   */
} TimeSlot_e;

/* =========================================================================
 * TIMER CONFIGURATION
 * ========================================================================= */

/*
 * CPU Timer 2 period for 10ms tick at 150MHz SYSCLK
 * Calculation: 150,000,000 Hz * 0.01 sec = 1,500,000 clocks
 */
#define TIMESLOT_TIMER_PERIOD_CLOCKS    1500000UL

/* Tick duration in ms (for reference / debug) */
#define TIMESLOT_TICK_MS                10

/* =========================================================================
 * SLOT DURATION CONFIGURATION (in ticks, each tick = 10ms)
 *
 * Tune these to adjust each slot's duration independently.
 * Total cycle time = (SLOT1 + SLOT2 + SLOT3) * 10ms
 * ========================================================================= */

#define SLOT1_VOLTAGE_TX_TICKS    10   /* 100ms - DMA capture + DAC + CAN TX */
#define SLOT2_BU_RX_TICKS         10   /* 100ms - Receive from BU boards     */
#define SLOT3_MBOARD_TX_TICKS     10   /* 100ms - Send params to M Board     */

/* Total ticks per cycle (auto-calculated) */
#define TIMESLOT_TICKS_PER_CYCLE  (SLOT1_VOLTAGE_TX_TICKS + \
                                   SLOT2_BU_RX_TICKS + \
                                   SLOT3_MBOARD_TX_TICKS)

/* Slot boundary tick values (auto-calculated, used by ISR) */
#define SLOT2_START_TICK          (SLOT1_VOLTAGE_TX_TICKS)
#define SLOT3_START_TICK          (SLOT1_VOLTAGE_TX_TICKS + SLOT2_BU_RX_TICKS)

/* =========================================================================
 * GLOBAL STATE VARIABLES (set by ISR, read by main loop)
 * ========================================================================= */

/* Current active time slot - read this in main loop to decide what to do */
extern volatile TimeSlot_e g_currentSlot;

/*
 * Set TRUE by ISR when the slot changes (not on every tick, only on transitions).
 * Main loop checks this, does the slot's work, then clears it to FALSE.
 * This ensures each slot's work runs exactly once per transition.
 */
extern volatile bool g_slotChanged;

/* Tick counter (0 to TICKS_PER_CYCLE-1), incremented by ISR every 10ms */
extern volatile uint16_t g_tickCount;

/* Counts complete cycles since startup (for debug/monitoring) */
extern volatile uint32_t g_cycleCount;

/* =========================================================================
 * FUNCTION PROTOTYPES
 * ========================================================================= */

/*
 * Initialize the time-slicing system.
 * Configures CPU Timer 2 for 10ms period and registers the ISR.
 * Call during startup, BEFORE enabling global interrupts (EINT).
 */
void TimeSlot_init(void);

/*
 * Start the time-slicing timer.
 * Starts CPU Timer 2. Call AFTER enabling global interrupts (EINT).
 * From this point, the ISR fires every 10ms and drives the slot machine.
 */
void TimeSlot_start(void);

/*
 * Stop the time-slicing timer.
 * Stops CPU Timer 2. Useful for debug or error recovery.
 */
void TimeSlot_stop(void);

/* =========================================================================
 * ISR PROTOTYPE
 * ========================================================================= */

/* CPU Timer 2 ISR - fires every 10ms, manages slot transitions */
__interrupt void cpuTimer2ISR(void);

#endif /* TIME_SLOT_H */
