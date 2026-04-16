/*
 * ct_dma.h
 *
 * DMA Configuration for 18 CT (Current Transformer) Channels
 *
 * ADC Configuration:
 *   ADC A: 15 channels (SOC0-SOC14)
 *   ADC E: 3 channels (SOC0-SOC2)
 *
 * Phase Assignment:
 *   R-Phase: CT1-CT6   (ADC A: SOC0-4, ADC E: SOC0)
 *   Y-Phase: CT7-CT12  (ADC E: SOC1-2, ADC A: SOC5-8)
 *   B-Phase: CT13-CT18 (ADC A: SOC9-14)
 *
 * Buffer size configurable via CYCLES_TO_CAPTURE in power_config.h
 */

#ifndef CT_DMA_H_
#define CT_DMA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "driverlib.h"
#include "device.h"
#include "power_config.h"

/* ========================================================================== */
/*                                  Macros                                    */
/* ========================================================================== */

/* Number of CT channels per ADC */
#define CT_ADCA_CHANNELS            (15U)   /* ADC A: SOC0-SOC14 */
#define CT_ADCE_CHANNELS            (3U)    /* ADC E: SOC0-SOC2 */
#define CT_TOTAL_CHANNELS           (18U)   /* Total CT channels */

/* Number of CTs per phase */
#define CT_PER_PHASE                (6U)

/* Phase definitions */
#define CT_PHASE_R                  (0U)
#define CT_PHASE_Y                  (1U)
#define CT_PHASE_B                  (2U)

/* DMA Channel assignments */
#define CT_DMA_CH_ADCA              DMA_CH1_BASE    /* DMA CH1 for ADC A */
#define CT_DMA_CH_ADCE              DMA_CH2_BASE    /* DMA CH2 for ADC E */

/* Buffer size per CT channel (same as voltage buffer) */
#define CT_BUFFER_SIZE              TOTAL_SAMPLE_COUNT

/* Ping-pong buffer identifiers */
#define CT_BUFFER_A                 (0U)
#define CT_BUFFER_B                 (1U)

/* ========================================================================== */
/*                      CT Channel to SOC Mapping                             */
/* ========================================================================== */

/*
 * R-Phase CTs (CT1-CT6):
 *   CT1:  ADC A, SOC0,  Pin A6
 *   CT2:  ADC A, SOC1,  Pin A2
 *   CT3:  ADC A, SOC2,  Pin A11
 *   CT4:  ADC A, SOC3,  Pin A12
 *   CT5:  ADC A, SOC4,  Pin A4
 *   CT6:  ADC E, SOC0,  Pin E1
 *
 * Y-Phase CTs (CT7-CT12):
 *   CT7:  ADC E, SOC1,  Pin E12
 *   CT8:  ADC A, SOC5,  Pin A15
 *   CT9:  ADC A, SOC6,  Pin A5
 *   CT10: ADC A, SOC7,  Pin A7
 *   CT11: ADC A, SOC8,  Pin A9
 *   CT12: ADC E, SOC2,  Pin E2
 *
 * B-Phase CTs (CT13-CT18):
 *   CT13: ADC A, SOC9,  Pin A3
 *   CT14: ADC A, SOC10, Pin A14
 *   CT15: ADC A, SOC11, Pin A1
 *   CT16: ADC A, SOC12, Pin A8
 *   CT17: ADC A, SOC13, Pin A24
 *   CT18: ADC A, SOC14, Pin A25
 */

/* SOC indices for each phase in ADC A (0-14) */
#define CT_ADCA_R_PHASE_SOC_START   (0U)    /* SOC0-SOC4 */
#define CT_ADCA_R_PHASE_SOC_COUNT   (5U)
#define CT_ADCA_Y_PHASE_SOC_START   (5U)    /* SOC5-SOC8 */
#define CT_ADCA_Y_PHASE_SOC_COUNT   (4U)
#define CT_ADCA_B_PHASE_SOC_START   (9U)    /* SOC9-SOC14 */
#define CT_ADCA_B_PHASE_SOC_COUNT   (6U)

/* SOC indices for each phase in ADC E (0-2) */
#define CT_ADCE_R_PHASE_SOC         (0U)    /* SOC0 - CT6 */
#define CT_ADCE_Y_PHASE_SOC_START   (1U)    /* SOC1-SOC2 - CT7, CT12 */
#define CT_ADCE_Y_PHASE_SOC_COUNT   (2U)

/* ========================================================================== */
/*                             Type Definitions                               */
/* ========================================================================== */

/* DMA transfer status */
typedef struct
{
    volatile bool adcA_complete;        /* ADC A DMA transfer complete */
    volatile bool adcE_complete;        /* ADC E DMA transfer complete */
    volatile bool all_complete;         /* All transfers complete */
    volatile uint32_t transferCount;    /* Number of completed transfer cycles */
} CT_DMA_Status;

/* Ping-pong buffer status for 18 CTs */
typedef struct
{
    volatile uint8_t activeBuffer;      /* Currently active buffer (0=A, 1=B) */
    volatile bool bufferReady;          /* Inactive buffer has valid data */
} CT_PingPong_Status;

/* ========================================================================== */
/*                          Global Variables                                  */
/* ========================================================================== */

/*
 * Raw ADC result buffers (DMA destination)
 * Organized as [sample_index][channel_index] for efficient DMA transfer
 */
extern volatile uint16_t g_adcA_rawResults[CT_BUFFER_SIZE][CT_ADCA_CHANNELS];
extern volatile uint16_t g_adcE_rawResults[CT_BUFFER_SIZE][CT_ADCE_CHANNELS];

/*
 * Per-CT buffers organized by phase (after offset removal)
 * These provide convenient access by CT number
 */

/* R-Phase CT buffers (CT1-CT6) */
extern volatile int16_t g_CT1_Buffer[CT_BUFFER_SIZE];   /* ADC A SOC0 */
extern volatile int16_t g_CT2_Buffer[CT_BUFFER_SIZE];   /* ADC A SOC1 */
extern volatile int16_t g_CT3_Buffer[CT_BUFFER_SIZE];   /* ADC A SOC2 */
extern volatile int16_t g_CT4_Buffer[CT_BUFFER_SIZE];   /* ADC A SOC3 */
extern volatile int16_t g_CT5_Buffer[CT_BUFFER_SIZE];   /* ADC A SOC4 */
extern volatile int16_t g_CT6_Buffer[CT_BUFFER_SIZE];   /* ADC E SOC0 */

/* Y-Phase CT buffers (CT7-CT12) */
extern volatile int16_t g_CT7_Buffer[CT_BUFFER_SIZE];   /* ADC E SOC1 */
extern volatile int16_t g_CT8_Buffer[CT_BUFFER_SIZE];   /* ADC A SOC5 */
extern volatile int16_t g_CT9_Buffer[CT_BUFFER_SIZE];   /* ADC A SOC6 */
extern volatile int16_t g_CT10_Buffer[CT_BUFFER_SIZE];  /* ADC A SOC7 */
extern volatile int16_t g_CT11_Buffer[CT_BUFFER_SIZE];  /* ADC A SOC8 */
extern volatile int16_t g_CT12_Buffer[CT_BUFFER_SIZE];  /* ADC E SOC2 */

/* B-Phase CT buffers (CT13-CT18) */
extern volatile int16_t g_CT13_Buffer[CT_BUFFER_SIZE];  /* ADC A SOC9 */
extern volatile int16_t g_CT14_Buffer[CT_BUFFER_SIZE];  /* ADC A SOC10 */
extern volatile int16_t g_CT15_Buffer[CT_BUFFER_SIZE];  /* ADC A SOC11 */
extern volatile int16_t g_CT16_Buffer[CT_BUFFER_SIZE];  /* ADC A SOC12 */
extern volatile int16_t g_CT17_Buffer[CT_BUFFER_SIZE];  /* ADC A SOC13 */
extern volatile int16_t g_CT18_Buffer[CT_BUFFER_SIZE];  /* ADC A SOC14 */

/*
 * Ping-Pong Buffers for all 18 CTs
 * Buffer A and Buffer B for double-buffering during continuous acquisition
 * Organized as 2D arrays: [CT_TOTAL_CHANNELS][CT_BUFFER_SIZE]
 */
extern volatile int16_t g_ctPingPongA[CT_TOTAL_CHANNELS][CT_BUFFER_SIZE];
extern volatile int16_t g_ctPingPongB[CT_TOTAL_CHANNELS][CT_BUFFER_SIZE];

/* DMA status */
extern volatile CT_DMA_Status g_ctDmaStatus;

/* Ping-pong buffer status */
extern volatile CT_PingPong_Status g_ctPingPongStatus;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/**
 * @brief   Initialize DMA for 18 CT channels
 *          Configures DMA CH1 for ADC A (15 channels)
 *          Configures DMA CH2 for ADC E (3 channels)
 */
void CT_DMA_init(void);

/**
 * @brief   Start DMA transfers for CT acquisition
 *          Call this after sync pulse received
 */
void CT_DMA_start(void);

/**
 * @brief   Stop DMA transfers
 */
void CT_DMA_stop(void);

/**
 * @brief   Reset DMA for next acquisition cycle
 *          Resets addresses and clears flags
 */
void CT_DMA_reset(void);

/**
 * @brief   Check if all DMA transfers are complete
 * @return  true if both ADC A and ADC E transfers are complete
 */
bool CT_DMA_isComplete(void);

/**
 * @brief   Copy raw ADC results to per-CT buffers with offset removal
 *          Call this after DMA transfer is complete
 *          Subtracts 2048 offset from 12-bit ADC values
 */
void CT_DMA_copyToBuffers(void);

/**
 * @brief   Get pointer to specific CT buffer
 * @param   ctNumber - CT number (1-18)
 * @return  Pointer to CT buffer, or NULL if invalid CT number
 */
volatile int16_t* CT_DMA_getBuffer(uint8_t ctNumber);

/**
 * @brief   Get pointer to phase CT buffers array
 * @param   phase - CT_PHASE_R, CT_PHASE_Y, or CT_PHASE_B
 * @param   buffers - Output array of 6 buffer pointers
 */
void CT_DMA_getPhaseBuffers(uint8_t phase, volatile int16_t* buffers[6]);

/**
 * @brief   DMA CH1 ISR handler (ADC A complete)
 *          Call this from the DMA CH1 interrupt
 */
void CT_DMA_ch1ISR(void);

/**
 * @brief   DMA CH2 ISR handler (ADC E complete)
 *          Call this from the DMA CH2 interrupt
 */
void CT_DMA_ch2ISR(void);

/* ========================================================================== */
/*                    Ping-Pong Buffer Functions                              */
/* ========================================================================== */

/**
 * @brief   Copy raw ADC results to active ping-pong buffer with DC offset removal
 *          Uses CT_DC_OFFSET from power_config.h (default: 2010)
 */
void CT_DMA_copyToPingPong(void);

/**
 * @brief   Swap ping-pong buffers (makes active buffer inactive and vice versa)
 *          Call after copying data to active buffer
 */
void CT_DMA_swapPingPong(void);

/**
 * @brief   Get pointer to active ping-pong buffer for a specific CT
 * @param   ctNumber - CT number (1-18)
 * @return  Pointer to active buffer for the specified CT
 */
volatile int16_t* CT_DMA_getActiveCTBuffer(uint8_t ctNumber);

/**
 * @brief   Get pointer to inactive (ready for processing) ping-pong buffer for a specific CT
 * @param   ctNumber - CT number (1-18)
 * @return  Pointer to inactive buffer for the specified CT
 */
volatile int16_t* CT_DMA_getInactiveCTBuffer(uint8_t ctNumber);

/**
 * @brief   Get pointer to all active buffers for a phase (6 CTs)
 * @param   phase - CT_PHASE_R, CT_PHASE_Y, or CT_PHASE_B
 * @param   buffers - Output array of 6 buffer pointers
 */
void CT_DMA_getActivePhaseBuffers(uint8_t phase, volatile int16_t* buffers[6]);

/**
 * @brief   Get pointer to all inactive buffers for a phase (6 CTs)
 * @param   phase - CT_PHASE_R, CT_PHASE_Y, or CT_PHASE_B
 * @param   buffers - Output array of 6 buffer pointers
 */
void CT_DMA_getInactivePhaseBuffers(uint8_t phase, volatile int16_t* buffers[6]);

/**
 * @brief   Check if inactive buffer has valid data ready for processing
 * @return  true if data is ready
 */
bool CT_DMA_isBufferReady(void);

/**
 * @brief   Clear buffer ready flag after processing inactive buffer
 */
void CT_DMA_clearBufferReady(void);

/**
 * @brief   Get current active buffer identifier (CT_BUFFER_A or CT_BUFFER_B)
 * @return  Active buffer ID
 */
uint8_t CT_DMA_getActiveBufferId(void);

#ifdef __cplusplus
}
#endif

#endif /* CT_DMA_H_ */
