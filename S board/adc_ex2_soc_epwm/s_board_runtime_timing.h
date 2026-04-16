/**
 * @file s_board_runtime_timing.h
 * @brief S Board runtime voltage transmission and sync timing system header
 */

#ifndef S_BOARD_RUNTIME_TIMING_H
#define S_BOARD_RUNTIME_TIMING_H

#include "common.h"
extern bool Ms200Passed  , Ms500Passed ;

// Function prototypes
void initRuntimeTiming(void);
void startRuntimeTiming(void);
void stopRuntimeTiming(void);
void processRuntimeTimingEvents(void);

// Interrupt service routines
__interrupt void cpuTimer1ISR(void);
/* cpuTimer2ISR moved to time_slot.h — Timer 2 now drives time-slicing */

// External variable declarations
extern volatile bool voltageTransmissionTrigger;
extern volatile bool syncToggleTrigger;
extern int c1,sToBuBoardSpeedIn20ms;

#endif // S_BOARD_RUNTIME_TIMING_H
