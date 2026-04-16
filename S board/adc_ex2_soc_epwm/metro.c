
#include "metro.h"
#include "metrology_calibration_defaults.h"
//#include "runtime.h"
//#include "runtimedata.h"
//
#include "can_driver.h"
#include <math.h>

// =============================================================================
// DEBUG: Sine wave generation for voltage testing
// =============================================================================
#define DEBUG_USE_SINE_WAVE     0       // Set to 0 to use actual ADC data

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Amplitude of debug sine wave (adjustable at runtime)
int DEBUG_SINE_AMPLITUDE = 200;

// Continuous phase counter - runs forever like real AC, never resets with buffer
static volatile float debugVoltSinePhase = 0.0f;

// Use 159 samples/cycle to create visible "drift" between buffers (~50Hz)
#define DEBUG_VOLT_SINE_PHASE_INCREMENT  (2.0f * M_PI / 159.0f)
// =============================================================================

metrologyData gMetrologyWorkingData;

int sd;
/* ---------------------------------------------------------VARIABLES------------------------------------------------------
 */
/*! @brief Defines metrology working data*/
// metrologyData gMetrologyWorkingData;
/*! @brief Defines TIDA handle instance*/
App_instance gAppHandle;
/*! @brief Stores phase log status  */
volatile uint8_t phaseLog = 0;
// calibrations.c below
//  __attribute__((section(".memsector01"))) struct infoMem nvParams;
struct infoMem nvParams;

calibrationData updatedData;
#define calInfo (&nvParams.seg_a.s.calData)
uint16_t cpuTimer0IntCount = 0;
uint16_t windowCounter = 1;
void generatePulse();
//
// cpuTimer0ISR - Counter for CpuTimer0
//

//

//
// Globals
//
int as=0;

signed int raw_value_ADC[6];  // Buffer for results
uint16_t index, set = 0;      // Index into result buffer
volatile uint16_t bufferfull; // Flag to indicate buffer is full
signed int ADCResults[RESULTS_BUFFER_SIZE],ADCResults_Ground;
signed long adcAResults_mul[16];
unsigned long dwSumPhaseVolt[16], dwMathWordBuff, dwMathWordBuff1;
unsigned int voltage_gain = 0, gain = 4096, wMathWordBuff;
unsigned int FanVolt[6], vgain = 210, ADC_SIGNAL, b;
unsigned int count = 0;

 unsigned int oneSecondCounter=0;
  unsigned int counter8000=0;

unsigned int counter200Ms=0;
//
// Function Prototypes
//
void calculateRMS(void);
void initEPWMMetro(void);
void metrology_main(void);
// Main
uint32_t bufferNumber = 0;
int bufferCount10;

int syncCount2=0;

int buffercheck[160];

int currentBufferIndex = 0;
int IsampleCounter = 0;

int IrPhaseBuffer[200] = {0};

int IyPhaseBuffer[200] = {0};

int IbPhaseBuffer[200] = {0};

static uint16_t sampleCounter = 0;
static uint16_t voltageBufferIndex = 0;

void sendCANMsg() {
  MCAN_TxBufElement TxMsg;
  memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
  TxMsg.id = ((uint32_t)(0x100)) << 18U;
  TxMsg.brs = 0x1;
  TxMsg.dlc = 15;
  TxMsg.rtr = 0;
  TxMsg.xtd = 0;
  TxMsg.fdf = 0x1;
  TxMsg.esi = 0U;
  TxMsg.efc = 1U;
  TxMsg.mm = 0xAAU;

  uint8_t datare[64] = {0x100};
  int i;
  for (i = 0; i < 64; i++) {
    TxMsg.data[i] = datare[i];
  }

  MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
  MCAN_txBufAddReq(CAN_BU_BASE, 0U);

  canTxWaitComplete(CAN_BU_BASE);
}
//
void metrology_main(void) {
  //


  Board_init();
  // Set up the ePWM
  //initEPWMMetro();
  //initEPWMMetro();
  App_init(&gAppHandle, &gMetrologyWorkingData);
  debugInit();
  cpuTimer0IntCount = 0;
  Metrology_init(&gMetrologyWorkingData);

  App_startDataCollection(&gAppHandle);
  // CPUTimer_startTimer(CPUTIMER1_BASE);

  //
  // Initialize results buffer
  //
  for (index = 0; index < RESULTS_BUFFER_SIZE; index++) {
    ADCResults[index] = 0;
  }

  index = 0;
  bufferfull = 0;

  //
  // Enable Global Interrupt (INTM) and realtime interrupt (DBGM)
  //

  CPUTimer_startTimer(CPUTIMER1_BASE); // cpu timer to run

  //
  // Loop indefinitely
  //
}

//
// Function to configure ePWM1 to generate the SOC.
//
void initEPWMMetro(void) {
  //
  // Disable SOCA
  //
  EPWM_disableADCTrigger(EPWM1_BASE, EPWM_SOC_A);

  //
  // Configure the SOC to occur on the first up-count event
  //
  EPWM_setADCTriggerSource(EPWM1_BASE, EPWM_SOC_A, EPWM_SOC_TBCTR_U_CMPA);
  EPWM_setADCTriggerEventPrescale(EPWM1_BASE, EPWM_SOC_A, 1);

  //
  // Set the compare A value to 1000 and the period to 1999
  // Assuming ePWM clock is 100MHz, this would give 50kHz sampling
  // 50MHz ePWM clock would give 25kHz sampling, etc.
  // The sample rate can also be modulated by changing the ePWM period
  // directly (ensure that the compare A value is less than the period).
  //

  //    > 1/20m
  //    ans = 50
  //
  //    > 8000/50
  //    ans = 160
  //
  //    > 20m/160
  //    ans = 1.25e-4
  //
  //    > 125u
  //    ans = 1.25e-4
  //
  //    > 1/125u
  //    ans = 8000
  //
  //    > 150M/(2*8000)
  //    ans = 9375

  EPWM_setTimeBasePeriod(EPWM1_BASE, 18750);
  EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, 9375);

  //
  // Set the local ePWM module clock divider to /1
  //
  EPWM_setClockPrescaler(EPWM1_BASE, EPWM_CLOCK_DIVIDER_1,
                         EPWM_HSCLOCK_DIVIDER_1);

  //
  // Start ePWM1, enabling SOCA and putting the counter in up-count mode
  //
  EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
  EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_UP);
}

//
// adcA1ISR - ADC A Interrupt 1 ISR
//


void generatePulse() {
    GPIO_togglePin(29);
}


// Metrology//
void App_init(App_instance *AppHandle, metrologyData *workingData) {
  // adsHandle->ready            = HAL_GPIO_IN_00;
  // adsHandle->sync             = HAL_GPIO_OUT_00;
  // adsHandle->spiChan          = HAL_SPI_CHAN_0;
  // adsHandle->spiCs            = HAL_SPI_CS_0;
  // adsHandle->crcEnable        = ADS_CRC_ENABLE;
  // adsHandle->crcType          = ADS_CRC_TYPE_CCITT;

  // gDLT645.uartChan            = HAL_UART_CHAN_0;

  workingData->activePulse = TRUE;
  workingData->reactivePulse = TRUE;

  AppHandle->intr0Enable =
      PHASE_ONE_ACTIVE_POWER_NEGATIVE | PHASE_TWO_ACTIVE_POWER_NEGATIVE |
      PHASE_THREE_ACTIVE_POWER_NEGATIVE | TOTAL_ACTIVE_POWER_NEGATIVE |
      PHASE_ONE_REACTIVE_POWER_NEGATIVE | PHASE_TWO_REACTIVE_POWER_NEGATIVE |
      PHASE_THREE_REACTIVE_POWER_NEGATIVE | TOTAL_REACTIVE_POWER_NEGATIVE |
      PHASE_ONE_CYCLE_COUNT_DONE | PHASE_TWO_CYCLE_COUNT_DONE |
      PHASE_THERE_CYCLE_COUNT_DONE;
  //
  AppHandle->intr1Enable = ZERO_CROSSING_PHASE_ONE | ZERO_CROSSING_PHASE_TWO |
                           ZERO_CROSSING_PHASE_THREE | SAG_EVENT_PHASE_ONE |
                           SAG_EVENT_PHASE_TWO | SAG_EVENT_PHASE_THREE;
  //
  AppHandle->intr0Status = 0x0;
  AppHandle->intr1Status = 0x0;
  //    // AppHandle->sync_reset     = HAL_GPIO_OUT_00;
  AppHandle->adcStatus = ADC_INIT;

  // DLT645_UARTRxdmaInit(dlt645);
}

void debugInit(void) {
  int i;
  // #ifdef DEBUG
  //     for(i = PHASE_ONE; i < MAX_PHASES; i++)
  //     {
  //         voltageIndex[i] = VOLTAGE_START_INDEX_DS + (i * PHASE_INCREMENT);
  //         currentIndex[i] = CURRENT_START_INDEX_DS + (i * PHASE_INCREMENT);
  //     }
  //     neutralIndex = CURRENT_START_INDEX_DS;
  //
  //     for( i= 0; i < SAMPLES_PER_CYCLE; i++)
  //     {
  //         float t = (((float)i / (float)SAMPLES_PER_CYCLE) * (float)TWO_PI);
  //
  //         voltage[i] = (((V_RMS * SQRT2)/V_SCALING) * sinf(t)) + ((3 *
  //         SQRT2)/V_SCALING * sinf(4*t)); current[i] = (((I_RMS *
  //         SQRT2)/I_SCALING) * sinf(t)) + ((4 * SQRT2)/I_SCALING * sinf(5*t));
  //     }
  // #endif
}

/*!
 * @brief App start data collection
 * @param[in] AppHandle   The App Instance
 */
void App_startDataCollection(App_instance *AppHandle) {
  // HAL_writeGPIOPin(AppHandle->sync_reset, HAL_GPIO_PIN_LOW);
  // HAL_delayMicroSeconds(1);
  // HAL_writeGPIOPin(AppHandle->sync_reset, HAL_GPIO_PIN_HIGH);

  AppHandle->adcStatus = ADC_READY;
}

void App_calculateMetrologyParameters(App_instance *AppHandle,
                                      metrologyData *workingData) {
  int ph;
  for (ph = 0; ph < 6; ph++) {
    phaseMetrology *phase = &workingData->phases[ph];

    if (phase->status & PHASE_STATUS_NEW_LOG) {
      phase->status &= ~PHASE_STATUS_NEW_LOG;
      Metrology_calculatePhaseReadings(workingData, ph);
      App_updateINT0(AppHandle, workingData->phases[ph].readings.activePower,
                     PHASE_ONE_ACTIVE_POWER_NEGATIVE << ph);
      App_updateINT0(AppHandle, workingData->phases[ph].readings.reactivePower,
                     PHASE_ONE_REACTIVE_POWER_NEGATIVE << ph);
      AppHandle->intr0Status |=
          ((PHASE_ONE_CYCLE_COUNT_DONE << ph) & AppHandle->intr0Enable);
      phaseLog |= PHASE_LOG_DONE << ph;
    } else {
      AppHandle->intr0Status &= ~(PHASE_ONE_CYCLE_COUNT_DONE << ph);
    }

    if (phase->cycleStatus & PHASE_STATUS_NEW_LOG) {
      phase->cycleStatus &= ~PHASE_STATUS_NEW_LOG;
      if (phase->params.cycleSkipCount < (INITIAL_ZERO_CROSSINGS_TO_SKIP - 1)) {
        phase->params.cycleSkipCount++;
      } else {
        Metrology_sagSwellDetection(workingData, ph);
        App_updateINT1(AppHandle, (uint16_t)phase->params.sagSwellState,
                       (SAG_EVENT_PHASE_ONE << ph));
      }
    } else {
      App_updateINT1(AppHandle,
                     (phase->cycleStatus & PHASE_STATUS_ZERO_CROSSING_MISSED),
                     (ZERO_CROSSING_PHASE_ONE << ph));
    }
  }
  //
  //    if(phaseLog == THREE_PHASE_LOG_DONE)
  //    {
  //       Metrology_calculateThreePhaseParameters(workingData);
  //       Metrology_calculateTotalParameters(workingData);
  //       App_updateINT0(AppHandle, workingData->totals.readings.activePower,
  //       TOTAL_ACTIVE_POWER_NEGATIVE); App_updateINT0(AppHandle,
  //       workingData->totals.readings.reactivePower,
  //       TOTAL_REACTIVE_POWER_NEGATIVE); phaseLog = 0;
  //    }

  //    if(workingData->neutral.status & PHASE_STATUS_NEW_LOG)
  //    {
  //       workingData->neutral.status &= ~PHASE_STATUS_NEW_LOG;
  //       Metrology_calculateNeutralReadings(workingData);
  //    }
}

/*!
 * @brief Update interrupt 0 register
 * @param[in] AppHandle  The App instance
 * @param[in] eventData   Event Data
 * @param[in] event       Event Type
 */
void App_updateINT0(App_instance *AppHandle, int32_t eventData, EVENTS0 event) {
  if (eventData < 0) {
    if (!(AppHandle->intr0Status & event)) {
      AppHandle->intr0Status |= (AppHandle->intr0Enable & event);
    }
  } else {
    if (AppHandle->intr0Status & event) {
      AppHandle->intr0Status &= ~event;
    }
  }
}

/*!
 * @brief Update interrupt 1 register
 * @param[in] AppHandle  The App instance
 * @param[in] eventData   Event Data
 * @param[in] event       Event Type
 */
void App_updateINT1(App_instance *AppHandle, uint16_t eventData,
                    EVENTS1 event) {
  if (eventData == PHASE_STATUS_ZERO_CROSSING_MISSED ||
      (int8_t)eventData == SAG_SWELL_VOLTAGE_SAG_CONTINUING) {
    if (!(AppHandle->intr1Status & event)) {
      AppHandle->intr1Status |= (AppHandle->intr1Enable & event);
    }
  } else {
    if (AppHandle->intr1Status & event) {
      AppHandle->intr1Status &= ~event;
    }
  }
}

// void get_debugData(metrologyData *workingData)
//{
//     workingData->rawVoltageData[0] = myADC0Results[0];
// }

void get_debugData(metrologyData *workingData) {
    //Output Voltage
      workingData->rawVoltageData[0] = (float)ADCResults[0];
      workingData->rawVoltageData[1] = (float)ADCResults[1];
      workingData->rawVoltageData[2] = (float)ADCResults[2];
    //Output Current
      workingData->rawCurrentData[0] = -1 * (float)ADCResults[3];
      workingData->rawCurrentData[1] = -1 * (float)ADCResults[4];
      workingData->rawCurrentData[2] = -1 * (float)ADCResults[5];
    //Input Voltage
      workingData->rawVoltageData[3] = (float)ADCResults[6];
      workingData->rawVoltageData[4] = (float)ADCResults[7];
      workingData->rawVoltageData[5] = (float)ADCResults[8];
    //Input Current
      workingData->rawCurrentData[3] = -1 * (float)ADCResults[9];
      workingData->rawCurrentData[4] = -1 * (float)ADCResults[10];
      workingData->rawCurrentData[5] = -1 * (float)ADCResults[11];
}

void Metrology_initializeCalibrationData() {
  memcpy(&updatedData, &nvParams.seg_a.s.calData,
         sizeof(nvParams.seg_a.s.calData));
  if (updatedData.calibrationInitFlag != CONFIG_INIT_FLAG) {
    Metrology_setDefaultCalibrationData();
  }
}

/*!
 * @brief Set default calibration data to flash
 */
void Metrology_setDefaultCalibrationData() {
  // HAL_clearMemoryBlock(startAddr);
  // HAL_copyMemoryBlock(&nvParams, &calibrationDefaults,
  // sizeof(calibrationDefaults), startAddr); HAL_secureMemoryBlock(startAddr);

  memcpy(&updatedData, &calibrationDefaults, sizeof(calibrationDefaults));
  memcpy(&nvParams.seg_a.s.calData, &updatedData, sizeof(updatedData));
}
