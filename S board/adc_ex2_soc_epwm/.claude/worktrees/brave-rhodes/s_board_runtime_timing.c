/**
 * @file s_board_runtime_timing.c
 * @brief S Board runtime sync timing system
 * @details GPIO28 sync signal and Timer1 management.
 *          NOTE: CPU Timer 2 is now managed by time_slot.c for time-slicing.
 */

#include "s_board_runtime_timing.h"
#include "runtime.h"
#include "bu_board.h"
#include "metro.h"
#include "runtimedata.h"

// Timing control variables
volatile bool voltageTransmissionTrigger = false;
volatile bool syncToggleTrigger = false;
volatile uint32_t syncToggleState = 0;
int c1,sToBuBoardSpeedIn20ms;  /* c2,c3 removed - old Timer2 counters no longer needed */

bool Ms200Passed  = false , Ms500Passed = false;

/**
 * @brief Initialize runtime timing system
 * @details Sets up Timer1 (200ms) and Timer2 (20ms) with 150MHz clock
 */
void initRuntimeTiming(void)
{

    // Configure GPIO28 for sync signal output
    GPIO_setPadConfig(28, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(28, GPIO_DIR_MODE_OUT);
    GPIO_setQualificationMode(28, GPIO_QUAL_SYNC);
    GPIO_writePin(28, 0);  // Initialize sync signal low

    // Initialize sync state
    syncToggleState = 0;
}

/**
 * @brief Start runtime timing system (Timer 1 only)
 * @note Timer 2 is now managed by time_slot.c — do NOT start it here
 */
void startRuntimeTiming(void)
{
    CPUTimer_startTimer(CPUTIMER1_BASE);
}

/**
 * @brief Stop runtime timing system
 */
void stopRuntimeTiming(void)
{
    CPUTimer_stopTimer(CPUTIMER1_BASE);
    CPUTimer_stopTimer(CPUTIMER2_BASE);
}

/**
 * @brief Timer1 ISR - 200ms voltage buffer transmission
 */
// __interrupt void cpuTimer1ISR(void)

// {
//     c1++;
//     voltageTransmissionTrigger = true;

//    sendVoltageBuffersToAllBUBoards();
//     Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
// }

/*
 * NOTE: cpuTimer2ISR() has been moved to time_slot.c
 * Timer 2 now fires every 100ms and drives the time-slicing state machine.
 * See time_slot.h for the timing diagram.
 */


