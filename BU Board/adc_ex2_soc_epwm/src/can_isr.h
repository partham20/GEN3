/*
 * can_isr.h
 *
 * MCAN Interrupt Handler
 * Handles RX FIFO 0 (CAN IDs 5, 6, 7) and Dedicated RX Buffer interrupts
 *
 *  Created on: Dec 7, 2025
 *      Author: Parthasarathy.M
 */

#ifndef SRC_CAN_ISR_H_
#define SRC_CAN_ISR_H_

#include "can_bu.h"

#ifdef CAN_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "driverlib.h"
#include "device.h"
#include "calibration.h"

/* Primary MCAN ISR (registered on INT_MCANA_1 / Line 1) */
__interrupt void MCANIntr1ISR(void);

/* SysConfig-referenced ISR stubs (redirect to MCANIntr1ISR) */
__interrupt void INT_myMCAN0_0_ISR(void);
__interrupt void INT_myMCAN0_1_ISR(void);

/* Debug counters - watch these in debugger */
extern volatile uint32_t g_isrCallCount;
extern volatile uint32_t g_intrStatus;
extern volatile uint32_t g_fifoHitCount;
extern volatile uint32_t g_rxBufHitCount;
extern volatile uint32_t g_newDataLow;
extern volatile uint32_t g_newDataHigh;
extern volatile int32_t g_lastError;

/* Calibration RX data - parsed from S-Board frames, consumed by main loop */
typedef struct {
    uint16_t currentGain[CAL_NUM_CTS];  /* Parsed current gains from frame 0x01 */
    uint16_t kwGain[CAL_NUM_CTS];       /* Parsed kW gains from frame 0x02 */
    volatile bool currentRxd;            /* Frame 0x01 received */
    volatile bool kwRxd;                 /* Frame 0x02 received */
    volatile bool dataReady;             /* Both frames received, ready for flash save */
} CalibRxData;

extern CalibRxData g_calibRx;


/* Live bus status - updated every ISR call */
extern volatile uint32_t g_livePSR;     /* Protocol Status: bits[2:0]=LEC, bit[5]=EP, bit[7]=BO */
extern volatile uint32_t g_liveECR;     /* Error Counters: bits[7:0]=TEC, bits[14:8]=REC */
extern volatile uint32_t g_liveRXF0S;   /* FIFO 0 Status: bits[6:0]=fill level */

#ifdef __cplusplus
}
#endif

#else /* !CAN_ENABLED */

/* Stub ISR declarations when CAN is disabled (needed by SysConfig board.c) */
__interrupt void INT_myMCAN0_0_ISR(void);
__interrupt void INT_myMCAN0_1_ISR(void);

#endif /* CAN_ENABLED */

#endif /* SRC_CAN_ISR_H_ */
