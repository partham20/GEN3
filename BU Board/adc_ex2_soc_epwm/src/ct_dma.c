/*
 * ct_dma.c
 *
 * DMA Implementation for 18 CT (Current Transformer) Channels
 *
 * DMA Configuration:
 *   DMA CH1: ADC A results (15 channels, SOC0-SOC14)
 *            Trigger: ADC A INT2 (EOC14 - last SOC)
 *            Burst: 15 words per trigger
 *            Transfer: TOTAL_SAMPLE_COUNT bursts
 *
 *   DMA CH2: ADC E results (3 channels, SOC0-SOC2)
 *            Trigger: ADC E INT2 (EOC2 - last SOC)
 *            Burst: 3 words per trigger
 *            Transfer: TOTAL_SAMPLE_COUNT bursts
 *
 * Memory Layout:
 *   g_adcA_rawResults[sample][channel] - 2D array for efficient DMA
 *   g_adcE_rawResults[sample][channel] - 2D array for efficient DMA
 *
 * After DMA complete, CT_DMA_copyToBuffers() extracts to individual CT buffers
 */

#include "ct_dma.h"
#include "board.h"
#include <string.h>

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/*
 * Raw ADC result buffers (DMA destination)
 * Place in dedicated RAM section for DMA access
 */
#pragma DATA_SECTION(g_adcA_rawResults, "ramgs1");
volatile uint16_t g_adcA_rawResults[CT_BUFFER_SIZE][CT_ADCA_CHANNELS];

#pragma DATA_SECTION(g_adcE_rawResults, "ramgs2");
volatile uint16_t g_adcE_rawResults[CT_BUFFER_SIZE][CT_ADCE_CHANNELS];

/*
 * Per-CT buffers organized by phase (after offset removal)
 * R-Phase CT buffers (CT1-CT6)
 */
volatile int16_t g_CT1_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT2_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT3_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT4_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT5_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT6_Buffer[CT_BUFFER_SIZE];

/* Y-Phase CT buffers (CT7-CT12) */
volatile int16_t g_CT7_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT8_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT9_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT10_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT11_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT12_Buffer[CT_BUFFER_SIZE];

/* B-Phase CT buffers (CT13-CT18) */
volatile int16_t g_CT13_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT14_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT15_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT16_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT17_Buffer[CT_BUFFER_SIZE];
volatile int16_t g_CT18_Buffer[CT_BUFFER_SIZE];

/* Pointer array for easy access by CT number (index 0 = CT1, etc.) */
static volatile int16_t* const g_ctBufferPtrs[CT_TOTAL_CHANNELS] = {
    g_CT1_Buffer,  g_CT2_Buffer,  g_CT3_Buffer,  g_CT4_Buffer,
    g_CT5_Buffer,  g_CT6_Buffer,  g_CT7_Buffer,  g_CT8_Buffer,
    g_CT9_Buffer,  g_CT10_Buffer, g_CT11_Buffer, g_CT12_Buffer,
    g_CT13_Buffer, g_CT14_Buffer, g_CT15_Buffer, g_CT16_Buffer,
    g_CT17_Buffer, g_CT18_Buffer
};

/* DMA status */
volatile CT_DMA_Status g_ctDmaStatus = {
    .adcA_complete = false,
    .adcE_complete = false,
    .all_complete = false,
    .transferCount = 0U
};

/*
 * Ping-Pong Buffers for all 18 CTs
 * Each buffer: [CT_TOTAL_CHANNELS][CT_BUFFER_SIZE] = [18][256] for 2 cycles
 * Total: 2 buffers × 18 CTs × 256 samples × 2 bytes = 18,432 bytes
 */
#pragma DATA_SECTION(g_ctPingPongA, "ramgs3");
volatile int16_t g_ctPingPongA[CT_TOTAL_CHANNELS][CT_BUFFER_SIZE];

#pragma DATA_SECTION(g_ctPingPongB, "ramgs4");
volatile int16_t g_ctPingPongB[CT_TOTAL_CHANNELS][CT_BUFFER_SIZE];

/* Ping-pong buffer status */
volatile CT_PingPong_Status g_ctPingPongStatus = {
    .activeBuffer = CT_BUFFER_A,
    .bufferReady = false
};

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

/**
 * @brief   Initialize DMA for 18 CT channels
 */
void CT_DMA_init(void)
{
    /*
     * ==================== ADC INT2 Configuration for DMA ====================
     *
     * ADC A INT2: Triggers on EOC14 (last SOC) for DMA CH1
     * ADC E INT2: Triggers on EOC2 (last SOC) for DMA CH2
     *
     * Continuous mode enabled for auto-clearing interrupt flag
     */

    /* Configure ADC A INT2 for DMA triggering on EOC14 (last SOC) */
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER2, ADC_INT_TRIGGER_EOC14);
    ADC_enableContinuousMode(ADCA_BASE, ADC_INT_NUMBER2);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER2);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER2);

    /* Configure ADC E INT2 for DMA triggering on EOC2 (last SOC) */
    ADC_setInterruptSource(ADCE_BASE, ADC_INT_NUMBER2, ADC_INT_TRIGGER_EOC2);
    ADC_enableContinuousMode(ADCE_BASE, ADC_INT_NUMBER2);
    ADC_enableInterrupt(ADCE_BASE, ADC_INT_NUMBER2);
    ADC_clearInterruptStatus(ADCE_BASE, ADC_INT_NUMBER2);

    /*
     * ==================== DMA CH1: ADC A (15 channels) ====================
     *
     * Source: ADCA Result registers (RESULT0-RESULT14)
     * Destination: g_adcA_rawResults[sample][channel]
     *
     * Burst: 15 words (all 15 SOC results)
     *   - srcBurstStep = 1 (consecutive result registers)
     *   - destBurstStep = 1 (consecutive in destination row)
     *
     * Transfer: CT_BUFFER_SIZE bursts (one per sample)
     *   - srcTransferStep = -(15-1) = -14 (back to RESULT0)
     *   - destTransferStep = 1 (continue to next row)
     *
     * Wrap: Not used (continuous mode disabled)
     */

    /* Configure source and destination addresses */
    DMA_configAddresses(CT_DMA_CH_ADCA,
                        (uint16_t *)&g_adcA_rawResults[0][0],           /* Destination: buffer start */
                        (uint16_t *)(ADCARESULT_BASE + ADC_O_RESULT0)); /* Source: ADC A RESULT0 */

    /* Configure burst: 15 words, srcStep=1, destStep=1 */
    DMA_configBurst(CT_DMA_CH_ADCA,
                    CT_ADCA_CHANNELS,   /* Burst size: 15 words */
                    1,                  /* Source step: +1 (next result register) */
                    1);                 /* Dest step: +1 (next array element) */

    /* Configure transfer: CT_BUFFER_SIZE bursts */
    /* srcStep = -(burstSize-1) to go back to RESULT0 */
    /* destStep = 1 to continue filling the array */
    DMA_configTransfer(CT_DMA_CH_ADCA,
                       CT_BUFFER_SIZE,              /* Transfer size: number of samples */
                       (int16_t)(1 - CT_ADCA_CHANNELS), /* Src step: -14 (back to RESULT0) */
                       1);                          /* Dest step: +1 (next location) */

    /* Configure wrap (not used, set to max) */
    DMA_configWrap(CT_DMA_CH_ADCA,
                   0xFFFF,  /* srcWrap: disabled */
                   0,       /* srcWrapStep */
                   0xFFFF,  /* destWrap: disabled */
                   0);      /* destWrapStep */

    /* Configure mode: Trigger on ADC A INT2, 16-bit, no oneshot, no continuous */
    DMA_configMode(CT_DMA_CH_ADCA,
                   DMA_TRIGGER_ADCA2,
                   DMA_CFG_ONESHOT_DISABLE |
                   DMA_CFG_CONTINUOUS_DISABLE |
                   DMA_CFG_SIZE_16BIT);

    /* Configure interrupt at end of transfer */
    DMA_setInterruptMode(CT_DMA_CH_ADCA, DMA_INT_AT_END);
    DMA_enableInterrupt(CT_DMA_CH_ADCA);

    /* Enable DMA trigger */
    DMA_enableTrigger(CT_DMA_CH_ADCA);

    /*
     * ==================== DMA CH2: ADC E (3 channels) ====================
     *
     * Source: ADCE Result registers (RESULT0-RESULT2)
     * Destination: g_adcE_rawResults[sample][channel]
     *
     * Burst: 3 words (all 3 SOC results)
     * Transfer: CT_BUFFER_SIZE bursts
     */

    /* Configure source and destination addresses */
    DMA_configAddresses(CT_DMA_CH_ADCE,
                        (uint16_t *)&g_adcE_rawResults[0][0],           /* Destination: buffer start */
                        (uint16_t *)(ADCERESULT_BASE + ADC_O_RESULT0)); /* Source: ADC E RESULT0 */

    /* Configure burst: 3 words, srcStep=1, destStep=1 */
    DMA_configBurst(CT_DMA_CH_ADCE,
                    CT_ADCE_CHANNELS,   /* Burst size: 3 words */
                    1,                  /* Source step: +1 */
                    1);                 /* Dest step: +1 */

    /* Configure transfer: CT_BUFFER_SIZE bursts */
    DMA_configTransfer(CT_DMA_CH_ADCE,
                       CT_BUFFER_SIZE,              /* Transfer size */
                       (int16_t)(1 - CT_ADCE_CHANNELS), /* Src step: -2 (back to RESULT0) */
                       1);                          /* Dest step: +1 */

    /* Configure wrap (not used) */
    DMA_configWrap(CT_DMA_CH_ADCE,
                   0xFFFF, 0,
                   0xFFFF, 0);

    /* Configure mode: Trigger on ADC E INT2, 16-bit */
    DMA_configMode(CT_DMA_CH_ADCE,
                   DMA_TRIGGER_ADCE2,
                   DMA_CFG_ONESHOT_DISABLE |
                   DMA_CFG_CONTINUOUS_DISABLE |
                   DMA_CFG_SIZE_16BIT);

    /* Configure interrupt at end of transfer */
    DMA_setInterruptMode(CT_DMA_CH_ADCE, DMA_INT_AT_END);
    DMA_enableInterrupt(CT_DMA_CH_ADCE);

    /* Enable DMA trigger */
    DMA_enableTrigger(CT_DMA_CH_ADCE);

    /* Reset status */
    g_ctDmaStatus.adcA_complete = false;
    g_ctDmaStatus.adcE_complete = false;
    g_ctDmaStatus.all_complete = false;
}

/**
 * @brief   Start DMA transfers for CT acquisition
 */
void CT_DMA_start(void)
{
    /* Clear status flags */
    g_ctDmaStatus.adcA_complete = false;
    g_ctDmaStatus.adcE_complete = false;
    g_ctDmaStatus.all_complete = false;

    /* Clear DMA trigger flags */
    DMA_clearTriggerFlag(CT_DMA_CH_ADCA);
    DMA_clearTriggerFlag(CT_DMA_CH_ADCE);

    /* Start both DMA channels */
    DMA_startChannel(CT_DMA_CH_ADCA);
    DMA_startChannel(CT_DMA_CH_ADCE);
}

/**
 * @brief   Stop DMA transfers
 */
void CT_DMA_stop(void)
{
    DMA_stopChannel(CT_DMA_CH_ADCA);
    DMA_stopChannel(CT_DMA_CH_ADCE);
}

/**
 * @brief   Reset DMA for next acquisition cycle
 */
void CT_DMA_reset(void)
{
    /* Stop channels first */
    CT_DMA_stop();

    /* Clear trigger flags */
    DMA_clearTriggerFlag(CT_DMA_CH_ADCA);
    DMA_clearTriggerFlag(CT_DMA_CH_ADCE);

    /* Reset source and destination addresses for ADC A */
    DMA_configAddresses(CT_DMA_CH_ADCA,
                        (uint16_t *)&g_adcA_rawResults[0][0],
                        (uint16_t *)(ADCARESULT_BASE + ADC_O_RESULT0));

    /* Reset source and destination addresses for ADC E */
    DMA_configAddresses(CT_DMA_CH_ADCE,
                        (uint16_t *)&g_adcE_rawResults[0][0],
                        (uint16_t *)(ADCERESULT_BASE + ADC_O_RESULT0));

    /* Clear status flags */
    g_ctDmaStatus.adcA_complete = false;
    g_ctDmaStatus.adcE_complete = false;
    g_ctDmaStatus.all_complete = false;
}

/**
 * @brief   Check if all DMA transfers are complete
 */
bool CT_DMA_isComplete(void)
{
    return g_ctDmaStatus.all_complete;
}

/**
 * @brief   Copy raw ADC results to per-CT buffers with offset removal
 *          Maps ADC channels to CT numbers based on phase assignment
 *          Uses CT_DC_OFFSET from power_config.h (configurable, default: 2010)
 *
 * Mapping:
 *   CT1-CT5:   ADC A SOC0-4   (R-phase)
 *   CT6:       ADC E SOC0     (R-phase)
 *   CT7:       ADC E SOC1     (Y-phase)
 *   CT8-CT11:  ADC A SOC5-8   (Y-phase)
 *   CT12:      ADC E SOC2     (Y-phase)
 *   CT13-CT18: ADC A SOC9-14  (B-phase)
 */
#pragma CODE_SECTION(CT_DMA_copyToBuffers, ".TI.ramfunc")
void CT_DMA_copyToBuffers(void)
{
    uint32_t i;
    int16_t rawValue;
    const int16_t dcOffset = CT_DC_OFFSET;

    /* Process all samples */
    for(i = 0; i < CT_BUFFER_SIZE; i++)
    {
        /*
         * R-Phase CTs (CT1-CT6)
         */
        /* CT1: ADC A SOC0 */
        rawValue = (int16_t)g_adcA_rawResults[i][0];
        g_CT1_Buffer[i] = rawValue - dcOffset;

        /* CT2: ADC A SOC1 */
        rawValue = (int16_t)g_adcA_rawResults[i][1];
        g_CT2_Buffer[i] = rawValue - dcOffset;

        /* CT3: ADC A SOC2 */
        rawValue = (int16_t)g_adcA_rawResults[i][2];
        g_CT3_Buffer[i] = rawValue - dcOffset;

        /* CT4: ADC A SOC3 */
        rawValue = (int16_t)g_adcA_rawResults[i][3];
        g_CT4_Buffer[i] = rawValue - dcOffset;

        /* CT5: ADC A SOC4 */
        rawValue = (int16_t)g_adcA_rawResults[i][4];
        g_CT5_Buffer[i] = rawValue - dcOffset;

        /* CT6: ADC E SOC0 */
        rawValue = (int16_t)g_adcE_rawResults[i][0];
        g_CT6_Buffer[i] = rawValue - dcOffset;

        /*
         * Y-Phase CTs (CT7-CT12)
         */
        /* CT7: ADC E SOC1 */
        rawValue = (int16_t)g_adcE_rawResults[i][1];
        g_CT7_Buffer[i] = rawValue - dcOffset;

        /* CT8: ADC A SOC5 */
        rawValue = (int16_t)g_adcA_rawResults[i][5];
        g_CT8_Buffer[i] = rawValue - dcOffset;

        /* CT9: ADC A SOC6 */
        rawValue = (int16_t)g_adcA_rawResults[i][6];
        g_CT9_Buffer[i] = rawValue - dcOffset;

        /* CT10: ADC A SOC7 */
        rawValue = (int16_t)g_adcA_rawResults[i][7];
        g_CT10_Buffer[i] = rawValue - dcOffset;

        /* CT11: ADC A SOC8 */
        rawValue = (int16_t)g_adcA_rawResults[i][8];
        g_CT11_Buffer[i] = rawValue - dcOffset;

        /* CT12: ADC E SOC2 */
        rawValue = (int16_t)g_adcE_rawResults[i][2];
        g_CT12_Buffer[i] = rawValue - dcOffset;

        /*
         * B-Phase CTs (CT13-CT18)
         */
        /* CT13: ADC A SOC9 */
        rawValue = (int16_t)g_adcA_rawResults[i][9];
        g_CT13_Buffer[i] = rawValue - dcOffset;

        /* CT14: ADC A SOC10 */
        rawValue = (int16_t)g_adcA_rawResults[i][10];
        g_CT14_Buffer[i] = rawValue - dcOffset;

        /* CT15: ADC A SOC11 */
        rawValue = (int16_t)g_adcA_rawResults[i][11];
        g_CT15_Buffer[i] = rawValue - dcOffset;

        /* CT16: ADC A SOC12 */
        rawValue = (int16_t)g_adcA_rawResults[i][12];
        g_CT16_Buffer[i] = rawValue - dcOffset;

        /* CT17: ADC A SOC13 */
        rawValue = (int16_t)g_adcA_rawResults[i][13];
        g_CT17_Buffer[i] = rawValue - dcOffset;

        /* CT18: ADC A SOC14 */
        rawValue = (int16_t)g_adcA_rawResults[i][14];
        g_CT18_Buffer[i] = rawValue - dcOffset;
    }
}

/**
 * @brief   Get pointer to specific CT buffer
 */
volatile int16_t* CT_DMA_getBuffer(uint8_t ctNumber)
{
    if(ctNumber < 1U || ctNumber > CT_TOTAL_CHANNELS)
    {
        return NULL;
    }

    return g_ctBufferPtrs[ctNumber - 1U];
}

/**
 * @brief   Get pointers to all 6 CT buffers for a phase
 */
void CT_DMA_getPhaseBuffers(uint8_t phase, volatile int16_t* buffers[6])
{
    switch(phase)
    {
        case CT_PHASE_R:
            buffers[0] = g_CT1_Buffer;
            buffers[1] = g_CT2_Buffer;
            buffers[2] = g_CT3_Buffer;
            buffers[3] = g_CT4_Buffer;
            buffers[4] = g_CT5_Buffer;
            buffers[5] = g_CT6_Buffer;
            break;

        case CT_PHASE_Y:
            buffers[0] = g_CT7_Buffer;
            buffers[1] = g_CT8_Buffer;
            buffers[2] = g_CT9_Buffer;
            buffers[3] = g_CT10_Buffer;
            buffers[4] = g_CT11_Buffer;
            buffers[5] = g_CT12_Buffer;
            break;

        case CT_PHASE_B:
            buffers[0] = g_CT13_Buffer;
            buffers[1] = g_CT14_Buffer;
            buffers[2] = g_CT15_Buffer;
            buffers[3] = g_CT16_Buffer;
            buffers[4] = g_CT17_Buffer;
            buffers[5] = g_CT18_Buffer;
            break;

        default:
            /* Invalid phase, set all to NULL */
            buffers[0] = NULL;
            buffers[1] = NULL;
            buffers[2] = NULL;
            buffers[3] = NULL;
            buffers[4] = NULL;
            buffers[5] = NULL;
            break;
    }
}

/**
 * @brief   DMA CH1 ISR handler (ADC A complete)
 */
void CT_DMA_ch1ISR(void)
{
    /* Mark ADC A transfer complete */
    g_ctDmaStatus.adcA_complete = true;

    /* Check if both transfers are complete */
    if(g_ctDmaStatus.adcE_complete)
    {
        g_ctDmaStatus.all_complete = true;
        g_ctDmaStatus.transferCount++;
    }

    /* Clear DMA trigger flag */
    DMA_clearTriggerFlag(CT_DMA_CH_ADCA);
}

/**
 * @brief   DMA CH2 ISR handler (ADC E complete)
 */
void CT_DMA_ch2ISR(void)
{
    /* Mark ADC E transfer complete */
    g_ctDmaStatus.adcE_complete = true;

    /* Check if both transfers are complete */
    if(g_ctDmaStatus.adcA_complete)
    {
        g_ctDmaStatus.all_complete = true;
        g_ctDmaStatus.transferCount++;
    }

    /* Clear DMA trigger flag */
    DMA_clearTriggerFlag(CT_DMA_CH_ADCE);
}

/* ========================================================================== */
/*                    Ping-Pong Buffer Functions                              */
/* ========================================================================== */

/**
 * @brief   Copy raw ADC results to active ping-pong buffer with DC offset removal
 *          Uses CT_DC_OFFSET from power_config.h (default: 2010)
 *
 * CT Mapping:
 *   Index 0-4:   CT1-CT5   (ADC A SOC0-4)   R-phase
 *   Index 5:     CT6       (ADC E SOC0)     R-phase
 *   Index 6:     CT7       (ADC E SOC1)     Y-phase
 *   Index 7-10:  CT8-CT11  (ADC A SOC5-8)   Y-phase
 *   Index 11:    CT12      (ADC E SOC2)     Y-phase
 *   Index 12-17: CT13-CT18 (ADC A SOC9-14)  B-phase
 */
#pragma CODE_SECTION(CT_DMA_copyToPingPong, ".TI.ramfunc")
void CT_DMA_copyToPingPong(void)
{
    uint32_t i;
    int16_t rawValue;
    const int16_t dcOffset = CT_DC_OFFSET;
    volatile int16_t (*destBuffer)[CT_BUFFER_SIZE];

    /* Select active buffer */
    if(g_ctPingPongStatus.activeBuffer == CT_BUFFER_A)
    {
        destBuffer = g_ctPingPongA;
    }
    else
    {
        destBuffer = g_ctPingPongB;
    }

    /* Process all samples for all 18 CTs */
    for(i = 0; i < CT_BUFFER_SIZE; i++)
    {
        /*
         * R-Phase CTs (Index 0-5 = CT1-CT6)
         */
        /* CT1: ADC A SOC0 */
        rawValue = (int16_t)g_adcA_rawResults[i][0];
        destBuffer[0][i] = rawValue - dcOffset;

        /* CT2: ADC A SOC1 */
        rawValue = (int16_t)g_adcA_rawResults[i][1];
        destBuffer[1][i] = rawValue - dcOffset;

        /* CT3: ADC A SOC2 */
        rawValue = (int16_t)g_adcA_rawResults[i][2];
        destBuffer[2][i] = rawValue - dcOffset;

        /* CT4: ADC A SOC3 */
        rawValue = (int16_t)g_adcA_rawResults[i][3];
        destBuffer[3][i] = rawValue - dcOffset;

        /* CT5: ADC A SOC4 */
        rawValue = (int16_t)g_adcA_rawResults[i][4];
        destBuffer[4][i] = rawValue - dcOffset;

        /* CT6: ADC E SOC0 */
        rawValue = (int16_t)g_adcE_rawResults[i][0];
        destBuffer[5][i] = rawValue - dcOffset;

        /*
         * Y-Phase CTs (Index 6-11 = CT7-CT12)
         */
        /* CT7: ADC E SOC1 */
        rawValue = (int16_t)g_adcE_rawResults[i][1];
        destBuffer[6][i] = rawValue - dcOffset;

        /* CT8: ADC A SOC5 */
        rawValue = (int16_t)g_adcA_rawResults[i][5];
        destBuffer[7][i] = rawValue - dcOffset;

        /* CT9: ADC A SOC6 */
        rawValue = (int16_t)g_adcA_rawResults[i][6];
        destBuffer[8][i] = rawValue - dcOffset;

        /* CT10: ADC A SOC7 */
        rawValue = (int16_t)g_adcA_rawResults[i][7];
        destBuffer[9][i] = rawValue - dcOffset;

        /* CT11: ADC A SOC8 */
        rawValue = (int16_t)g_adcA_rawResults[i][8];
        destBuffer[10][i] = rawValue - dcOffset;

        /* CT12: ADC E SOC2 */
        rawValue = (int16_t)g_adcE_rawResults[i][2];
        destBuffer[11][i] = rawValue - dcOffset;

        /*
         * B-Phase CTs (Index 12-17 = CT13-CT18)
         */
        /* CT13: ADC A SOC9 */
        rawValue = (int16_t)g_adcA_rawResults[i][9];
        destBuffer[12][i] = rawValue - dcOffset;

        /* CT14: ADC A SOC10 */
        rawValue = (int16_t)g_adcA_rawResults[i][10];
        destBuffer[13][i] = rawValue - dcOffset;

        /* CT15: ADC A SOC11 */
        rawValue = (int16_t)g_adcA_rawResults[i][11];
        destBuffer[14][i] = rawValue - dcOffset;

        /* CT16: ADC A SOC12 */
        rawValue = (int16_t)g_adcA_rawResults[i][12];
        destBuffer[15][i] = rawValue - dcOffset;

        /* CT17: ADC A SOC13 */
        rawValue = (int16_t)g_adcA_rawResults[i][13];
        destBuffer[16][i] = rawValue - dcOffset;

        /* CT18: ADC A SOC14 */
        rawValue = (int16_t)g_adcA_rawResults[i][14];
        destBuffer[17][i] = rawValue - dcOffset;
    }

    /* Also copy to legacy individual buffers for backward compatibility */
    for(i = 0; i < CT_BUFFER_SIZE; i++)
    {
        g_CT1_Buffer[i]  = destBuffer[0][i];
        g_CT2_Buffer[i]  = destBuffer[1][i];
        g_CT3_Buffer[i]  = destBuffer[2][i];
        g_CT4_Buffer[i]  = destBuffer[3][i];
        g_CT5_Buffer[i]  = destBuffer[4][i];
        g_CT6_Buffer[i]  = destBuffer[5][i];
        g_CT7_Buffer[i]  = destBuffer[6][i];
        g_CT8_Buffer[i]  = destBuffer[7][i];
        g_CT9_Buffer[i]  = destBuffer[8][i];
        g_CT10_Buffer[i] = destBuffer[9][i];
        g_CT11_Buffer[i] = destBuffer[10][i];
        g_CT12_Buffer[i] = destBuffer[11][i];
        g_CT13_Buffer[i] = destBuffer[12][i];
        g_CT14_Buffer[i] = destBuffer[13][i];
        g_CT15_Buffer[i] = destBuffer[14][i];
        g_CT16_Buffer[i] = destBuffer[15][i];
        g_CT17_Buffer[i] = destBuffer[16][i];
        g_CT18_Buffer[i] = destBuffer[17][i];
    }
}

/**
 * @brief   Swap ping-pong buffers
 */
void CT_DMA_swapPingPong(void)
{
    /* Toggle active buffer */
    if(g_ctPingPongStatus.activeBuffer == CT_BUFFER_A)
    {
        g_ctPingPongStatus.activeBuffer = CT_BUFFER_B;
    }
    else
    {
        g_ctPingPongStatus.activeBuffer = CT_BUFFER_A;
    }

    /* Mark inactive buffer as ready for processing */
    g_ctPingPongStatus.bufferReady = true;
}

/**
 * @brief   Get pointer to active ping-pong buffer for a specific CT
 */
volatile int16_t* CT_DMA_getActiveCTBuffer(uint8_t ctNumber)
{
    if(ctNumber < 1U || ctNumber > CT_TOTAL_CHANNELS)
    {
        return NULL;
    }

    uint8_t ctIndex = ctNumber - 1U;

    if(g_ctPingPongStatus.activeBuffer == CT_BUFFER_A)
    {
        return g_ctPingPongA[ctIndex];
    }
    else
    {
        return g_ctPingPongB[ctIndex];
    }
}

/**
 * @brief   Get pointer to inactive (ready for processing) ping-pong buffer for a specific CT
 */
volatile int16_t* CT_DMA_getInactiveCTBuffer(uint8_t ctNumber)
{
    if(ctNumber < 1U || ctNumber > CT_TOTAL_CHANNELS)
    {
        return NULL;
    }

    uint8_t ctIndex = ctNumber - 1U;

    /* Return the opposite of active buffer */
    if(g_ctPingPongStatus.activeBuffer == CT_BUFFER_A)
    {
        return g_ctPingPongB[ctIndex];
    }
    else
    {
        return g_ctPingPongA[ctIndex];
    }
}

/**
 * @brief   Get pointers to all active buffers for a phase (6 CTs)
 */
void CT_DMA_getActivePhaseBuffers(uint8_t phase, volatile int16_t* buffers[6])
{
    uint8_t startIndex;
    uint8_t i;
    volatile int16_t (*srcBuffer)[CT_BUFFER_SIZE];

    /* Select active buffer */
    if(g_ctPingPongStatus.activeBuffer == CT_BUFFER_A)
    {
        srcBuffer = g_ctPingPongA;
    }
    else
    {
        srcBuffer = g_ctPingPongB;
    }

    /* Determine start index based on phase */
    switch(phase)
    {
        case CT_PHASE_R:
            startIndex = 0U;   /* CT1-CT6: Index 0-5 */
            break;
        case CT_PHASE_Y:
            startIndex = 6U;   /* CT7-CT12: Index 6-11 */
            break;
        case CT_PHASE_B:
            startIndex = 12U;  /* CT13-CT18: Index 12-17 */
            break;
        default:
            for(i = 0; i < 6; i++) buffers[i] = NULL;
            return;
    }

    /* Fill buffer pointers */
    for(i = 0; i < 6; i++)
    {
        buffers[i] = srcBuffer[startIndex + i];
    }
}

/**
 * @brief   Get pointers to all inactive buffers for a phase (6 CTs)
 */
void CT_DMA_getInactivePhaseBuffers(uint8_t phase, volatile int16_t* buffers[6])
{
    uint8_t startIndex;
    uint8_t i;
    volatile int16_t (*srcBuffer)[CT_BUFFER_SIZE];

    /* Select inactive buffer (opposite of active) */
    if(g_ctPingPongStatus.activeBuffer == CT_BUFFER_A)
    {
        srcBuffer = g_ctPingPongB;
    }
    else
    {
        srcBuffer = g_ctPingPongA;
    }

    /* Determine start index based on phase */
    switch(phase)
    {
        case CT_PHASE_R:
            startIndex = 0U;   /* CT1-CT6: Index 0-5 */
            break;
        case CT_PHASE_Y:
            startIndex = 6U;   /* CT7-CT12: Index 6-11 */
            break;
        case CT_PHASE_B:
            startIndex = 12U;  /* CT13-CT18: Index 12-17 */
            break;
        default:
            for(i = 0; i < 6; i++) buffers[i] = NULL;
            return;
    }

    /* Fill buffer pointers */
    for(i = 0; i < 6; i++)
    {
        buffers[i] = srcBuffer[startIndex + i];
    }
}

/**
 * @brief   Check if inactive buffer has valid data ready for processing
 */
bool CT_DMA_isBufferReady(void)
{
    return g_ctPingPongStatus.bufferReady;
}

/**
 * @brief   Clear buffer ready flag after processing inactive buffer
 */
void CT_DMA_clearBufferReady(void)
{
    g_ctPingPongStatus.bufferReady = false;
}

/**
 * @brief   Get current active buffer identifier
 */
uint8_t CT_DMA_getActiveBufferId(void)
{
    return g_ctPingPongStatus.activeBuffer;
}
