/**
 * @file main.c
 * @brief Main application for PDU firmware
 */

#include "common.h"
#include "can_driver.h"
#include "flash_driver.h"
#include "timer_driver.h"
#include "pdu_manager.h"
#include "bu_board.h"
#include "m_board.h"
#include "s_board_adc.h"
#include "runtime.h"
#include "runtimedata.h"
#include "s_board_runtime_transmission.h"
#include "s_board_runtime_timing.h"
// time_slot.h removed — all work runs sequentially in while loop
#include "i2c_sboard.h"
#include "metro.h"
#include "phase_detection.h"
// I2C
#include "i2c_scanner.h"
//THD
#include "c2000ware_libraries.h"
#include "THD_Module/THD_Module.h"

bool discoveryComplete = false;
volatile bool runtimeActive = true;
int m=0;
int t,z;

int counter1 = 0, counter2 = 0, counter3 = 0;


int c,d;


//
// Sample Configuration
//
#define NUM_SAMPLES             256     // Total ADC samples to capture per phase
#define RESULTS_BUFFER_SIZE     NUM_SAMPLES

//
// Three-Phase Configuration
//
#define NUM_PHASES              3       // Number of phases (R, Y, B)
#define PHASE_R                 0       // R phase index
#define PHASE_Y                 1       // Y phase index
#define PHASE_B                 2       // B phase index

#define CAN_VOLTAGE_DATA_START  2
#define VALUES_PER_FRAME_CAN    31
#define VOLTAGE_BUFFER_SIZE_CAN NUM_SAMPLES
#define FRAMES_PER_BUFFER       ((NUM_SAMPLES + VALUES_PER_FRAME_CAN - 1) / VALUES_PER_FRAME_CAN)

// =============================================================================
// ADC CONFIGURATION
// =============================================================================
#define ADC_MODULE_BASE         myADC0_BASE         // ADC module base address
#define ADC_RESULT_BASE         ADCARESULT_BASE     // ADC result register base

// ADC SOC (Start of Conversion) assignments per phase
#define ADC_SOC_R               ADC_SOC_NUMBER0     // R phase uses SOC0
#define ADC_SOC_Y               ADC_SOC_NUMBER1     // Y phase uses SOC1
#define ADC_SOC_B               ADC_SOC_NUMBER2     // B phase uses SOC2

// ADC Result Register Offsets for each SOC
#define ADC_RESULT_R            (ADC_RESULT_BASE + ADC_O_RESULT0)   // SOC0 result
#define ADC_RESULT_Y            (ADC_RESULT_BASE + ADC_O_RESULT1)   // SOC1 result
#define ADC_RESULT_B            (ADC_RESULT_BASE + ADC_O_RESULT2)   // SOC2 result

// ADC Interrupt assignments for DMA triggering
#define ADC_INT_R               ADC_INT_NUMBER2     // ADC INT2 for R phase DMA
#define ADC_INT_Y               ADC_INT_NUMBER3     // ADC INT3 for Y phase DMA
#define ADC_INT_B               ADC_INT_NUMBER4     // ADC INT4 for B phase DMA
#define ADC_INT_PLAYBACK        ADC_INT_NUMBER1     // ADC INT1 for DAC playback ISR

// ADC EOC (End of Conversion) triggers for interrupts
#define ADC_EOC_TRIGGER_R       ADC_INT_TRIGGER_EOC0    // SOC0 EOC triggers INT2
#define ADC_EOC_TRIGGER_Y       ADC_INT_TRIGGER_EOC1    // SOC1 EOC triggers INT3
#define ADC_EOC_TRIGGER_B       ADC_INT_TRIGGER_EOC2    // SOC2 EOC triggers INT4

// =============================================================================
// DMA CONFIGURATION
// =============================================================================
// DMA Channel Bases for each phase
#define DMA_CH_R_BASE           DMA_CH1_BASE        // DMA Channel 1 for R phase
#define DMA_CH_Y_BASE           DMA_CH2_BASE        // DMA Channel 2 for Y phase
#define DMA_CH_B_BASE           DMA_CH3_BASE        // DMA Channel 3 for B phase

// DMA Interrupt assignments
#define DMA_INT_R               INT_DMA_CH1         // DMA CH1 interrupt for R phase
#define DMA_INT_Y               INT_DMA_CH2         // DMA CH2 interrupt for Y phase
#define DMA_INT_B               INT_DMA_CH3         // DMA CH3 interrupt for B phase

// DMA Triggers (ADC interrupts that trigger DMA)
#define DMA_TRIGGER_R           DMA_TRIGGER_ADCA2   // ADC INT2 triggers R phase DMA
#define DMA_TRIGGER_Y           DMA_TRIGGER_ADCA3   // ADC INT3 triggers Y phase DMA
#define DMA_TRIGGER_B           DMA_TRIGGER_ADCA4   // ADC INT4 triggers B phase DMA

// DMA Transfer Configuration
#define DMA_BURST_SIZE          1       // Words per burst (1 = single word per trigger)
#define DMA_SRC_BURST_STEP      0       // Source address step per burst (0 = no change)
#define DMA_DEST_BURST_STEP     1       // Destination address step per burst
#define DMA_SRC_TRANSFER_STEP   0       // Source address step per transfer
#define DMA_DEST_TRANSFER_STEP  1       // Destination address step per transfer

// =============================================================================
// EPWM CONFIGURATION (ADC Trigger Source)
// =============================================================================
#define EPWM_TRIGGER_BASE       EPWM7_BASE          // ePWM module for ADC triggering
#define EPWM_SOC_TRIGGER        EPWM_SOC_A          // SOC A or SOC B
#define EPWM_TRIGGER_SOURCE     EPWM_SOC_TBCTR_U_CMPA   // Trigger on CMPA up-count
#define EPWM_TRIGGER_PRESCALE   1                   // Trigger every N events

// ePWM Timing Configuration (for 5.12kHz sample rate @ 120MHz SYSCLK)
#define EPWM_PERIOD_COUNT       23436               // Period = (SYSCLK/SampleRate) - 1
#define EPWM_CMPA_COUNT         11719               // Compare A value (50% duty)
#define EPWM_CLK_DIV            EPWM_CLOCK_DIVIDER_1        // Clock divider
#define EPWM_HSCLK_DIV          EPWM_HSCLOCK_DIVIDER_1      // High-speed clock divider

// =============================================================================
// DAC CONFIGURATION
// =============================================================================
#define DAC_MODULE_BASE         DACA_BASE           // DAC module base address
#define DAC_REF_VOLTAGE         DAC_REF_ADC_VREFHI  // Reference voltage source
#define DAC_GAIN                DAC_GAIN_ONE        // DAC gain (1x or 2x)
#define DAC_LOAD_MODE           DAC_LOAD_SYSCLK     // DAC load mode
#define DAC_INITIAL_VALUE       2048                // Initial DAC output (midscale for 12-bit)
#define DAC_POWERUP_DELAY_US    10                  // DAC power-up delay in microseconds

// Playback buffer selection (which phase to play back on DAC)
#define DAC_PLAYBACK_BUFFER     adcBufferR          // Buffer to use for DAC playback

// =============================================================================
// CAN FD CONFIGURATION
// =============================================================================
#define CAN_MODULE_BASE         MCANA_DRIVER_BASE   // MCAN module base address
#define CAN_TX_BUFFER_INDEX     0                   // TX buffer index (0-31)

// CAN FD Frame Configuration
#define CAN_FD_ENABLE           1                   // 1 = CAN FD, 0 = Classic CAN
#define CAN_BRS_ENABLE          1                   // 1 = Bit rate switching enabled
#define CAN_DLC_VALUE           15                  // DLC for 64-byte CAN FD frame
#define CAN_FRAME_SIZE          64                  // CAN FD frame data size in bytes
#define CAN_ID_SHIFT            18U                 // Bit shift for standard CAN ID
#define CAN_MSG_MARKER          0xAAU               // Message marker for TX event FIFO
#define CAN_EFC_ENABLE          1                   // Event FIFO control enable

// CAN TX Timeout Configuration
#define CAN_TX_TIMEOUT          100000              // TX timeout counter value

// CAN Data Packing Configuration
#define CAN_VOLTAGE_DATA_START  2                   // Byte index where voltage data starts
#define VALUES_PER_FRAME        31                  // 16-bit values per frame ((64-2)/2)
#define VOLTAGE_BUFFER_SIZE     NUM_SAMPLES
#define FRAMES_PER_BUFFER       ((NUM_SAMPLES + VALUES_PER_FRAME - 1) / VALUES_PER_FRAME)

// CAN Message IDs for each phase
#define CAN_ID_R                0x05                // CAN ID for R phase voltage data
#define CAN_ID_Y                0x06                // CAN ID for Y phase voltage data
#define CAN_ID_B                0x07                // CAN ID for B phase voltage data
#define CAN_ID_INIT             0x05                // CAN ID for init message

// Phase identifiers sent in byte 0 of each CAN frame
#define PHASE_ID_R              1                   // R phase identifier in CAN frame
#define PHASE_ID_Y              2                   // Y phase identifier in CAN frame
#define PHASE_ID_B              3                   // B phase identifier in CAN frame

// =============================================================================
// SYSTEM TIMING CONFIGURATION
// =============================================================================
#define STARTUP_DELAY_US        100000              // Startup delay in microseconds

// =============================================================================
// MEMORY CONFIGURATION
// =============================================================================
// RAM sections for phase buffers (must match linker command file)
#define RAM_SECTION_R           "ramgs0"            // RAM section for R phase buffer
#define RAM_SECTION_Y           "ramgs1"            // RAM section for Y phase buffer
#define RAM_SECTION_B           "ramgs2"            // RAM section for B phase buffer

// =============================================================================
// INTERRUPT CONFIGURATION
// =============================================================================
#define DMA_INT_ACK_GROUP       INTERRUPT_ACK_GROUP7    // DMA interrupt ACK group

//I2C
//
// Application-level defines
//
#define DEBUG_PIN               29
#define TCA9555_ADDR_START      0x20    // First possible TCA9555 address
#define TCA9555_ADDR_END        0x28    // One past the last (0x20-0x27)
#define TCA9555_REG_OUTPUT0     0x00    // Register to probe

//
// Globals - Three-Phase ADC Buffers in dedicated RAM sections
// Note: Pragma strings must match RAM_SECTION_R/Y/B defines above
//       Update both if changing RAM section assignments
//
#pragma DATA_SECTION(adcBufferR, "ramgs0");  // Must match RAM_SECTION_R
#pragma DATA_SECTION(adcBufferY, "ramgs1");  // Must match RAM_SECTION_Y
#pragma DATA_SECTION(adcBufferB, "ramgs2");  // Must match RAM_SECTION_B
uint16_t adcBufferR[RESULTS_BUFFER_SIZE];    // R phase voltage samples
uint16_t adcBufferY[RESULTS_BUFFER_SIZE];    // Y phase voltage samples
uint16_t adcBufferB[RESULTS_BUFFER_SIZE];    // B phase voltage samples

// DMA completion flags for each phase
volatile uint16_t dmaCompleteR = 0;
volatile uint16_t dmaCompleteY = 0;
volatile uint16_t dmaCompleteB = 0;
volatile uint16_t done = 0;    // All phases complete flag

//
// DAC Globals
//
volatile uint16_t dacOutput = 0;
volatile uint16_t adcInput = 0;

//
// Playback Globals
//
volatile uint16_t playbackIndex = 0;
volatile uint16_t isPlaybackMode = 0;  // 0 = capture mode, 1 = playback mode

int counter_send=0,CAN_COUNT;
uint16_t vrmsR, vrmsY, vrmsB;

uint32_t Timer500MS = 0;
//THD

// ****************************************************************************
// ALIGNED FFT BUFFERS (hardware alignment required by RFFT library)
// ****************************************************************************
#ifdef __cplusplus
    #pragma DATA_SECTION("RFFTdata1")
    #pragma DATA_ALIGN(1024)
#else
    #pragma DATA_SECTION(RFFTin1Buff,"RFFTdata1")
    #pragma DATA_ALIGN(RFFTin1Buff, 1024)
#endif
float32_t RFFTin1Buff[THD_FFT_SIZE];

#ifdef __cplusplus
    #pragma DATA_SECTION("RFFTdata2")
#else
    #pragma DATA_SECTION(RFFTmagBuff,"RFFTdata2")
#endif
float32_t RFFTmagBuff[THD_FFT_SIZE/2+1];

#ifdef __cplusplus
    #pragma DATA_SECTION("RFFTdata3")
#else
    #pragma DATA_SECTION(RFFTphBuff,"RFFTdata3")
#endif
float32_t RFFTphBuff[THD_FFT_SIZE/2];

#ifdef __cplusplus
    #pragma DATA_SECTION("RFFTdata4")
#else
    #pragma DATA_SECTION(RFFToutBuff,"RFFTdata4")
#endif
float32_t RFFToutBuff[THD_FFT_SIZE];

float32_t RFFTTwiddleCoef[THD_FFT_SIZE];
float32_t WindowCoef[THD_FFT_SIZE];

// ****************************************************************************
// GLOBAL POINTERS (required by auto-generated c2000ware_libraries.c)
// ****************************************************************************
float32_t* inPtr      = RFFTin1Buff;
float32_t* outPtr     = RFFToutBuff;
float32_t* magPtr     = RFFTmagBuff;
float32_t* phPtr      = RFFTphBuff;
float32_t* twiddlePtr = RFFTTwiddleCoef;

// ****************************************************************************
// CHANNEL DATA STORAGE
// ****************************************************************************
float32_t AdcRawBuffers[THD_NUM_CHANNELS][THD_FFT_SIZE];

// ****************************************************************************
// ANALYZER INSTANCE
// ****************************************************************************
THD_Analyzer g_analyzer;

//

//
// Function Prototypes major
//
void initEPWM(void);
void initADC_DMA(void);
void initDAC(void);
void initDMAChannel(uint32_t dmaBase, uint16_t *destBuffer, uint32_t adcResultAddr, uint16_t dmaTrigger);
__interrupt void dmaCh1ISR(void);   // R phase DMA complete
__interrupt void dmaCh2ISR(void);   // Y phase DMA complete
__interrupt void dmaCh3ISR(void);   // B phase DMA complete
__interrupt void adcA1ISR(void);
void sendAllPhaseVoltages(void);
void sendPhaseVoltageBuffer(uint16_t *buffer, uint16_t phaseId, uint32_t canId, uint16_t vrms);
void sendSingleVoltageFrame(uint16_t frameNumber, uint16_t *buffer, uint16_t startIndex, uint16_t phaseId, uint32_t canId, bool isLastFrame, uint16_t vrms);


//
// Main
//
void can_init(){
    MCAN_TxBufElement TxMsg;
    MCAN_ProtocolStatus protStatus;
    uint32_t timeout = CAN_TX_TIMEOUT;

    //
    // Check MCAN protocol status before transmitting
    //
    MCAN_getProtocolStatus(CAN_MODULE_BASE, &protStatus);

    //
    // If bus-off, try to recover
    //
    if(protStatus.busOffStatus)
    {
        // Bus is in bus-off state, cannot transmit
        return;
    }

    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
    TxMsg.id = ((uint32_t)(CAN_ID_INIT)) << CAN_ID_SHIFT;
    TxMsg.brs = CAN_BRS_ENABLE;
    TxMsg.dlc = CAN_DLC_VALUE;
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = CAN_FD_ENABLE;
    TxMsg.esi = 0U;
    TxMsg.efc = CAN_EFC_ENABLE;
    TxMsg.mm = CAN_MSG_MARKER;

    uint8_t datare[CAN_FRAME_SIZE] = { 0x75, 0x00 };
    int i;
    for (i = 0; i < CAN_FRAME_SIZE; i++)
    {
        TxMsg.data[i] = datare[i];
    }

    MCAN_writeMsgRam(CAN_MODULE_BASE, MCAN_MEM_TYPE_BUF, CAN_TX_BUFFER_INDEX, &TxMsg);

    //
    // Add transmission request for Tx buffer
    //
    MCAN_txBufAddReq(CAN_MODULE_BASE, CAN_TX_BUFFER_INDEX);

    //
    // Wait till the frame is successfully transmitted (with timeout)
    //
    canTxWaitComplete(CAN_MODULE_BASE);
}


void sendAllPhaseVoltages(void)
{


    CAN_COUNT++;

    /*
     * Get Vrms from metrology for each phase.
     * RMSVoltage is float, convert to uint16_t (multiply by 10 for 1 decimal place precision)
     * e.g., 220.5V becomes 2205
     */
    vrmsR = (uint16_t)(gMetrologyWorkingData.phases[3].readings.RMSVoltage );
    vrmsY = (uint16_t)(gMetrologyWorkingData.phases[4].readings.RMSVoltage );
    vrmsB = (uint16_t)(gMetrologyWorkingData.phases[5].readings.RMSVoltage );

    sendPhaseVoltageBuffer(adcBufferR, PHASE_ID_R, CAN_ID_R, vrmsR);
    sendPhaseVoltageBuffer(adcBufferY, PHASE_ID_Y, CAN_ID_Y, vrmsY);
    sendPhaseVoltageBuffer(adcBufferB, PHASE_ID_B, CAN_ID_B, vrmsB);
}

void sendPhaseVoltageBuffer(uint16_t *buffer, uint16_t phaseId, uint32_t canId, uint16_t vrms)
{
    uint16_t frameNumber;
    uint16_t startIndex;
    bool isLastFrame;

    for (frameNumber = 1; frameNumber <= FRAMES_PER_BUFFER; frameNumber++)
    {
        startIndex = (frameNumber - 1) * VALUES_PER_FRAME_CAN;
        isLastFrame = (frameNumber == FRAMES_PER_BUFFER);
        sendSingleVoltageFrame(frameNumber, buffer, startIndex, phaseId, canId, isLastFrame, vrms);
    }
}

void sendSingleVoltageFrame(uint16_t frameNumber, uint16_t *buffer, uint16_t startIndex, uint16_t phaseId, uint32_t canId, bool isLastFrame, uint16_t vrms)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id = ((uint32_t)(canId)) << CAN_ID_SHIFT;
    TxMsg.brs = CAN_BRS_ENABLE;
    TxMsg.dlc = CAN_DLC_VALUE;
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = CAN_FD_ENABLE;
    TxMsg.esi = 0U;
    TxMsg.efc = CAN_EFC_ENABLE;
    TxMsg.mm = CAN_MSG_MARKER;

    uint16_t data[CAN_FRAME_SIZE] = {0};
    data[0] = phaseId;
    data[1] = frameNumber;

    uint16_t valuesInThisFrame = VALUES_PER_FRAME_CAN;
    if ((startIndex + VALUES_PER_FRAME_CAN) > VOLTAGE_BUFFER_SIZE_CAN)
    {
        valuesInThisFrame = VOLTAGE_BUFFER_SIZE_CAN - startIndex;
    }

    uint16_t dataIndex = CAN_VOLTAGE_DATA_START;
    uint16_t i;
    for (i = 0; i < valuesInThisFrame; i++)
    {
        uint16_t voltageValue = buffer[startIndex + i];
        data[dataIndex++] = (voltageValue >> 8) & 0xFF;
        data[dataIndex++] = voltageValue & 0xFF;
    }

    /*
     * For the last frame, add Vrms in the last 2 bytes (index 62 and 63).
     * Vrms is stored as: byte 62 = MSB (high byte), byte 63 = LSB (low byte)
     */
    if (isLastFrame)
    {
        data[62] = (vrms >> 8) & 0xFF;   /* Vrms high byte */
        data[63] = vrms & 0xFF;          /* Vrms low byte */
    }

    memcpy(TxMsg.data, data, CAN_FRAME_SIZE);

    MCAN_writeMsgRam(CAN_MODULE_BASE, MCAN_MEM_TYPE_BUF, CAN_TX_BUFFER_INDEX, &TxMsg);
    MCAN_txBufAddReq(CAN_MODULE_BASE, CAN_TX_BUFFER_INDEX);

    canTxWaitComplete(CAN_MODULE_BASE);
}



//
// initDMAChannel - Configure a single DMA channel for ADC transfer
// @param dmaBase - DMA channel base address
// @param destBuffer - Destination buffer pointer
// @param adcResultAddr - ADC result register address
// @param dmaTrigger - DMA trigger source
//
void initDMAChannel(uint32_t dmaBase, uint16_t *destBuffer, uint32_t adcResultAddr, uint16_t dmaTrigger)
{
    //
    // Configure DMA addresses
    // Source: ADC Result Register
    // Destination: Phase buffer in RAM
    //
    DMA_configAddresses(dmaBase, destBuffer, (uint16_t *)adcResultAddr);

    //
    // Configure DMA burst and transfer
    // Burst: DMA_BURST_SIZE words per trigger
    // Transfer: RESULTS_BUFFER_SIZE total transfers
    //
    DMA_configBurst(dmaBase, DMA_BURST_SIZE, DMA_SRC_BURST_STEP, DMA_DEST_BURST_STEP);
    DMA_configTransfer(dmaBase, RESULTS_BUFFER_SIZE, DMA_SRC_TRANSFER_STEP, DMA_DEST_TRANSFER_STEP);

    //
    // Configure DMA mode
    // One-shot: Disabled
    // Continuous: Disabled (stop after transferSize transfers)
    // Size: 16-bit transfers
    //
    DMA_configMode(dmaBase, dmaTrigger,
                   DMA_CFG_ONESHOT_DISABLE |
                   DMA_CFG_CONTINUOUS_DISABLE |
                   DMA_CFG_SIZE_16BIT);

    //
    // Configure DMA interrupt at end of transfer
    //
    DMA_setInterruptMode(dmaBase, DMA_INT_AT_END);
    DMA_enableInterrupt(dmaBase);

    //
    // Enable DMA trigger
    //
    DMA_enableTrigger(dmaBase);
}

//
// initADC_DMA - Configure ADC interrupts and DMA channels for all three phases
// Note: ADC INT1 remains enabled for DAC output in adcA1ISR
//       ADC INT2/3/4 are used to trigger DMA for R/Y/B phases respectively
//
void initADC_DMA(void)
{
    //
    // =========================================================================
    // Configure ADC Interrupts for DMA Triggering
    // Continuous mode auto-clears the flag - THIS IS THE KEY!
    // =========================================================================
    //

    //
    // ADC interrupt for R phase DMA triggering
    //
    ADC_setInterruptSource(ADC_MODULE_BASE, ADC_INT_R, ADC_EOC_TRIGGER_R);
    ADC_enableContinuousMode(ADC_MODULE_BASE, ADC_INT_R);
    ADC_enableInterrupt(ADC_MODULE_BASE, ADC_INT_R);
    ADC_clearInterruptStatus(ADC_MODULE_BASE, ADC_INT_R);

    //
    // ADC interrupt for Y phase DMA triggering
    //
    ADC_setInterruptSource(ADC_MODULE_BASE, ADC_INT_Y, ADC_EOC_TRIGGER_Y);
    ADC_enableContinuousMode(ADC_MODULE_BASE, ADC_INT_Y);
    ADC_enableInterrupt(ADC_MODULE_BASE, ADC_INT_Y);
    ADC_clearInterruptStatus(ADC_MODULE_BASE, ADC_INT_Y);

    //
    // ADC interrupt for B phase DMA triggering
    //
    ADC_setInterruptSource(ADC_MODULE_BASE, ADC_INT_B, ADC_EOC_TRIGGER_B);
    ADC_enableContinuousMode(ADC_MODULE_BASE, ADC_INT_B);
    ADC_enableInterrupt(ADC_MODULE_BASE, ADC_INT_B);
    ADC_clearInterruptStatus(ADC_MODULE_BASE, ADC_INT_B);

    //
    // =========================================================================
    // Configure DMA Channels for each phase
    // =========================================================================
    //

    //
    // DMA Channel for R phase (SOC result -> adcBufferR)
    //
    initDMAChannel(DMA_CH_R_BASE, adcBufferR, ADC_RESULT_R, DMA_TRIGGER_R);

    //
    // DMA Channel for Y phase (SOC result -> adcBufferY)
    //
    initDMAChannel(DMA_CH_Y_BASE, adcBufferY, ADC_RESULT_Y, DMA_TRIGGER_Y);

    //
    // DMA Channel for B phase (SOC result -> adcBufferB)
    //
    initDMAChannel(DMA_CH_B_BASE, adcBufferB, ADC_RESULT_B, DMA_TRIGGER_B);
}

//
// initEPWM - Function to configure ePWM to generate the ADC SOC trigger
//
void initEPWM(void)
{
    //
    // Disable SOC trigger initially
    //
    EPWM_disableADCTrigger(EPWM_TRIGGER_BASE, EPWM_SOC_TRIGGER);

    //
    // Configure the SOC trigger source and prescale
    //
    EPWM_setADCTriggerSource(EPWM_TRIGGER_BASE, EPWM_SOC_TRIGGER, EPWM_TRIGGER_SOURCE);
    EPWM_setADCTriggerEventPrescale(EPWM_TRIGGER_BASE, EPWM_SOC_TRIGGER, EPWM_TRIGGER_PRESCALE);

    //
    // Set the compare A value and period
    // Sample rate = SYSCLK / (EPWM_PERIOD_COUNT + 1)
    //
    EPWM_setCounterCompareValue(EPWM_TRIGGER_BASE, EPWM_COUNTER_COMPARE_A, EPWM_CMPA_COUNT);
    EPWM_setTimeBasePeriod(EPWM_TRIGGER_BASE, EPWM_PERIOD_COUNT);

    //
    // Set the local ePWM module clock divider
    //
    EPWM_setClockPrescaler(EPWM_TRIGGER_BASE, EPWM_CLK_DIV, EPWM_HSCLK_DIV);

    //
    // Enable SOC event counter initialization
    //
    EPWM_enableADCTriggerEventCountInit(EPWM_TRIGGER_BASE, EPWM_SOC_TRIGGER);

    //
    // Freeze the counter initially
    //
    EPWM_setTimeBaseCounterMode(EPWM_TRIGGER_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);
}

//
// dmaCh1ISR - DMA Channel 1 ISR (R Phase)
// Called when all NUM_SAMPLES samples have been transferred to adcBufferR
//
#pragma CODE_SECTION(dmaCh1ISR, ".TI.ramfunc");
__interrupt void dmaCh1ISR(void)
{
    //
    // Set flag to indicate R phase transfer complete
    //
    dmaCompleteR = 1;

    //
    // Clear DMA trigger flag
    //
    DMA_clearTriggerFlag(DMA_CH_R_BASE);

    //
    // Acknowledge interrupt to allow future interrupts from Group 7
    //
    Interrupt_clearACKGroup(DMA_INT_ACK_GROUP);
}

//
// dmaCh2ISR - DMA Channel 2 ISR (Y Phase)
// Called when all NUM_SAMPLES samples have been transferred to adcBufferY
//
#pragma CODE_SECTION(dmaCh2ISR, ".TI.ramfunc");
__interrupt void dmaCh2ISR(void)
{
    //
    // Set flag to indicate Y phase transfer complete
    //
    dmaCompleteY = 1;

    //
    // Clear DMA trigger flag
    //
    DMA_clearTriggerFlag(DMA_CH_Y_BASE);

    //
    // Acknowledge interrupt to allow future interrupts from Group 7
    //
    Interrupt_clearACKGroup(DMA_INT_ACK_GROUP);
}

//
// dmaCh3ISR - DMA Channel 3 ISR (B Phase)
// Called when all NUM_SAMPLES samples have been transferred to adcBufferB
//
#pragma CODE_SECTION(dmaCh3ISR, ".TI.ramfunc");
__interrupt void dmaCh3ISR(void)
{
    //
    // Set flag to indicate B phase transfer complete
    //
    dmaCompleteB = 1;

    //
    // Clear DMA trigger flag
    //
    DMA_clearTriggerFlag(DMA_CH_B_BASE);

    //
    // Acknowledge interrupt to allow future interrupts from Group 7
    //
    Interrupt_clearACKGroup(DMA_INT_ACK_GROUP);
}

//
// adcA1ISR - ADC Playback Interrupt ISR
// In playback mode: outputs captured samples to DAC
// In capture mode: does nothing (DMA handles capture)
//
#pragma CODE_SECTION(adcA1ISR, ".TI.ramfunc");
__interrupt void adcA1ISR(void)
{
    //
    // Check if in playback mode
    //
    if(isPlaybackMode)
    {
        //
        // Output captured sample to DAC (configurable playback buffer)
        //
        dacOutput = DAC_PLAYBACK_BUFFER[playbackIndex];
        DAC_setShadowValue(DAC_MODULE_BASE, dacOutput);

        //
        // Increment playback index
        //
        playbackIndex++;

        //
        // Check if all samples have been output
        //
        if(playbackIndex >= RESULTS_BUFFER_SIZE)
        {
            done = 1;
            playbackIndex = 0;
        }
    }
    //
    // In capture mode, do nothing - DMA handles capture
    //

    //Output Voltage
        ADCResults[0] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0)-2048);
        ADCResults[1] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1)-2048);
        ADCResults[2] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER2)-2048);
        //Output Current
        ADCResults[3] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER3));
        ADCResults[4] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER4));
        ADCResults[5] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER5));
        //Input Voltage
        ADCResults[6] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER6)-2048);
        ADCResults[7] = (signed int)(ADC_readResult(ADCBRESULT_BASE, ADC_SOC_NUMBER0)-2048);
        ADCResults[8] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER7)-2048);
        //Input Current
        ADCResults[9] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER8));
        ADCResults[10] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER9));
        ADCResults[11] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER10));
        //Ground Current
        ADCResults_Ground = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER11)-2048);

        if(gAppHandle.adcStatus == ADC_READY)
        {
            get_debugData(&gMetrologyWorkingData);
            Metrology_perSampleProcessing(&gMetrologyWorkingData);
           // Metrology_perSampleEnergyPulseProcessing(&gMetrologyWorkingData);

            /* Phase sequence detection on input voltage (already offset-corrected) */
            PhaseDetection_phaseSequenceISR(ADCResults[6], ADCResults[7], ADCResults[8]);
        }

        Timer500MS++;

        if (Timer500MS >= 2560)
        {
            Ms500Passed = true;
            Timer500MS=0;
        }
//THD
        uint16_t res0 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
        uint16_t res1 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1);
        uint16_t res2 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER2);
        uint16_t res3 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER3);
        uint16_t res4 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER4);
        uint16_t res5 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER5);
        uint16_t res6 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER6);
        uint16_t res7 = ADC_readResult(ADCBRESULT_BASE, ADC_SOC_NUMBER0);
        uint16_t res8 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER7);
        uint16_t res9 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER8);
        uint16_t res10 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER9);
        uint16_t res11 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER10);

        g_adcState.conversionCount++;

        if(g_adcState.sampleIndex < THD_FFT_SIZE)
        {
            uint16_t idx = g_adcState.sampleIndex;

            g_adcState.rawBuffers[0 ][idx] = (float32_t)res0  * THD_ADC_SCALE;
            g_adcState.rawBuffers[1 ][idx] = (float32_t)res1  * THD_ADC_SCALE;
            g_adcState.rawBuffers[2 ][idx] = (float32_t)res2  * THD_ADC_SCALE;
            g_adcState.rawBuffers[3 ][idx] = (float32_t)res3  * THD_ADC_SCALE;
            g_adcState.rawBuffers[4 ][idx] = (float32_t)res4  * THD_ADC_SCALE;
            g_adcState.rawBuffers[5 ][idx] = (float32_t)res5  * THD_ADC_SCALE;
            g_adcState.rawBuffers[6 ][idx] = (float32_t)res6  * THD_ADC_SCALE;
            g_adcState.rawBuffers[7 ][idx] = (float32_t)res7  * THD_ADC_SCALE;
            g_adcState.rawBuffers[8 ][idx] = (float32_t)res8  * THD_ADC_SCALE;
            g_adcState.rawBuffers[9 ][idx] = (float32_t)res9  * THD_ADC_SCALE;
            g_adcState.rawBuffers[10][idx] = (float32_t)res10 * THD_ADC_SCALE;
            g_adcState.rawBuffers[11][idx] = (float32_t)res11 * THD_ADC_SCALE;

            g_adcState.sampleIndex++;
        }

        if(g_adcState.sampleIndex >= THD_FFT_SIZE)
        {
            g_adcState.bufferFull = 1;
         //   Interrupt_disable(INT_ADCA1);
        }

    //
    // Clear the interrupt flag
    //
    ADC_clearInterruptStatus(ADC_MODULE_BASE, ADC_INT_PLAYBACK);

    //
    // Check if overflow has occurred
    //
    if(true == ADC_getInterruptOverflowStatus(ADC_MODULE_BASE, ADC_INT_PLAYBACK))
    {
        ADC_clearInterruptOverflowStatus(ADC_MODULE_BASE, ADC_INT_PLAYBACK);
    }

    //
    // Acknowledge the interrupt
    //
    Interrupt_clearACKGroup(INT_myADC0_1_INTERRUPT_ACK_GROUP);
}

//
// initDAC - Initialize DAC for analog output
//
void initDAC(void)
{
    //
    // Set DAC reference voltage
    //
    DAC_setReferenceVoltage(DAC_MODULE_BASE, DAC_REF_VOLTAGE);

    //
    // Set DAC gain mode
    //
    DAC_setGainMode(DAC_MODULE_BASE, DAC_GAIN);

    //
    // Set DAC load mode
    //
    DAC_setLoadMode(DAC_MODULE_BASE, DAC_LOAD_MODE);

    //
    // Enable DAC output
    //
    DAC_enableOutput(DAC_MODULE_BASE);

    //
    // Set initial DAC output value
    //
    DAC_setShadowValue(DAC_MODULE_BASE, DAC_INITIAL_VALUE);

    //
    // Delay for DAC to power up
    //
    DEVICE_DELAY_US(DAC_POWERUP_DELAY_US);
}

//
// End of file




__interrupt void  INT_Cputimer_ISR()
{
    c++;
    timerSystemTick();
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}




/**
 * @brief System startup initialization
 */
extern uint16_t FPUmathTablesLoadStart;
extern uint16_t FPUmathTablesLoadSize;
extern uint16_t FPUmathTablesRunStart;

void startup(void)
{
    //
        // 1. Core Device Initialization
        //
        Device_init();
        Device_initGPIO();

        //
        // 1b. Copy FPU math tables (sin/cos lookup) from flash to RAM
        //     Required for RFFT twiddle factor generation
        //
        memcpy(&FPUmathTablesRunStart, &FPUmathTablesLoadStart,
               (size_t)&FPUmathTablesLoadSize);

        //
        // 2. Flash API Initialization
        //
        FlashAPI_init();

        //
        // 3. Interrupt Infrastructure
        //
        Interrupt_initModule();
        Interrupt_initVectorTable();
        //THD
        C2000Ware_libraries_init();

        // ---- Initialize THD Analyzer ----
        THD_init(&g_analyzer,
                 RFFTin1Buff,
                 RFFToutBuff,
                 RFFTmagBuff,
                 RFFTphBuff,
                 RFFTTwiddleCoef,
                 WindowCoef,
                 (float32_t*)AdcRawBuffers,
                 THD_NUM_CHANNELS,
                 THD_FFT_SIZE,
                 THD_FFT_STAGES);

        // ---- Start ADC Acquisition ----
        THD_ADC_init();
        //THD_ADC_initEPWM();
        //THD_ADC_start();

        //
        //
        // 4. Metrology Initialization
        //
        metrology_main();
        PhaseDetection_init();

        //
        // 5. Register DMA Interrupts (before peripheral config)
        //
        Interrupt_register(DMA_INT_R, &dmaCh1ISR);   // R phase
        Interrupt_register(DMA_INT_Y, &dmaCh2ISR);   // Y phase
        Interrupt_register(DMA_INT_B, &dmaCh3ISR);   // B phase
        Interrupt_enable(DMA_INT_R);
        Interrupt_enable(DMA_INT_Y);
        Interrupt_enable(DMA_INT_B);

        //
        // 6. ePWM, ADC/DMA, DAC Configuration
        //
        SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

        EPWM_init();
        initEPWM();
        initADC_DMA();
        initDAC();

        SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

        //
        // 7. Initialize ADC Buffers
        //
        uint16_t i;
        for(i = 0; i < RESULTS_BUFFER_SIZE; i++)
        {
            adcBufferR[i] = 0;
            adcBufferY[i] = 0;
            adcBufferB[i] = 0;
        }

        //
        // 8. I2C and Runtime Timing
        //
       // i2c_main();
        initRuntimeTiming();
        startRuntimeTiming();

        // Time-slicing removed — all work runs sequentially in while loop

        //
        // 9. Flash Data Management
        //
        findReadAndWriteAddress();
        initializePDUManager();

        if (newestData == 0)
        {
            /* No valid data in flash — write factory defaults */
            writeAddress = BANK4_START;
            writeDefaultCalibrationValues();
            Example_ProgramUsingAutoECC();

            if (!updateFailed)
            {
                syncReadWriteToWorking();
            }

            readAddress = BANK4_START;
        }
        else
        {
            /* Load existing calibration from flash */
            memcpy(&pduManager.readWriteData, (void*)readAddress, sizeof(CalibrationData));

            /* Validate flash CRC (old firmware wrote without CRC — tolerate failure) */
            if (!validateFlashCRC(&pduManager.readWriteData))
            {
                /* CRC mismatch — recompute CRC for the loaded data so working
                 * copy has a valid CRC going forward.  Individual gain values
                 * are clamped by validateCalibrationGains() below. */
                computeFlashCRC(&pduManager.readWriteData);
            }

            validateCalibrationGains();
            syncReadWriteToWorking();
        }

        //
        // 10. MCAN (CAN FD) Configuration
        //
        SysCtl_setMCANClk(SYSCTL_MCANA, SYSCTL_MCANCLK_DIV_5);
        SysCtl_setMCANClk(SYSCTL_MCANB, SYSCTL_MCANCLK_DIV_5);

        MCANIntrConfig();

        // CANA GPIO
        GPIO_setPinConfig(GPIO_0_MCANA_RX);
        GPIO_setPinConfig(GPIO_1_MCANA_TX);
        // CANB GPIO
        GPIO_setPinConfig(GPIO_3_MCANB_RX);
        GPIO_setPinConfig(GPIO_2_MCANB_TX);

        MCANAConfig();
        MCANBConfig();

        timer_system_init();

        //
        // CPU Timer 0: 1-second tick for software timer system
        // timerSystemTick() drives ALL CAN-related software timers:
        //   - TIMER_10_SEC (discovery timeout)
        //   - TIMER_FRAME_TRACKING (2s retransmission)
        //   - TIMER_RETRY (2s retry)
        //   - TIMER_BU_DATA_COLLECTION (10s BU data timeout)
        //
        CPUTimer_stopTimer(CPUTIMER0_BASE);
        CPUTimer_selectClockSource(CPUTIMER0_BASE,
                                   CPUTIMER_CLOCK_SOURCE_SYS,
                                   CPUTIMER_CLOCK_PRESCALER_1);
        CPUTimer_setEmulationMode(CPUTIMER0_BASE,
                                  CPUTIMER_EMULATIONMODE_STOPAFTERNEXTDECREMENT);
        CPUTimer_setPeriod(CPUTIMER0_BASE, 150000000UL);  // 1 sec at 150MHz
        CPUTimer_setPreScaler(CPUTIMER0_BASE, 0);
        CPUTimer_clearOverflowFlag(CPUTIMER0_BASE);
        CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
        CPUTimer_enableInterrupt(CPUTIMER0_BASE);
        Interrupt_register(INT_TIMER0, &INT_Cputimer_ISR);
        Interrupt_enable(INT_TIMER0);

        MCAN_enableIntr(MCANA_DRIVER_BASE, MCAN_INTR_MASK_ALL, 1U);
        MCAN_enableIntr(MCANB_DRIVER_BASE, MCAN_INTR_MASK_ALL, 1U);
        MCAN_selectIntrLine(MCANA_DRIVER_BASE, MCAN_INTR_MASK_ALL, MCAN_INTR_LINE_NUM_1);
        MCAN_selectIntrLine(MCANB_DRIVER_BASE, MCAN_INTR_MASK_ALL, MCAN_INTR_LINE_NUM_1);
        MCAN_enableIntrLine(MCANA_DRIVER_BASE, MCAN_INTR_LINE_NUM_1, 1U);
        MCAN_enableIntrLine(MCANB_DRIVER_BASE, MCAN_INTR_LINE_NUM_1, 1U);

     //  sendCalibrationDataFormatVersion();

        //
        // 11. Application-Level Initialization
        //
        initBUTracking();
        memset(buMessagePending, 0, sizeof(buMessagePending));
        buMessageReceived = false;

        initRuntimeVoltageTransmission();
        initRuntimeDataCollection();
        initSBoardRuntimeToMBoard();

        //
        // 12. Enable Global Interrupts and Start
        //

        //I2c
        //
        // Check bus idle state before bringing up I2C
        // Expected: 0x03 (both SDA and SCL high)
        //
        uint16_t busState = I2C_Scanner_checkBusIdle();
        (void)busState;  // inspect in debugger
        //
        // Initialize the scanner (pin mux, ISR, I2C peripheral)
        //
        I2C_Scanner_init();

        EINT;
        ERTM;
        DEVICE_DELAY_US(STARTUP_DELAY_US);

        // Start CPU Timer 0 (software timer system - 1 sec tick)
        CPUTimer_startTimer(CPUTIMER0_BASE);
}

/**
 * @brief Main application entry point vrms
 */
void main(void)
 {

    // Initialize the system
  startup();
    counter2++;

   metrology_main();
   //adc_init(); // retryCounter  ClearRxFIFOBuffer();
   counter3++;

    /* =====================================================================
     * MAIN LOOP — Sequential Architecture
     * =====================================================================
     * ALWAYS active (regardless of calibration state):
     *  1. Process BU board messages (discovery responses)
     *  2. Process CAN RX FIFO
     *  3. Check discovery completion
     *  4. Send calibration data to M-Board
     *  5. Handle M-Board commands
     *  6. GPIO29 heartbeat
     *
     * RUNTIME-GATED (require runtimeActive && !calibrationInProgress):
     *  7. Process BU runtime messages
     *  8. Handle retransmission requests
     *  9. Check BU data collection completion
     * 10. Metrology calculations
     * 11. Voltage capture + DAC playback + send to BU boards
     * 12. Reset frame tracking
     * 13. Send runtime parameters to M-Board
     *
     * CALIBRATION PHASE (entire phase controlled by M-Board):
     *  CMD_START_DISCOVERY   → calibrationInProgress=true, runtimeActive=false
     *  CMD_FLASH_ENABLE      → (stays in calibration)
     *  CMD_ERASE_BANK4       → (stays in calibration)
     *  CMD_WRITE_DEFAULT     → flash write (stays in calibration)
     *  CMD_STOP_CALIBRATION  → syncReadWriteToWorking → copyFlashToBUBoards →
     *                          calibrationInProgress=false, runtimeActive=true
     * ===================================================================== */
    //I2C
   //
   // Initial scan
   //
   uint16_t numDevices = 0;
   const I2C_ScanResult *results = NULL;

   numDevices = I2C_Scanner_scanBus(TCA9555_ADDR_START, TCA9555_ADDR_END,
                                        TCA9555_REG_OUTPUT0);
   results = I2C_Scanner_getResults();

   while (1)
    {
        z++;

        /* --- Process BU board messages (discovery responses — always active) --- */
        if (buMessageReceived)
        {
            processBUMessages();
            buMessageReceived = false;
        }

        /* --- Process CAN RX FIFO buffer when full (always active) --- */
        if (bufferFull)
        {
            copyRxFIFO();
        }

        /* --- Handle retransmission requests (active during both calibration and runtime) --- */
        if (requestRetransmission)
        {
            handleMissingFrames();
            requestRetransmission = false;
            stopSoftwareTimer(TIMER_FRAME_TRACKING);
        }

        /* === Runtime-gated activities (skip during calibration) === */
        if (!calibrationInProgress && runtimeActive)
        {
            /* --- Process incoming BU runtime messages (CAN RX) --- */
            if (runtimeMessageReceived)
            {
                processBURuntimeMessages();
                runtimeMessageReceived = false;
            }

            /* --- Check BU data collection completion --- */
            if (activeBUMask != 0) {
                if (!buDataCollectionComplete &&
                    receivedCurrentFrames == activeBUMask &&
                    receivedKWFrames == activeBUMask) {
                    buDataCollectionComplete = true;
                    if (!buDataProcessed) {
                        sendToMBoard = true;
                        sendToBUBoard = false;
                        syncBUBoardsData();
                    }
                }
            }
        }

        /* --- Check for discovery completion --- */
        if (discoveryInProgress && timer10SecExpired)
        {
            finalizeBUDiscovery();
            if (activeBUMask != 0) {
                copyFlashToBUBoards();
                /* Reset BU data tracking — wait for BU boards to respond with their gains */
                receivedCurrentFrames = 0;
                receivedKWFrames = 0;
                buDataCollectionComplete = false;
                buDataProcessed = false;
                sendToMBoard = false;
            }
            else
            {
                /* No BU boards found — nothing to collect */
                buDataCollectionComplete = true;
                buDataProcessed = true;
                sendToMBoard = true;
            }
            discoveryComplete = true;
            /* runtimeActive stays false — M-Board must send CMD_STOP_CALIBRATION to resume */
        }

        /* --- Check if BU boards have responded with calibration data --- */
        if (discoveryComplete && !buDataCollectionComplete && activeBUMask != 0)
        {
            if (receivedCurrentFrames == activeBUMask &&
                receivedKWFrames == activeBUMask)
            {
                buDataCollectionComplete = true;
                syncBUBoardsData();
                buDataProcessed = true;
                sendToMBoard = true;
            }
        }

        /* --- Send calibration data to M-Board when ready --- */
        if (discoveryComplete && buDataCollectionComplete && buDataProcessed && sendToMBoard)
        {
            sendCalibrationDataToMBoard();
        }

        /* --- Handle commands from M-Board --- */
        if (CommandReceived)
        {
            switch (rxMsg[3].data[0])
            {
                case CMD_START_DISCOVERY:
                    calibrationInProgress = true;
                    runtimeActive = false;
                    resetCANModule();
                    ClearRxFIFOBuffer();
                    resetFrameTracking();
                    sendDiscoveryStartAck();
                    startNewDiscovery();
                    break;

                case CMD_READ:
                    findReadAndWriteAddress();
                    prepareFramesForTransmission((uint16 *)(readAddress), calibrationdDataSize+1);
                    handleRetransmissionRequest(&rxMsg[3]);
                    break;

                case CMD_VERSION_CHECK:
                    sendCalibrationDataFormatVersion();
                    break;

                case CMD_FLASH_ENABLE:
                    calibrationInProgress = true;
                    runtimeActive = false;
                    enableFlashWrite();
                    break;

                case CMD_ERASE_BANK4:
                    calibrationInProgress = true;
                    runtimeActive = false;
                    relayCommandToBUBoards(&rxMsg[3]);
                    eraseBank4();
                    erasedBank4Ack();
                    break;

                case CMD_WRITE_DEFAULT:
                    relayCommandToBUBoards(&rxMsg[3]);
                    findReadAndWriteAddress();
                    eraseSectorOrFindEmptySector();
                    writeDefaultCalibrationValues();
                    Example_ProgramUsingAutoECC();
                    writtenDefaultCalibrationValuesAck();
                    /* Stay in calibration — M-Board must send CMD_STOP_CALIBRATION */
                    break;

                case CMD_STOP_CALIBRATION:
                    /* End of calibration phase:
                     * 1. Sync new gains from flash to working data
                     * 2. Push updated gains to all active BU boards
                     * 3. Send stop calibration to BU boards (ID 4) so they reset too
                     * 4. ACK to M-Board
                     * 5. Reset MCU — boots fresh with latest gains into runtime */
                    if (!updateFailed)
                    {
                        syncReadWriteToWorking();
                        if (activeBUMask != 0)
                        {
                            copyFlashToBUBoards();
                        }
                    }
                    sendStopCalibrationToBUBoards();
                    sendStopCalibrationAck();
                    DEVICE_DELAY_US(10000);  /* 10ms for ACK/BU msg to transmit before reset */
                    SysCtl_resetDevice();
                    /* MCU resets here — boots into startup() → runtimeActive=true */
                    break;

                case 0xFD:
                    processSBoardRuntimeToMBoard();
                    break;

                default:
                    break;
            }

            CommandReceived = false;
        }

        /* =============================================================
         * RUNTIME ACTIVITIES (gated by runtimeActive, blocked during calibration)
         * ============================================================= */
        if (!calibrationInProgress && runtimeActive)
        {
            /* --- Metrology parameter calculation --- */
            App_calculateMetrologyParameters(&gAppHandle, &gMetrologyWorkingData);

            /* --- Phase detection: voltage lack, unbalance, phase loss --- */
            PhaseDetection_runAll();

            /* =============================================================
             * VOLTAGE CAPTURE & SEND TO BU BOARDS
             * ============================================================= */

            /* -- Capture Phase: DMA collects ADC samples -- */
            done = 0;
            dmaCompleteR = 0;
            dmaCompleteY = 0;
            dmaCompleteB = 0;
            isPlaybackMode = 0;

            /* Clear all pending interrupt flags */
            DMA_clearTriggerFlag(DMA_CH_R_BASE);
            DMA_clearTriggerFlag(DMA_CH_Y_BASE);
            DMA_clearTriggerFlag(DMA_CH_B_BASE);
            ADC_clearInterruptStatus(ADC_MODULE_BASE, ADC_INT_PLAYBACK);
            ADC_clearInterruptStatus(ADC_MODULE_BASE, ADC_INT_R);
            ADC_clearInterruptStatus(ADC_MODULE_BASE, ADC_INT_Y);
            ADC_clearInterruptStatus(ADC_MODULE_BASE, ADC_INT_B);
            ADC_clearInterruptOverflowStatus(ADC_MODULE_BASE, ADC_INT_PLAYBACK);
            ADC_clearInterruptOverflowStatus(ADC_MODULE_BASE, ADC_INT_R);
            ADC_clearInterruptOverflowStatus(ADC_MODULE_BASE, ADC_INT_Y);
            ADC_clearInterruptOverflowStatus(ADC_MODULE_BASE, ADC_INT_B);
            EPWM_clearADCTriggerFlag(EPWM_TRIGGER_BASE, EPWM_SOC_TRIGGER);
            EPWM_forceADCTriggerEventCountInit(EPWM_TRIGGER_BASE, EPWM_SOC_TRIGGER);

            /* Reset DMA addresses */
            DMA_configAddresses(DMA_CH_R_BASE, (uint16_t *)adcBufferR, (uint16_t *)ADC_RESULT_R);
            DMA_configAddresses(DMA_CH_Y_BASE, (uint16_t *)adcBufferY, (uint16_t *)ADC_RESULT_Y);
            DMA_configAddresses(DMA_CH_B_BASE, (uint16_t *)adcBufferB, (uint16_t *)ADC_RESULT_B);

            /* Start DMA channels */
            DMA_startChannel(DMA_CH_R_BASE);
            DMA_startChannel(DMA_CH_Y_BASE);
            DMA_startChannel(DMA_CH_B_BASE);

            /* Start ePWM to trigger ADC */
            EPWM_setTimeBaseCounter(EPWM_TRIGGER_BASE, 0);
            EPWM_enableADCTrigger(EPWM_TRIGGER_BASE, EPWM_SOC_TRIGGER);
            EPWM_setTimeBaseCounterMode(EPWM_TRIGGER_BASE, EPWM_COUNTER_MODE_UP);

            /* Stop ePWM after capture */
            EPWM_disableADCTrigger(EPWM_TRIGGER_BASE, EPWM_SOC_TRIGGER);
            EPWM_setTimeBaseCounterMode(EPWM_TRIGGER_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);

            /* -- Playback Phase: Output to DAC -- */
            done = 0;
            playbackIndex = 0;
            isPlaybackMode = 1;

            ADC_clearInterruptStatus(ADC_MODULE_BASE, ADC_INT_PLAYBACK);
            ADC_clearInterruptOverflowStatus(ADC_MODULE_BASE, ADC_INT_PLAYBACK);
            EPWM_clearADCTriggerFlag(EPWM_TRIGGER_BASE, EPWM_SOC_TRIGGER);
            EPWM_forceADCTriggerEventCountInit(EPWM_TRIGGER_BASE, EPWM_SOC_TRIGGER);

            EPWM_setTimeBaseCounter(EPWM_TRIGGER_BASE, 0);
            EPWM_enableADCTrigger(EPWM_TRIGGER_BASE, EPWM_SOC_TRIGGER);
            EPWM_setTimeBaseCounterMode(EPWM_TRIGGER_BASE, EPWM_COUNTER_MODE_UP);

            while(done == 0)
            {
                /* Wait for DAC playback to complete */
            }

            EPWM_disableADCTrigger(EPWM_TRIGGER_BASE, EPWM_SOC_TRIGGER);
            EPWM_setTimeBaseCounterMode(EPWM_TRIGGER_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);

            /* -- Compute crest factors from captured DMA buffers -- */
            PhaseDetection_computeCrestFactors();

            /* -- Send 27 voltage frames (3 phases x 9 frames) to BU boards -- */
            sendAllPhaseVoltages();

            /* =============================================================
             * RESET FRAME TRACKING FOR BU BOARD RX
             * ============================================================= */
            resetRuntimeFrameTracking();

            //THD — process BEFORE populate so results are current when transmitted
            if(THD_ADC_isBufferReady())
            {
                int ch;
                for(ch = 0; ch < THD_NUM_CHANNELS; ch++)
                {
                    THD_loadChannelData(&g_analyzer, ch,
                                        THD_ADC_getChannelBuffer(ch),
                                        THD_FFT_SIZE);
                }
                THD_processAllChannels(&g_analyzer);
                THD_ADC_resetAndRearm();
            }

            /* =============================================================
             * SEND PARAMETERS TO M BOARD
             * ============================================================= */
            populateInputParameters();
            populateOutputParameters();
            processSBoardRuntimeToMBoard();

            //I2C
            numDevices = I2C_Scanner_scanBus(TCA9555_ADDR_START, TCA9555_ADDR_END,
                                                     TCA9555_REG_OUTPUT0);
            results = I2C_Scanner_getResults();

        } /* end runtime-gated block */

        /* --- GPIO29 heartbeat toggle (always active) --- */
        GPIO_togglePin(29);
    }
}
