//
// DWW_FIRMWARE.h - Power Monitoring System Header File
//

#ifndef DWW_FIRMWARE_H
#define DWW_FIRMWARE_H

//
// Included Files
//
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "dww_parameter.h"
#include "runtime.h"


// Defines
//
#define PHASE_CURR            7
#define PHASE_VOLT            6
#define SAMPLE_COUNT_THRESHOLD  1000
#define Peak_Current          107
#define BUFFER_SIZE           10
#define VOLT_SIZE             200
#define CHANNEL_SIZE          6
#define TIMER_FREQ            150000000
#define LACKING_VOLT          25
#define PHASE_LACK            50
#define UNBALANCE_RATE        3
#define PHASE_LOSS            50

// System states
#define PRIMARY               0
#define SECONDARY             1

// //
// // Function Prototypes
// //
// void adc_init();
// void adc_start_process ();
// void initEPWM(void);
// void configureGPIO28(void);
// __interrupt void adcA1ISR(void);

// // Power monitoring functions
// void dww_Pri_Sec_Calculation(unsigned int reg);
// void dww_Max_Min_Calculation(unsigned int reg);
// void dww_Phase_Calculation(void);
// void dww_Power_Calculation(unsigned int reg);
// void dww_Frequency_Calculation(void);
// void dww_phase_sequence_error(void);
// void dww_volt_buff_creation(void);
// void copyVoltageBuffersToRuntimeStruct(void);

// //
// // Global Variables (External declarations)
// //
// extern int vari;
// extern int k;

// extern unsigned int Pri_Phase_RY;
// extern unsigned int Pri_Phase_YB;
// extern unsigned int Pri_Phase_BR;
// extern unsigned int Sec_Phase_RY;
// extern unsigned int Sec_Phase_YB;
// extern unsigned int Sec_Phase_BR;
// extern int Volt_Lack_flag;
// extern int Volt_Unbalance_flag;
// extern int Phase_loss_flag;
// extern int Phase_Seq_flag;

// // Buffer arrays
// extern signed int Output_Volt_R[VOLT_SIZE];
// extern signed int Output_Volt_Y[VOLT_SIZE];
// extern signed int Output_Volt_B[VOLT_SIZE];
// extern signed int Input_Volt_R[VOLT_SIZE];
// extern signed int Input_Volt_Y[VOLT_SIZE];
// extern signed int Input_Volt_B[VOLT_SIZE];
// extern unsigned int buff_cntr;

// // Phase sequence variables
// extern unsigned int lastsample[3];
// extern unsigned long zcsample[3];
// extern unsigned long sampleindex;
// extern unsigned int index;
// extern signed int volt_buff[BUFFER_SIZE];

// // Phase calculation variables
// extern float Vsum;
// extern float v_alpha;
// extern float v_beta;
// extern float last_sample_alpha;
// extern float adc_r, adc_y, adc_b;
// extern float V_alpha_t, V_beta_t;
// extern float V_alpha_next, V_beta_next;
// extern float cross_product;
// extern int reversal_detected;
// extern float theta_p;
// extern float del_theta_p;
// extern float theta_zp;

// // Frequency and phase measurement
// extern float frequency;
// extern volatile float lasttimestamp_R;
// extern volatile float currenttimestamp_R;
// extern signed int last_sample_R;
// extern float timediff;
// extern float timeperiod;
// extern signed int last_sample_Y, last_sample_B;
// extern volatile float lasttimestamp_Y, lasttimestamp_B;
// extern volatile float currenttimestamp_Y, currenttimestamp_B;
// extern int zcd_detected_R, zcd_detected_Y, zcd_detected_B;
// extern float offset_value;

// // ADC buffers and accumulators
// extern signed int wAdcCurrBuff[PHASE_CURR];
// extern signed int wAdcVoltBuff[PHASE_VOLT];
// extern unsigned long dwSumPhaseCurr[PHASE_CURR];
// extern unsigned long dwSumPhaseVolt[PHASE_VOLT];
// extern signed long long dwSumPhaseWatt[7];
// extern unsigned long kw_r, kw_y, kw_b;
// extern unsigned long kw_r_sec, kw_y_sec, kw_b_sec;

// // Calibration constants
// extern float wGain_Kw_pri;
// extern float wGain_Kw_sec;
// extern float calib_pri_R;
// extern float calib_pri_Y;
// extern float calib_pri_B;
// extern float calib_kw_pri;
// extern float calib_sec_R;
// extern float calib_sec_Y;
// extern float calib_sec_B;
// extern float calib_curr_pri_r;
// extern float calib_curr_pri_y;
// extern float calib_curr_pri_b;
// extern float calib_curr_sec_r;
// extern float calib_curr_sec_y;
// extern float calib_curr_sec_b;

// // Sample counter and parameter structures
// extern unsigned int sampleCount;
// extern DWW_PRIMARY_PARAMETER primary;
// extern DWW_SECONDARY_PARAMETER secondary;
// extern DWW_SYSTEM_PARAMETER system_para;
// extern DWW_THRESHOLD_UNION pri_Thd;
// extern DWW_THRESHOLD_UNION sec_Thd;

// // Additional global variables
// extern float dwMathBuff;
// extern unsigned int struct_size;
// extern unsigned long dwwMathBuff;
// extern unsigned int max_curr;
// extern unsigned int demand_chk_1sec, demand_chk_1hr, demand_chk_24hr;
// extern unsigned int wDemandSumCntr[4];
// extern unsigned long dw_CurrDemandSum[PHASE_CURR];
// extern dww_structflag *sFlag;
// extern unsigned int adcAResult[PHASE_CURR];
// extern unsigned int dww_RMS_Current;
// extern unsigned int Count;
// extern float current_time;
// extern float period;

#endif // DWW_FIRMWARE_H
