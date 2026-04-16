
#include "metro.h"
#include "metrology_calibration_defaults.h"
#include "src/power_config.h"



int k,COUNTER;



// Removed unused index and bufferFull variables

// Global accumulator for each channels sum of squares
unsigned long dwSumPhaseVolt[NUM_CHANNELS] = {0};

// Sample counter for RMS computation
unsigned int sampleCount = 0;

// Global variable for calibration factor (adjust as needed)
float calib = 5265;

//
// Global Declaration for Demand and other parameters (unchanged)
//
signed long buf[200], buf_v[200], buf_c[200];
uint8_t countSample;
unsigned int struct_size;
unsigned long dwwMathBuff;
unsigned int max_curr;
static int i;
unsigned int demand_chk_1sec, demand_chk_1hr, demand_chk_24hr;
unsigned int wDemandSumCntr[4];
unsigned long dw_CurrDemandSum[NUM_CHANNELS];

// Other global variables remain as originally defined
unsigned int adcAResult[NUM_CHANNELS];
unsigned int dww_RMS_Current;
unsigned int count = 0;

int root;
int pinstate;

int as;


int k;
/* ---------------------------------------------------------VARIABLES------------------------------------------------------ */
/*! @brief Defines metrology working data*/
//metrologyData gMetrologyWorkingData;
/*! @brief Defines TIDA handle instance*/
App_instance   gAppHandle;
/*! @brief Stores phase log status  */
volatile uint8_t phaseLog = 0;
//calibrations.c below
// __attribute__((section(".memsector01"))) struct infoMem nvParams;
struct infoMem nvParams;

calibrationData updatedData;
 #define calInfo (&nvParams.seg_a.s.calData)
uint16_t sync200ms = false;
//
// cpuTimer0ISR - Counter for CpuTimer0
//




//
unsigned int counter200Ms;

 unsigned int oneSecondCounter=0;
  unsigned int counter8000=0;

//
// Globals
//
uint16_t index,set=0;                              // Index into result buffer
volatile uint16_t bufferfull;                // Flag to indicate buffer is full
int16_t ADCResults[18];
unsigned long dwSumPhaseVolt[NUM_CHANNELS],dwMathWordBuff,dwMathWordBuff1;
unsigned int voltage_gain=0,gain=4096,wMathWordBuff;
unsigned int FanVolt[6],vgain=210,ADC_SIGNAL,b;
extern unsigned int count;
signed long ADC_Display;
volatile int16_t rawADC0;  // Debug: raw ADC value before subtraction
volatile int16_t offset = 2010;
volatile int16_t display = 0;
int32_t debugResult0;  // Debug: result after subtraction
//
// Function Prototypes
//
void calculateRMS(void);
void initEPWM_Metro();
// Main
//
metrologyData gMetrologyWorkingData;

void metrology_main(void)
{
    //
   


    // Set up the ePWM
    //initEPWM();

    App_init(&gAppHandle, &gMetrologyWorkingData);
    debugInit();
    Metrology_init(&gMetrologyWorkingData);

    App_startDataCollection(&gAppHandle);
    CPUTimer_startTimer(CPUTIMER1_BASE);

    //
    // Initialize results buffer
    //
    for(index = 0; index < RESULTS_BUFFER_SIZE; index++)
    {
        ADCResults[index] = 0;
    }

    index = 0;
    bufferfull = 0;

    //
    // Enable Global Interrupt (INTM) and realtime interrupt (DBGM)
    //
    
    CPUTimer_startTimer(CPUTIMER1_BASE);// cpu timer to run

    //
    // Loop indefinitely
    //
    
}



//Metrology//
void App_init(App_instance *AppHandle, metrologyData *workingData)
{
    // adsHandle->ready            = HAL_GPIO_IN_00;
    // adsHandle->sync             = HAL_GPIO_OUT_00;
    // adsHandle->spiChan          = HAL_SPI_CHAN_0;
    // adsHandle->spiCs            = HAL_SPI_CS_0;
    // adsHandle->crcEnable        = ADS_CRC_ENABLE;
    // adsHandle->crcType          = ADS_CRC_TYPE_CCITT;

    // gDLT645.uartChan            = HAL_UART_CHAN_0;

    AppHandle->intr0Enable    = PHASE_ONE_ACTIVE_POWER_NEGATIVE |
                                 PHASE_TWO_ACTIVE_POWER_NEGATIVE |
                                 PHASE_THREE_ACTIVE_POWER_NEGATIVE |
                                 TOTAL_ACTIVE_POWER_NEGATIVE      |
                                 PHASE_ONE_REACTIVE_POWER_NEGATIVE|
                                 PHASE_TWO_REACTIVE_POWER_NEGATIVE|
                                 PHASE_THREE_REACTIVE_POWER_NEGATIVE|
                                 TOTAL_REACTIVE_POWER_NEGATIVE|
                                 PHASE_ONE_CYCLE_COUNT_DONE|
                                 PHASE_TWO_CYCLE_COUNT_DONE|
                                 PHASE_THERE_CYCLE_COUNT_DONE;
//
    AppHandle->intr1Enable    = ZERO_CROSSING_PHASE_ONE  |
                                 ZERO_CROSSING_PHASE_TWO  |
                                 ZERO_CROSSING_PHASE_THREE|
                                 SAG_EVENT_PHASE_ONE      |
                                 SAG_EVENT_PHASE_TWO      |
                                 SAG_EVENT_PHASE_THREE;
//
    AppHandle->intr0Status    = 0x0;
    AppHandle->intr1Status    = 0x0;
//    // AppHandle->sync_reset     = HAL_GPIO_OUT_00;
    AppHandle->adcStatus      = ADC_INIT;

    // DLT645_UARTRxdmaInit(dlt645);
}


void debugInit(void)
{int i;
//#ifdef DEBUG
//    for(i = PHASE_ONE; i < MAX_PHASES; i++)
//    {
//        voltageIndex[i] = VOLTAGE_START_INDEX_DS + (i * PHASE_INCREMENT);
//        currentIndex[i] = CURRENT_START_INDEX_DS + (i * PHASE_INCREMENT);
//    }
//    neutralIndex = CURRENT_START_INDEX_DS;
//
//    for( i= 0; i < SAMPLES_PER_CYCLE; i++)
//    {
//        float t = (((float)i / (float)SAMPLES_PER_CYCLE) * (float)TWO_PI);
//
//        voltage[i] = (((V_RMS * SQRT2)/V_SCALING) * sinf(t)) + ((3 * SQRT2)/V_SCALING * sinf(4*t));
//        current[i] = (((I_RMS * SQRT2)/I_SCALING) * sinf(t)) + ((4 * SQRT2)/I_SCALING * sinf(5*t));
//    }
//#endif
}


/*!
 * @brief App start data collection
 * @param[in] AppHandle   The App Instance
 */
void App_startDataCollection(App_instance *AppHandle)
{
    // HAL_writeGPIOPin(AppHandle->sync_reset, HAL_GPIO_PIN_LOW);
    // HAL_delayMicroSeconds(1);
    // HAL_writeGPIOPin(AppHandle->sync_reset, HAL_GPIO_PIN_HIGH);

    AppHandle->adcStatus = ADC_READY;
}


void App_calculateMetrologyParameters(App_instance *AppHandle ,metrologyData *workingData)
{int ph;
COUNTER++;
    for(ph = 0; ph < 18; ph++)
    {
        phaseMetrology *phase = &workingData->phases[ph];

        if(phase->status & PHASE_STATUS_NEW_LOG)
        {
            phase->status &= ~PHASE_STATUS_NEW_LOG;
            Metrology_calculatePhaseReadings(workingData, ph);
            AppHandle->intr0Status |= ((PHASE_ONE_CYCLE_COUNT_DONE << ph) & AppHandle->intr0Enable);
            phaseLog |= PHASE_LOG_DONE << ph;
        }
        else
        {
            AppHandle->intr0Status &= ~(PHASE_ONE_CYCLE_COUNT_DONE << ph);
        }
    }
//
//    if(phaseLog == THREE_PHASE_LOG_DONE)
//    {
//       Metrology_calculateThreePhaseParameters(workingData);
//       Metrology_calculateTotalParameters(workingData);
//       App_updateINT0(AppHandle, workingData->totals.readings.activePower, TOTAL_ACTIVE_POWER_NEGATIVE);
//       App_updateINT0(AppHandle, workingData->totals.readings.reactivePower, TOTAL_REACTIVE_POWER_NEGATIVE);
//       phaseLog = 0;
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
void App_updateINT0(App_instance *AppHandle, int32_t eventData, EVENTS0 event)
{
    if(eventData < 0)
    {
        if(!(AppHandle->intr0Status & event))
        {
            AppHandle->intr0Status |= (AppHandle->intr0Enable & event);
        }
    }
    else
    {
        if(AppHandle->intr0Status & event)
        {
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
void App_updateINT1(App_instance *AppHandle, uint16_t eventData, EVENTS1 event)
{
    if(eventData == PHASE_STATUS_ZERO_CROSSING_MISSED || (int8_t)eventData == SAG_SWELL_VOLTAGE_SAG_CONTINUING)
    {
        if(!(AppHandle->intr1Status & event))
        {
            AppHandle->intr1Status |= (AppHandle->intr1Enable & event);
        }
    }
    else
    {
        if(AppHandle->intr1Status & event)
        {
            AppHandle->intr1Status &= ~event;
        }
    }
}


void get_debugData(metrologyData *workingData)
{
    workingData->rawCurrentData[0]      =  (float)ADCResults[0];
    workingData->rawCurrentData[1]      =  (float)ADCResults[1];
    workingData->rawCurrentData[2]      =  (float)ADCResults[2];
    workingData->rawCurrentData[3]      =  (float)ADCResults[3];
    workingData->rawCurrentData[4]      =  (float)ADCResults[4];
    workingData->rawCurrentData[5]      =  (float)ADCResults[5];
    workingData->rawCurrentData[6]      =  (float)ADCResults[6];
    workingData->rawCurrentData[7]      =  (float)ADCResults[7];
    workingData->rawCurrentData[8]      =  (float)ADCResults[8];
    workingData->rawCurrentData[9]      =  (float)ADCResults[9];
    workingData->rawCurrentData[10]      =  (float)ADCResults[10];
    workingData->rawCurrentData[11]      =  (float)ADCResults[11];
    workingData->rawCurrentData[12]      =  (float)ADCResults[12];
    workingData->rawCurrentData[13]      =  (float)ADCResults[13];
    workingData->rawCurrentData[14]      =  (float)ADCResults[14];
    workingData->rawCurrentData[15]      =  (float)ADCResults[15];
    workingData->rawCurrentData[16]      =  (float)ADCResults[16];
    workingData->rawCurrentData[17]      =  (float)ADCResults[17];
}

void Metrology_initializeCalibrationData()
{
    memcpy(&updatedData, &nvParams.seg_a.s.calData, sizeof(nvParams.seg_a.s.calData));
    if(updatedData.calibrationInitFlag != CONFIG_INIT_FLAG)
    {
        Metrology_setDefaultCalibrationData();
    }
}

/*!
 * @brief Set default calibration data to flash
 */
void Metrology_setDefaultCalibrationData()
{
    // HAL_clearMemoryBlock(startAddr);
    // HAL_copyMemoryBlock(&nvParams, &calibrationDefaults, sizeof(calibrationDefaults), startAddr);
    // HAL_secureMemoryBlock(startAddr);

    memcpy(&updatedData, &calibrationDefaults, sizeof(calibrationDefaults));
    memcpy(&nvParams.seg_a.s.calData, &updatedData, sizeof(updatedData));
}
