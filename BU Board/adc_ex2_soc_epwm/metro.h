
#ifndef METRO_H
#define METRO_H

#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "math.h"
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "c2000ware_libraries.h"
#include "gpio.h"
#include "dsp.h"
#include "fpu.h"
#include "metrology.h"
//#include "metrology_calibration_defaults.h"
#include "metrology_defines.h"




/* ---------------------------------------------------------DEFINES------------------------------------------------------ */
/*!
 * @brief Define when testing with lookup table
 *        Undefine when testing with actual input
 */
// #undef DEBUG
// #define DEBUG

/*! @brief Defines Number of cycles to skip initially   */
#define INITIAL_ZERO_CROSSINGS_TO_SKIP     10
/*! @brief Defines phase log complete status in three phases    */
#define THREE_PHASE_LOG_DONE    0x003F
/*! @brief Defines phase log complete status    */
#define PHASE_LOG_DONE          0x01
/*! @brief Defines ADC are in initialization state  */
#define ADC_INIT                0x00
/*! @brief Defines ADC are ready for data conversion  */
#define ADC_READY               0x01

#ifdef DEBUG
/*! @brief Defines square root of 2 */
#define SQRT2                (1.414213562373095f)
/*! @brief Defines voltage scale factor */
#define V_SCALING            (1585.2f)
/*! @brief Defines current scale factor */
#define I_SCALING            (125.0f)
/*! @brief Defines RMS voltage input */
#define V_RMS                (120.0f)
/*! @brief Defines RMS current input */
#define I_RMS                (10.0f)
/*! @brief Defines samples per cycle */
//#define SAMPLES_PER_CYCLE    (128)
/*! @brief Defines 2 * PI value */
#define TWO_PI               (2 * PI)
/*! @brief Defines voltage start index  */
#define VOLTAGE_START_INDEX_DS    (0)
/*! @brief Defines current start index  */
#define CURRENT_START_INDEX_DS    (0)
/*! @brief Defines phase increment  */
#define PHASE_INCREMENT           (53)
/*! @brief array to store the indexes of voltage    */
extern uint32_t voltageIndex[3];
/*! @brief array to store the indexes of current    */
extern uint32_t currentIndex[3] ;
/*! @brief Store the index of neutral current    */
extern uint32_t neutralIndex ;
/*! @brief Array to store voltage debug data    */

//extern float voltage[SAMPLES_PER_CYCLE];
/*! @brief Array to store current debug data    */

//extern float current[SAMPLES_PER_CYCLE];
#endif

extern uint16_t SET_VALUE;
/* ---------------------------------------------------------STRUCTS------------------------------------------------------ */
/*! @brief Defines App instance    */
typedef struct
{
    /*! @brief interrupt 0 Enable */
    uint32_t intr0Enable;
    /*! @brief interrupt 1 Enable */
    uint32_t intr1Enable;
    /*! @brief interrupt 0 status */
    uint32_t intr0Status;
    /*! @brief interrupt 1 status */
    uint32_t intr1Status;
    /*! @brief sync pin to all ADCs */
    bool sync_reset;
    /*! @brief Stores ADC status bit    */
    uint32_t adcStatus;
}App_instance;

extern int as;

extern metrologyData gMetrologyWorkingData;

/*! @enum EVENTS0  */
typedef enum
{
    /*! @brief Defines Phase 1 active power is negative */
    PHASE_ONE_ACTIVE_POWER_NEGATIVE     = 0x00000001,
    /*! @brief Defines Phase 2 active power is negative */
    PHASE_TWO_ACTIVE_POWER_NEGATIVE     = 0x00000002,
    /*! @brief Defines Phase 3 active power is negative */
    PHASE_THREE_ACTIVE_POWER_NEGATIVE   = 0x00000004,
    /*! @brief Defines total active power is negative */
    TOTAL_ACTIVE_POWER_NEGATIVE         = 0x00000008,
    /*! @brief Defines Phase 1 reactive power is negative */
    PHASE_ONE_REACTIVE_POWER_NEGATIVE   = 0x00000010,
    /*! @brief Defines Phase 2 reactive power is negative */
    PHASE_TWO_REACTIVE_POWER_NEGATIVE   = 0x00000020,
    /*! @brief Defines Phase 3 reactive power is negative */
    PHASE_THREE_REACTIVE_POWER_NEGATIVE = 0x00000040,
    /*! @brief Defines total reactive power is negative */
    TOTAL_REACTIVE_POWER_NEGATIVE       = 0x00000080,
    /*! @brief Defines phase 1 cycle count  is done */
    PHASE_ONE_CYCLE_COUNT_DONE          = 0x00000100,
    /*! @brief Defines phase 2 cycle count  is done */
    PHASE_TWO_CYCLE_COUNT_DONE          = 0x00000200,
    /*! @brief Defines phase 3 cycle count  is done */
    PHASE_THERE_CYCLE_COUNT_DONE        = 0x00000400,
}EVENTS0;

/*! @enum EVENTS1  */
typedef enum
{
    /*! @brief Defines zero crossing is missed in phase 1   */
    ZERO_CROSSING_PHASE_ONE    = 0x00000001,
    /*! @brief Defines zero crossing is missed in phase 2   */
    ZERO_CROSSING_PHASE_TWO    = 0x00000002,
    /*! @brief Defines zero crossing is missed in phase 3   */
    ZERO_CROSSING_PHASE_THREE  = 0x00000004,
    /*! @brief Defines sag event occured in phase 1   */
    SAG_EVENT_PHASE_ONE        = 0x00000008,
    /*! @brief Defines sag event occured in phase 2   */
    SAG_EVENT_PHASE_TWO        = 0x00000010,
    /*! @brief Defines sag event occured in phase 3   */
    SAG_EVENT_PHASE_THREE      = 0x00000020,
}EVENTS1;

#define sample                  1000
#define NUM_CHANNELS            18


/* ---------------------------------------------------------FUNCTIONS------------------------------------------------------ */
//TIDA_010243.c
void App_updateINT0(App_instance *AppHandle, int32_t eventData, EVENTS0 event);
void App_updateINT1(App_instance *AppHandle, uint16_t eventData, EVENTS1 event);
void App_init(App_instance *AppHandle, metrologyData *workingData);
void App_calculateMetrologyParameters(App_instance *AppHandle ,metrologyData *workingData);
void App_startDataCollection(App_instance *AppHandle);
void App_resetADC(App_instance * AppHandle);
void metrology_main(void);


//irq_handler.c
void debugInit(void);

void get_debugData(metrologyData *workingData_new);

//calibration.c
void Metrology_initializeCalibrationData();
void Metrology_setDefaultCalibrationData();
/* ---------------------------------------------------------VARIABLES------------------------------------------------------ */
/*! @brief Defines metrology working data*/
//metrologyData gMetrologyWorkingData;
/*! @brief Defines TIDA handle instance*/

extern App_instance   gAppHandle;
/*! @brief Stores phase log status  */
extern volatile uint8_t phaseLog;
//calibrations.c below
// __attribute__((section(".memsector01"))) struct infoMem nvParams;
extern struct infoMem nvParams;

extern calibrationData updatedData;
 #define calInfo (&nvParams.seg_a.s.calData)


extern uint16_t index,set;                              // Index into result buffer
extern volatile uint16_t bufferfull;                // Flag to indicate buffer is full

extern unsigned long dwSumPhaseVolt[NUM_CHANNELS],dwMathWordBuff,dwMathWordBuff1;
extern unsigned int voltage_gain,gain,wMathWordBuff;
extern unsigned int FanVolt[6],vgain,ADC_SIGNAL,b;
extern unsigned int count;
extern uint16_t sync200ms ;


 extern unsigned int oneSecondCounter;
 extern unsigned int counter8000;


#endif
