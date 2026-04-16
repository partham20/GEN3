//
////
//// Included Files
////
//#include "s_board_adc.h"
//// Defines
////
//
///*
// * Added for ADC check need to remove furthir
// */
//    int i,k;
//    float avgSquare, rms;
//    unsigned long curr_square;
//    unsigned long volt_square;
//
//    int vari;
//    int voltageBufferIndex = 0;
//
///*
// * Global Declaration for the Phase Calculation function
// */
//unsigned int Pri_Phase_RY;
//unsigned int Pri_Phase_YB;
//unsigned int Pri_Phase_BR;
//unsigned int Sec_Phase_RY;
//unsigned int Sec_Phase_YB;
//unsigned int Sec_Phase_BR;
//int Volt_Lack_flag = 0;
//int Volt_Unbalance_flag = 0;
//int Phase_loss_flag = 0;
//int Phase_Seq_flag = 0;
//
///*
// * Global declaration for the buffer creation
// */
//signed int Output_Volt_R[VOLT_SIZE];
//signed int Output_Volt_Y[VOLT_SIZE];
//signed int Output_Volt_B[VOLT_SIZE];
//signed int Input_Volt_R[VOLT_SIZE];
//signed int Input_Volt_Y[VOLT_SIZE];
//signed int Input_Volt_B[VOLT_SIZE];
//unsigned int buff_cntr = 0;
//
//
//
//
///*
// * Added for the phase sequence error 07/04/2025
// */
//
//unsigned int lastsample[3] = {0};
//unsigned long zcsample[3] = {0};
//unsigned long sampleindex = 0;
//
//
///*
// * Global declaration for the instantaneous voltage buffer creation
// */
////unsigned int index = 0;
//signed int volt_buff[BUFFER_SIZE];
//
//
///*
// * Global Declaration for the phase sequence error
// */
//float Vsum;
//float v_alpha;
//float v_beta;
//float last_sample_alpha;
//float adc_r, adc_y, adc_b;
//float V_alpha_t, V_beta_t;
//float V_alpha_next, V_beta_next;
//float cross_product;
//int reversal_detected = 0;
//
//
//float theta_p;
//float del_theta_p;
//float theta_zp = 0;
//
///*
// *Global Declaration for the frequency and Phase sequence error
// */
//float frequency = 0;
//volatile float lasttimestamp_R = 0;
//volatile float currenttimestamp_R = 0;
//signed int last_sample_R = 0;
//float timediff;
//float timeperiod;
//
///*
// * For testing purpose for the phase sequennce error in have added
// * this global declaration further we can remove this
// */
//signed int last_sample_Y = 0, last_sample_B = 0;
//volatile float lasttimestamp_Y = 0,lasttimestamp_B = 0;
//volatile float currenttimestamp_Y = 0, currenttimestamp_B = 0;
//int zcd_detected_R = 0, zcd_detected_Y = 0, zcd_detected_B = 0;
//
//
//
//float offset_value=2048;
//
//int a=0;
////
//// Globals
////
//signed int  wAdcCurrBuff[PHASE_CURR]; // Buffer for OP, IP and GND Current
//signed int  wAdcVoltBuff[PHASE_VOLT]; // Buffer for OP and IP Voltage
//
//// Global accumulator for each channel's sum of squares
//// unsigned long dwSumPhaseCurr[PHASE_CURR] = {0}; // For IP, OP, GND Current
//// // unsigned long dwSumPhaseVolt[PHASE_VOLT] = {0}; // For IP, OP Voltage
//// signed long long dwSumPhaseWatt[7] = {0};
//unsigned long kw_r, kw_y, kw_b; //primary
//unsigned long kw_r_sec, kw_y_sec, kw_b_sec; //secondary
//
//float wGain_Kw_pri = 3000.2f;
//float wGain_Kw_sec = 2100.2f;
//
//// Sample counter for RMS computation
//unsigned int sampleCount = 0;
//
///*
// * These are the gain values for the
// * Input Line to Line Voltage
// * Output Line to Neutral Voltage
// */
//
//float calib_pri_R = 4228.51f;
//float calib_pri_Y = 4228.51f;
//float calib_pri_B = 4228.51f;
//float calib_kw_pri = 1.0f;//This need to be changed accordingly
//
//float calib_sec_R = 2519.53f;
//float calib_sec_Y = 2519.53f;
//float calib_sec_B = 2519.53f;
//
//
//#if 0
//float calib_sec = 1992.0f; //This is the original value obtained from the calculation but the ADC is not working as expected
//float calib_curr = 6587.1f;
//#endif                     //But if i change the value to 1250.0 then it is working as expected. need to investigate on this
//
//float calib_curr_pri_r = 7400.1f;
//float calib_curr_pri_y = 7400.1f;
//float calib_curr_pri_b = 7400.1f;
//float calib_curr_sec_r = 7400.1f;
//float calib_curr_sec_y = 7400.1f;
//float calib_curr_sec_b = 7400.1f;
//
////
//// Global Declaration for Demand and other parameters (unchanged)
////
//DWW_PRIMARY_PARAMETER primary;
//DWW_SECONDARY_PARAMETER secondary;
//DWW_SYSTEM_PARAMETER system_para;
//
///*
// * Added for the Threshold Calculation
// */
//DWW_THRESHOLD_UNION pri_Thd;
//DWW_THRESHOLD_UNION sec_Thd;
//float dwMathBuff;
//unsigned int x; // Added for the Kw gain; can be eliminated if not required
//
//unsigned int struct_size;
//unsigned long dwwMathBuff;
//unsigned int max_curr;
//int i;
//unsigned int demand_chk_1sec, demand_chk_1hr, demand_chk_24hr;
//unsigned int wDemandSumCntr[4];
//unsigned long dw_CurrDemandSum[PHASE_CURR];
//dww_structflag *sFlag;
//
//// Other global variables remain as originally defined
//unsigned int adcAResult[PHASE_CURR];
//unsigned int dww_RMS_Current;
//unsigned int Count = 0;
//
//
///*
// * Added for the calculation for the frequency
// */
//float current_time;
//float period;
////
//
//void initEPWM(void);
//
////
//// Main
//
//signed long buf[200], buf_v[200], buf_c[200];
//uint8_t countSample;
//
//
//
///*
// * configured GPIO28 for the sync signal
// */
//void configureGPIO28(void)
//{
//    GPIO_setPadConfig(28, GPIO_PIN_TYPE_PULLUP );
//    GPIO_setPinConfig(GPIO_28_GPIO28);
//    GPIO_setDirectionMode(28, GPIO_DIR_MODE_OUT);
//    GPIO_setQualificationMode(28, GPIO_QUAL_SYNC);
//
//    GPIO_writePin(28, 1);  // Set the pin high (1)
//
//}
////
//// Function to configure ePWM1 to generate the SOC.
////
//// void initEPWM(void)
//// {
////     EPWM_disableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
////     EPWM_setADCTriggerSource(EPWM1_BASE, EPWM_SOC_A, EPWM_SOC_TBCTR_U_CMPA);
////     EPWM_setADCTriggerEventPrescale(EPWM1_BASE, EPWM_SOC_A, 1);
////     //EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, 2048);
////     //EPWM_setTimeBasePeriod(EPWM1_BASE, 4095);
////     EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, 9374);
////     EPWM_setTimeBasePeriod(EPWM1_BASE, 18750);
////     EPWM_setClockPrescaler(EPWM1_BASE, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
////     EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_UP_DOWN);
//
////     /*
////      * Added logic to calculate the frequency
////      */
////     EPWM_setPeriodLoadMode(EPWM1_BASE,EPWM_PERIOD_DIRECT_LOAD);
////     EPWM_setTimeBaseCounter(EPWM1_BASE, 0);
//
//// }
//
//
//// // Fixed adcA1ISR - ADC A Interrupt 1 ISR
//// __interrupt void adcA1ISR(void)
//// {
////     k++;
//
////     // Local variables for current ISR execution
////     int i;
////     float avgSquare, rms;
////     unsigned long curr_square;
////     unsigned long volt_square;
//
////     // Static variables to maintain state between ISR calls
////      // Index for 200-sample voltage buffer
////     static int sampleCounter = 0;       // Counter for tracking every 5th sample
//
////     /*
////      * ADC Conversion for the Input, Output and Ground Current
////      * Read all ADC values every interrupt (for 1000 sample RMS calculation)
////      */
////     wAdcCurrBuff[0] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0) - 2011); // OP Current R Phase
////     wAdcCurrBuff[1] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1) - 2011); // OP Current Y Phase
////     wAdcCurrBuff[2] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER2) - 2011); // OP Current B Phase
////     wAdcCurrBuff[3] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER3) - 2012); // IP Current R Phase
////     wAdcCurrBuff[4] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER4) - 2011); // IP Current Y Phase
////     wAdcCurrBuff[5] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER5) - 2011); // IP Current B Phase
////     wAdcCurrBuff[6] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER6) - 2011); // GND Current
//
////     /*
////      * ADC Conversion for the Input and Output Voltage
////      */
////     wAdcVoltBuff[0] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER7) - 2030);  // OP Voltage RN Phase
////     wAdcVoltBuff[1] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER8) - 2030);  // OP Voltage YN Phase
////     wAdcVoltBuff[2] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER9) - 2030);  // OP Voltage BN Phase
//
////     wAdcVoltBuff[3] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER10) - 2030); // IP Voltage RY Phase
////     wAdcVoltBuff[4] = (signed int)(ADC_readResult(ADCERESULT_BASE, ADC_SOC_NUMBER0) - 2030);  // IP Voltage YB Phase
////     wAdcVoltBuff[5] = (signed int)(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER11) - 2030); // IP Voltage BR Phase
//
////     /*
////      * ALWAYS accumulate for RMS calculation (all 1000 samples)
////      * Square and accumulate current values
////      */
////     for(i = 0; i < PHASE_CURR; i++)
////     {
////         curr_square = (unsigned long)wAdcCurrBuff[i] * (unsigned long)wAdcCurrBuff[i];
////         dwSumPhaseCurr[i] += curr_square;
////     }
//
////     /*
////      * Square and accumulate voltage values
////      */
////     for(i = 0; i < PHASE_VOLT; i++)
////     {
////         volt_square = (unsigned long)wAdcVoltBuff[i] * (unsigned long)wAdcVoltBuff[i];
////         dwSumPhaseVolt[i] += volt_square;
////     }
//
////     /*
////      * Power calculation - accumulate for all 1000 samples
////      * Primary per phase KW
////      */
////     kw_r = ((signed long)wAdcVoltBuff[3]) * ((signed long)wAdcCurrBuff[3]) * (wGain_Kw_pri/10000);
////     dwSumPhaseWatt[0] += kw_r;
//
////     kw_y = ((signed long)wAdcVoltBuff[4]) * ((signed long)wAdcCurrBuff[4]) * (wGain_Kw_pri/10000);
////     dwSumPhaseWatt[1] += kw_y;
//
////     kw_b = ((signed long)wAdcVoltBuff[5]) * ((signed long)wAdcCurrBuff[5]) * (wGain_Kw_pri/10000);
////     dwSumPhaseWatt[2] += kw_b;
//
////     /*
////      * Secondary per phase KW
////      */
////     kw_r_sec = ((signed long)wAdcVoltBuff[0]) * ((signed long)wAdcCurrBuff[0]) * (wGain_Kw_sec/10000);
////     dwSumPhaseWatt[3] += kw_r_sec;
//
////     kw_y_sec = ((signed long)wAdcVoltBuff[1]) * ((signed long)wAdcCurrBuff[1]) * (wGain_Kw_sec/10000);
////     dwSumPhaseWatt[4] += kw_y_sec;
//
////     kw_b_sec = ((signed long)wAdcVoltBuff[2]) * ((signed long)wAdcCurrBuff[2]) * (wGain_Kw_sec/10000);
////     dwSumPhaseWatt[5] += kw_b_sec;
//
////     /*
////      * Voltage Buffer Creation for CAN transmission
////      * Store every 5th sample (200 samples total from 1000)
////      */
////     sampleCounter++;
////     if (sampleCounter >= 5)  // Every 5th sample
////     {
////         sampleCounter = 0;  // Reset counter
//// //
////         if (voltageBufferIndex < VOLT_SIZE)  // VOLT_SIZE should be 200
////         {
////             // Store voltage samples for CAN transmission
//// //            Output_Volt_R[voltageBufferIndex] = wAdcVoltBuff[0];  // Already offset-corrected
//// //            Output_Volt_Y[voltageBufferIndex] = wAdcVoltBuff[1];
//// //            Output_Volt_B[voltageBufferIndex] = wAdcVoltBuff[2];
////             runtimeData.rPhaseBuffer[voltageBufferIndex]  = wAdcVoltBuff[0];
////             runtimeData.yPhaseBuffer[voltageBufferIndex]  = wAdcVoltBuff[1];
////             runtimeData.bPhaseBuffer[voltageBufferIndex]  = wAdcVoltBuff[2];
//
////             voltageBufferIndex++;
////         }
////     }
//
////     /*
////      * Frequency calculation - check for zero crossing
////      */
////     if((last_sample_R < 0 && wAdcVoltBuff[0] >= 0))
////     {
////         lasttimestamp_R = currenttimestamp_R;
////         currenttimestamp_R = CPUTimer_getTimerCount(CPUTIMER0_BASE);
////         dww_Frequency_Calculation();
////     }
////     last_sample_R = wAdcVoltBuff[0];
//
////     /*
////      * Increment the main sample counter //sin
////      */
////     sampleCount++;
//
////     /*
////      * Once 1000 samples have been collected:
////      * 1. Calculate RMS values
////      * 2. Calculate power values
////      * 3. Copy voltage buffer to runtime structure for CAN
////      */
////     if(sampleCount >= 160)  // Changed from SAMPLE_COUNT_THRESHOLD to explicit 1000
////     {
////         /*
////          * RMS calculation of the input, output and GND current
////          */
////         // Secondary currents
////         avgSquare = (float)dwSumPhaseCurr[0] / 1000.0f;
////         rms = sqrtf(avgSquare) * calib_curr_sec_r / 10000;
////         secondary.RMS_Curr_Phase_R = (float)rms;
////         dwSumPhaseCurr[0] = 0;
//
////         avgSquare = (float)dwSumPhaseCurr[1] / 1000.0f;
////         rms = sqrtf(avgSquare) * calib_curr_sec_y / 10000;
////         secondary.RMS_Curr_Phase_Y = (float)rms;
////         dwSumPhaseCurr[1] = 0;
//
////         avgSquare = (float)dwSumPhaseCurr[2] / 1000.0f;
////         rms = sqrtf(avgSquare) * calib_curr_sec_b / 10000;
////         secondary.RMS_Curr_Phase_B = (float)rms;
////         dwSumPhaseCurr[2] = 0;
//
////         // Primary currents
////         avgSquare = (float)dwSumPhaseCurr[3] / 1000.0f;
////         rms = sqrtf(avgSquare) * calib_curr_pri_r / 10000;
////         primary.RMS_Curr_Phase_R = (float)rms;
////         dwSumPhaseCurr[3] = 0;
//
////         avgSquare = (float)dwSumPhaseCurr[4] / 1000.0f;
////         rms = sqrtf(avgSquare) * calib_curr_pri_y / 10000;
////         primary.RMS_Curr_Phase_Y = (float)rms;
////         dwSumPhaseCurr[4] = 0;
//
////         avgSquare = (float)dwSumPhaseCurr[5] / 1000.0f;
////         rms = sqrtf(avgSquare) * calib_curr_pri_b / 10000;
////         primary.RMS_Curr_Phase_B = (float)rms;
////         dwSumPhaseCurr[5] = 0;
//
////         // Ground current
////         avgSquare = (float)dwSumPhaseCurr[6] / 1000.0f;
////         rms = sqrtf(avgSquare) * calib_curr_pri_b / 10000;
////         system_para.Ground_current = (float)rms;
////         dwSumPhaseCurr[6] = 0;
//
////         /*
////          * Current masking (5A threshold)
////          */
////         if(primary.RMS_Curr_Phase_R <= 8.0)
////             primary.RMS_Curr_Phase_R = 0;
////         if(primary.RMS_Curr_Phase_Y <= 8.0)
////             primary.RMS_Curr_Phase_Y = 0;
////         if(primary.RMS_Curr_Phase_B <= 8.0)
////             primary.RMS_Curr_Phase_B = 0;
////         if(secondary.RMS_Curr_Phase_R <= 8.0)
////             secondary.RMS_Curr_Phase_R = 0;
////         if(secondary.RMS_Curr_Phase_Y <= 8.0)
////             secondary.RMS_Curr_Phase_Y = 0;
////         if(secondary.RMS_Curr_Phase_B <= 8.0)
////             secondary.RMS_Curr_Phase_B = 0;
//
////         /*
////          * RMS Calculation of the Input and Output Voltage
////          */
////         // Secondary voltages (Line to Neutral)
////         avgSquare = (float)dwSumPhaseVolt[0] / 160.0f;
////         rms = sqrtf(avgSquare) * calib_sec_R / 100.0f;
////         secondary.L2N_Volt_Phase_R = (float)rms;
////         dwSumPhaseVolt[0] = 0;
//
////         avgSquare = (float)dwSumPhaseVolt[1] / 160.0f;
////         rms = sqrtf(avgSquare) * calib_sec_Y / 100.0f;
////         secondary.L2N_Volt_Phase_Y = (float)rms;
////         dwSumPhaseVolt[1] = 0;
//
////         avgSquare = (float)dwSumPhaseVolt[2] / 160.0f;
////         rms = sqrtf(avgSquare) * calib_sec_B / 100.0f;
////         secondary.L2N_Volt_Phase_B = (float)rms;
////         dwSumPhaseVolt[2] = 0;
//
////         // Primary voltages (Line to Line)
////         avgSquare = (float)dwSumPhaseVolt[3] / 160.0f;
////         rms = sqrtf(avgSquare) * calib_pri_R / 100.0f;
////         primary.L2L_Volt_Phase_RY = (float)rms;
////         dwSumPhaseVolt[3] = 0;
//
////         avgSquare = (float)dwSumPhaseVolt[4] / 160.0f;
////         rms = sqrtf(avgSquare) * calib_pri_Y / 100.0f;
////         primary.L2L_Volt_Phase_YB = (float)rms;
////         dwSumPhaseVolt[4] = 0;
//
////         avgSquare = (float)dwSumPhaseVolt[5] / 160.0f;
////         rms = sqrtf(avgSquare) * calib_pri_B / 100.0f;
////         primary.L2L_Volt_Phase_BR = (float)rms;
////         dwSumPhaseVolt[5] = 0;
//
////         /*
////          * Voltage masking (25V threshold)
////          */
////         if(primary.L2L_Volt_Phase_RY <= 30.0)
////             primary.L2L_Volt_Phase_RY = 0;
////         if(primary.L2L_Volt_Phase_YB <= 30.0)
////             primary.L2L_Volt_Phase_YB = 0;
////         if(primary.L2L_Volt_Phase_BR <= 30.0)
////             primary.L2L_Volt_Phase_BR = 0;
////         if(secondary.L2N_Volt_Phase_R <= 30.0)
////             secondary.L2N_Volt_Phase_R = 0;
////         if(secondary.L2N_Volt_Phase_Y <= 30.0)
////             secondary.L2N_Volt_Phase_Y = 0;
////         if(secondary.L2N_Volt_Phase_B <= 30.0)
////             secondary.L2N_Volt_Phase_B = 0;
//
////         /*
////          * Power calculation - average over 1000 samples
////          */
////         primary.Total_KW_R = (dwSumPhaseWatt[0]) / 1000000.0f;  // Divide by 1000 for samples, then by 1000 for kW
////         dwSumPhaseWatt[0] = 0;
//
////         primary.Total_KW_Y = (dwSumPhaseWatt[1]) / 1000000.0f;
////         dwSumPhaseWatt[1] = 0;
//
////         primary.Total_KW_B = (dwSumPhaseWatt[2]) / 1000000.0f;
////         dwSumPhaseWatt[2] = 0;
//
////         secondary.KW_Phase_R = (3.0f * dwSumPhaseWatt[3]) / 1000000.0f;
////         dwSumPhaseWatt[3] = 0;
//
////         secondary.KW_Phase_Y = (3.0f * dwSumPhaseWatt[4]) / 1000000.0f;
////         dwSumPhaseWatt[4] = 0;
//
////         secondary.KW_Phase_B = (3.0f * dwSumPhaseWatt[5]) / 1000000.0f;
////         dwSumPhaseWatt[5] = 0;
//
////         /*
////          * Copy 200-sample voltage buffer to runtime structure for CAN transmission
////          * This happens once every 1000 samples when we have collected 200 voltage samples
////          */
////         if (voltageBufferIndex >= VOLT_SIZE)  // Should be 200
////         {
////            // copyVoltageBuffersToRuntimeStruct();
////             voltageBufferIndex = 0;  // Reset for next cycle
////         }
//
////         /*
////          * Update voltage buffers for power calculations
////          */
////         primary.Volt_buff[0] = primary.L2L_Volt_Phase_RY;
////         primary.Volt_buff[1] = primary.L2L_Volt_Phase_YB;
////         primary.Volt_buff[2] = primary.L2L_Volt_Phase_BR;
//
////         secondary.Volt_buff[0] = (float)secondary.L2N_Volt_Phase_R;
////         secondary.Volt_buff[1] = (float)secondary.L2N_Volt_Phase_Y;
////         secondary.Volt_buff[2] = (float)secondary.L2N_Volt_Phase_B;
//
////         /*
////          * Reset counters for next 1000-sample cycle
////          */
////         sampleCount = 0;
//
////         // Clear power accumulation variables
////         kw_r = 0;
////         kw_y = 0;
////         kw_b = 0;
////         kw_r_sec = 0;
////         kw_y_sec = 0;
////         kw_b_sec = 0;
////     }
//
////     /*
////      * Clear the ADC interrupt flags
////      */
////     ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER1);
////     ADC_clearInterruptStatus(myADC1_BASE, ADC_INT_NUMBER1);
//
////     if(ADC_getInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER1))
////     {
////         ADC_clearInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER1);
////         ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER1);
////     }
//
////     if(ADC_getInterruptOverflowStatus(myADC1_BASE, ADC_INT_NUMBER1))
////     {
////         ADC_clearInterruptOverflowStatus(myADC1_BASE, ADC_INT_NUMBER1);
////         ADC_clearInterruptStatus(myADC1_BASE, ADC_INT_NUMBER1);
////     }
//
////     Interrupt_clearACKGroup(INT_myADC0_1_INTERRUPT_ACK_GROUP);
//// }
//
//
//void dww_phase_sequence_error()
//{
////
////    adc_r = (wAdcVoltBuff[0]);
////         adc_y = (wAdcVoltBuff[1]);
////         adc_b = (wAdcVoltBuff[2]);
////
////    /*
////    * Clarks transformation
////    */
////        v_alpha = ((2.0f / 3.0f) * adc_r) - ((1.0f / 3.0f) * adc_y) - ((1.0f / 3.0f) * adc_b);
////        v_beta =  (1.0f / sqrtf(3.0f)) * (adc_y - adc_b);
////
////        theta_p = atan2f(v_beta, v_alpha);
////        del_theta_p = theta_p * (180.0f / 3.1415926f);
////#if 0
////
////    if (v_alpha >30 &&(v_alpha<100))
////    {
////        if(v_beta > 0)
////        {
////            Phase_Seq_flag = 1;
////        }
////        else
////            Phase_Seq_flag = 0;
////
////    }
////#endif
////
//}
//
//
///*
// *Logic added calculating the Frequency
// *
// */
//void dww_Frequency_Calculation()
//{
//
//
//
//}
//
///*
// * This Function includes the following Calculations:
// * 1. Primary Line to Neutral calculation
// * 2. Primary Average Current
// * 3. Primary Average Voltage
// * 4. Primary Neutral Current
// * 5. Primary KVA
// * 6. Primary KVAR
// */
//void dww_Pri_Sec_Calculation(unsigned int reg)
//{
//    unsigned int dwCurrSqr;
//    unsigned int dwCurrSum;
//
//    switch(reg)
//    {
//    case PRIMARY:
//    {
//        /*
//         * Calculation to convert the L2L voltage to L2N Voltage for Input
//         */
//        primary.L2N_Volt_Phase_R = (primary.L2L_Volt_Phase_RY / sqrtf(3.0));
//        primary.L2N_Volt_Phase_Y = (primary.L2L_Volt_Phase_YB / sqrtf(3.0));
//        primary.L2N_Volt_Phase_B = (primary.L2L_Volt_Phase_BR / sqrtf(3.0));
//
//        /*
//         * Calculation for the Primary Average Current
//         */
//        primary.RMS_Avg_Curr = ((primary.RMS_Curr_Phase_R + primary.RMS_Curr_Phase_Y + primary.RMS_Curr_Phase_B) / 3);
//
//        /*
//         * Calculation for the Primary Average Voltage
//         */
//        primary.RMS_Avg_Volt = ((primary.L2L_Volt_Phase_RY + primary.L2L_Volt_Phase_YB + primary.L2L_Volt_Phase_BR) / 3);
//
//        /*
//         * Calculation for the Primary Neutral Current
//         */
//        dwCurrSqr = (primary.RMS_Curr_Phase_R * primary.RMS_Curr_Phase_R) +
//                    (primary.RMS_Curr_Phase_Y * primary.RMS_Curr_Phase_Y) +
//                    (primary.RMS_Curr_Phase_B * primary.RMS_Curr_Phase_B);
//        dwCurrSum = (primary.RMS_Curr_Phase_R * primary.RMS_Curr_Phase_Y) +
//                    (primary.RMS_Curr_Phase_R * primary.RMS_Curr_Phase_B) +
//                    (primary.RMS_Curr_Phase_Y * primary.RMS_Curr_Phase_B);
//        primary.Neutral_Current = sqrtf(dwCurrSqr - dwCurrSum);
//
//        break;
//    }
//    case SECONDARY:
//    {
//        /*
//         * Calculation to convert L2N voltage to L2L Voltage for Output
//         */
//
//        secondary.L2L_Volt_Phase_RY = (secondary.L2N_Volt_Phase_R * sqrtf(3));
//        secondary.L2L_Volt_Phase_YB = (secondary.L2N_Volt_Phase_Y * sqrtf(3));
//        secondary.L2L_Volt_Phase_BR = (secondary.L2N_Volt_Phase_B * sqrtf(3));
//
//        /*
//         * Calculation for the secondary Average Current
//         */
//        secondary.RMS_Avg_Curr = ((secondary.RMS_Curr_Phase_R + secondary.RMS_Curr_Phase_Y + secondary.RMS_Curr_Phase_B) / 3);
//
//        /*
//         * Calculation for the secondary Average Line to Line Voltage
//         */
//        secondary.RMS_Avg_Volt = ((secondary.L2L_Volt_Phase_RY + secondary.L2L_Volt_Phase_YB + secondary.L2L_Volt_Phase_BR) / 3);
//
//        /*
//         * Calculation for the secondary Average Line to Neutral Voltage
//         */
//        secondary.RMS_Avg_L2N_Volt = ((secondary.L2N_Volt_Phase_R + secondary.L2N_Volt_Phase_Y + secondary.L2N_Volt_Phase_B) / 3);
//
//        /*
//         * Calculation for the Crest factor
//         */
//
//        secondary.Crest_Factor_Phase_R = ((secondary.RMS_Curr_Phase_R * sqrtf(2.0f)) / (secondary.RMS_Curr_Phase_R));
//        secondary.Crest_Factor_Phase_Y = ((secondary.RMS_Curr_Phase_Y * sqrtf(2.0f)) / (secondary.RMS_Curr_Phase_Y));
//        secondary.Crest_Factor_Phase_B = ((secondary.RMS_Curr_Phase_B * sqrtf(2.0f)) / (secondary.RMS_Curr_Phase_B));
//#if 0
//        /*
//         * New logic added to calculate the crest factor for each phase
//         * The values here provided is based on the assumptions. Once we verify with peak current
//         * We can adjust the value accordingly after testing with the non linear load
//         */
//        if (secondary.RMS_Curr_Phase_R < 11.0)  //For Secondary R Phase
//        {
//            if (secondary.RMS_Curr_Phase_R < 5.0)
//            {
//                secondary.Crest_Factor_Phase_R = 0xFFFF;
//            }
//            else
//                secondary.Crest_Factor_Phase_R = 2.30;
//        }
//        else
//            secondary.Crest_Factor_Phase_R = 1.45;
//
//        if (secondary.RMS_Curr_Phase_Y < 11.0)  //For Secondary Y Phase
//        {
//            if (secondary.RMS_Curr_Phase_Y < 5.0)
//            {
//                secondary.Crest_Factor_Phase_Y = 0xFFFF;
//            }
//            else
//                secondary.Crest_Factor_Phase_Y = 2.30;
//        }
//        else
//            secondary.Crest_Factor_Phase_Y = 1.45;
//
//        if (secondary.RMS_Curr_Phase_B < 11.0)  //For Secondary B Phase
//        {
//            if (secondary.RMS_Curr_Phase_B < 5.0)
//            {
//                secondary.Crest_Factor_Phase_B = 0xFFFF;
//            }
//            else
//                secondary.Crest_Factor_Phase_B = 2.30;
//        }
//        else
//            secondary.Crest_Factor_Phase_B = 1.45;
//#endif
//
//        /*
//         * Calculation for the Secondary Neutral Current
//         */
//        dwCurrSqr = (secondary.RMS_Curr_Phase_R * secondary.RMS_Curr_Phase_R) +
//                    (secondary.RMS_Curr_Phase_Y * secondary.RMS_Curr_Phase_Y) +
//                    (secondary.RMS_Curr_Phase_B * secondary.RMS_Curr_Phase_B);
//        dwCurrSum = (secondary.RMS_Curr_Phase_R * secondary.RMS_Curr_Phase_Y) +
//                    (secondary.RMS_Curr_Phase_R * secondary.RMS_Curr_Phase_B) +
//                    (secondary.RMS_Curr_Phase_Y * secondary.RMS_Curr_Phase_B);
//        secondary.Neutral_Current = sqrtf(dwCurrSqr - dwCurrSum);
//
//        break;
//    }
//    }
//}
//
///*
// * Logic for Maximum and Minimum Parameter Calculation for input and Output parameters
// */
//void dww_Max_Min_Calculation(unsigned int reg)
//{
//    switch (reg)
//    {
//    case PRIMARY:
//    {
//        /*
//         * Calculation for Input Maximum Current
//         */
//        if (((primary.RMS_Curr_Phase_R != 0xFFFF) &&
//             ((primary.MAX_Curr_Phase_R < primary.RMS_Curr_Phase_R) ||
//              (primary.MAX_Curr_Phase_R == 0xFFFF))))
//        {
//            primary.MAX_Curr_Phase_R = primary.RMS_Curr_Phase_R;
//        }
//
//        if (((primary.RMS_Curr_Phase_Y != 0xFFFF) &&
//             ((primary.MAX_Curr_Phase_Y < primary.RMS_Curr_Phase_Y) ||
//              (primary.MAX_Curr_Phase_Y == 0xFFFF))))
//        {
//            primary.MAX_Curr_Phase_Y = primary.RMS_Curr_Phase_Y;
//        }
//
//        if (((primary.RMS_Curr_Phase_B != 0xFFFF) &&
//             ((primary.MAX_Curr_Phase_B < primary.RMS_Curr_Phase_B) ||
//              (primary.MAX_Curr_Phase_B == 0xFFFF))))
//        {
//            primary.MAX_Curr_Phase_B = primary.RMS_Curr_Phase_B;
//        }
//        /*
//         * Calculation for Input Minimum Current
//         */
//        if (((primary.RMS_Curr_Phase_R != 0x0000) &&
//             ((primary.MIN_Curr_Phase_R > primary.RMS_Curr_Phase_R) ||
//              (primary.MIN_Curr_Phase_R == 0x0000))))
//        {
//            primary.MIN_Curr_Phase_R = primary.RMS_Curr_Phase_R;
//        }
//
//        if (((primary.RMS_Curr_Phase_Y != 0x0000) &&
//             ((primary.MIN_Curr_Phase_Y > primary.RMS_Curr_Phase_Y) ||
//              (primary.MIN_Curr_Phase_Y == 0x0000))))
//        {
//            primary.MIN_Curr_Phase_Y = primary.RMS_Curr_Phase_Y;
//        }
//
//        if (((primary.RMS_Curr_Phase_B != 0x0000) &&
//             ((primary.MIN_Curr_Phase_B > primary.RMS_Curr_Phase_B) ||
//              (primary.MIN_Curr_Phase_B == 0x0000))))
//        {
//            primary.MIN_Curr_Phase_B = primary.RMS_Curr_Phase_B;
//        }
//
//        /*
//         * Calculation for Input Maximum Voltage
//         */
//        if (((primary.L2N_Volt_Phase_R != 0xFFFF) &&
//             ((primary.MAX_Volt_Phase_R < primary.L2N_Volt_Phase_R) ||
//              (primary.MAX_Volt_Phase_R == 0xFFFF))))
//        {
//            primary.MAX_Volt_Phase_R = primary.L2N_Volt_Phase_R;
//        }
//
//        if (((primary.L2N_Volt_Phase_Y != 0xFFFF) &&
//             ((primary.MAX_Volt_Phase_Y < primary.L2N_Volt_Phase_Y) ||
//              (primary.MAX_Volt_Phase_Y == 0xFFFF))))
//        {
//            primary.MAX_Volt_Phase_Y = primary.L2N_Volt_Phase_Y;
//        }
//
//        if (((primary.L2N_Volt_Phase_B != 0xFFFF) &&
//             ((primary.MAX_Volt_Phase_B < primary.L2N_Volt_Phase_B) ||
//              (primary.MAX_Volt_Phase_B == 0xFFFF))))
//        {
//            primary.MAX_Volt_Phase_B = primary.L2N_Volt_Phase_B;
//        }
//        /*
//         * Calculation for Input Minimum Voltage
//         */
//        if (((primary.L2N_Volt_Phase_R != 0x0000) &&
//             ((primary.MIN_Volt_Phase_R > primary.L2N_Volt_Phase_R) ||
//              (primary.MIN_Volt_Phase_R == 0x0000))))
//        {
//            primary.MIN_Volt_Phase_R = primary.L2N_Volt_Phase_R;
//        }
//
//        if (((primary.L2N_Volt_Phase_Y != 0x0000) &&
//             ((primary.MIN_Volt_Phase_Y > primary.L2N_Volt_Phase_Y) ||
//              (primary.MIN_Volt_Phase_Y == 0x0000))))
//        {
//            primary.MIN_Volt_Phase_Y = primary.L2N_Volt_Phase_Y;
//        }
//
//        if (((primary.L2N_Volt_Phase_B != 0x0000) &&
//             ((primary.MIN_Volt_Phase_B > primary.L2N_Volt_Phase_B) ||
//              (primary.MIN_Volt_Phase_B == 0x0000))))
//        {
//            primary.MIN_Volt_Phase_B = primary.L2N_Volt_Phase_B;
//        }
//        break;
//    }
//    case SECONDARY:
//    {
//        /*
//         * Calculation for Output Maximum Current
//         */
//        if (((secondary.RMS_Curr_Phase_R != 0xFFFF) &&
//             ((secondary.MAX_Curr_Phase_R < secondary.RMS_Curr_Phase_R) ||
//              (secondary.MAX_Curr_Phase_R == 0xFFFF))))
//        {
//            secondary.MAX_Curr_Phase_R = secondary.RMS_Curr_Phase_R;
//        }
//
//        if (((secondary.RMS_Curr_Phase_Y != 0xFFFF) &&
//             ((secondary.MAX_Curr_Phase_Y < secondary.RMS_Curr_Phase_Y) ||
//              (secondary.MAX_Curr_Phase_Y == 0xFFFF))))
//        {
//            secondary.MAX_Curr_Phase_Y = secondary.RMS_Curr_Phase_Y;
//        }
//
//        if (((secondary.RMS_Curr_Phase_B != 0xFFFF) &&
//             ((secondary.MAX_Curr_Phase_B < secondary.RMS_Curr_Phase_B) ||
//              (secondary.MAX_Curr_Phase_B == 0xFFFF))))
//        {
//            secondary.MAX_Curr_Phase_B = secondary.RMS_Curr_Phase_B;
//        }
//        /*
//         * Calculation for Output Minimum Current
//         */
//        if (((secondary.RMS_Curr_Phase_R != 0x0000) &&
//             ((secondary.MIN_Curr_Phase_R > secondary.RMS_Curr_Phase_R) ||
//              (secondary.MIN_Curr_Phase_R == 0x0000))))
//        {
//            secondary.MIN_Curr_Phase_R = secondary.RMS_Curr_Phase_R;
//        }
//
//        if (((secondary.RMS_Curr_Phase_Y != 0x0000) &&
//             ((secondary.MIN_Curr_Phase_Y > secondary.RMS_Curr_Phase_Y) ||
//              (secondary.MIN_Curr_Phase_Y == 0x0000))))
//        {
//            secondary.MIN_Curr_Phase_Y = secondary.RMS_Curr_Phase_Y;
//        }
//
//        if (((secondary.RMS_Curr_Phase_B != 0x0000) &&
//             ((secondary.MIN_Curr_Phase_B > secondary.RMS_Curr_Phase_B) ||
//              (secondary.MIN_Curr_Phase_B == 0x0000))))
//        {
//            secondary.MIN_Curr_Phase_B = secondary.RMS_Curr_Phase_B;
//        }
//        /*
//         * Calculation for the Output Maximum Voltage
//         */
//        if (((secondary.L2N_Volt_Phase_R != 0xFFFF) &&
//             ((secondary.MAX_Volt_Phase_R < secondary.L2N_Volt_Phase_R) ||
//              (secondary.MAX_Volt_Phase_R == 0xFFFF))))
//        {
//            secondary.MAX_Volt_Phase_R = secondary.L2N_Volt_Phase_R;
//        }
//
//        if (((secondary.L2N_Volt_Phase_Y != 0xFFFF) &&
//             ((secondary.MAX_Volt_Phase_Y < secondary.L2N_Volt_Phase_Y) ||
//              (secondary.MAX_Volt_Phase_Y == 0xFFFF))))
//        {
//            secondary.MAX_Volt_Phase_Y = secondary.L2N_Volt_Phase_Y;
//        }
//
//        if (((secondary.L2N_Volt_Phase_B != 0xFFFF) &&
//             ((secondary.MAX_Volt_Phase_B < secondary.L2N_Volt_Phase_B) ||
//              (secondary.MAX_Volt_Phase_B == 0xFFFF))))
//        {
//            secondary.MAX_Volt_Phase_B = secondary.L2N_Volt_Phase_B;
//        }
//        /*
//         * Calculation for the Output Minimum Voltage
//         */
//        if (((secondary.L2N_Volt_Phase_R != 0x0000) &&
//             ((secondary.MIN_Volt_Phase_R > secondary.L2N_Volt_Phase_R) ||
//              (secondary.MIN_Volt_Phase_R == 0x0000))))
//        {
//            secondary.MIN_Volt_Phase_R = secondary.L2N_Volt_Phase_R;
//        }
//
//        if (((secondary.L2N_Volt_Phase_Y != 0x0000) &&
//             ((secondary.MIN_Volt_Phase_Y > secondary.L2N_Volt_Phase_Y) ||
//              (secondary.MIN_Volt_Phase_Y == 0x0000))))
//        {
//            secondary.MIN_Volt_Phase_Y = secondary.L2N_Volt_Phase_Y;
//        }
//
//        if (((secondary.L2N_Volt_Phase_B != 0x0000) &&
//             ((secondary.MIN_Volt_Phase_B > secondary.L2N_Volt_Phase_B) ||
//              (secondary.MIN_Volt_Phase_B == 0x0000))))
//        {
//            secondary.MIN_Volt_Phase_B = secondary.L2N_Volt_Phase_B;
//        }
//    }
//    default:
//        break;
//    }
//}
//
//void dww_Phase_Calculation()
//{
//    /*
//     * Added Voltage Lack Calculation logic
//     */
//    Sec_Phase_RY = (secondary.L2L_Volt_Phase_RY > secondary.L2L_Volt_Phase_YB) ? (secondary.L2L_Volt_Phase_RY - secondary.L2L_Volt_Phase_YB)
//                   : (secondary.L2L_Volt_Phase_YB - secondary.L2L_Volt_Phase_RY);
//    Sec_Phase_YB = (secondary.L2L_Volt_Phase_YB > secondary.L2L_Volt_Phase_BR) ? (secondary.L2L_Volt_Phase_YB - secondary.L2L_Volt_Phase_BR)
//                   : (secondary.L2L_Volt_Phase_BR - secondary.L2L_Volt_Phase_YB);
//    Sec_Phase_BR = (secondary.L2L_Volt_Phase_BR > secondary.L2L_Volt_Phase_RY) ? (secondary.L2L_Volt_Phase_BR - secondary.L2L_Volt_Phase_RY)
//                   : (secondary.L2L_Volt_Phase_RY - secondary.L2L_Volt_Phase_BR);
//    Pri_Phase_RY = (primary.L2L_Volt_Phase_RY > primary.L2L_Volt_Phase_YB) ? (primary.L2L_Volt_Phase_RY - primary.L2L_Volt_Phase_YB)
//                   : (primary.L2L_Volt_Phase_YB - primary.L2L_Volt_Phase_RY);
//    Pri_Phase_YB = (primary.L2L_Volt_Phase_YB > primary.L2L_Volt_Phase_BR) ? (primary.L2L_Volt_Phase_YB - primary.L2L_Volt_Phase_BR)
//                   : (primary.L2L_Volt_Phase_BR - primary.L2L_Volt_Phase_YB);
//    Pri_Phase_BR = (primary.L2L_Volt_Phase_BR > primary.L2L_Volt_Phase_RY) ? (primary.L2L_Volt_Phase_BR - primary.L2L_Volt_Phase_RY)
//                   : (primary.L2L_Volt_Phase_RY - primary.L2L_Volt_Phase_BR);
//
//    if ((Sec_Phase_RY > LACKING_VOLT) ||
//        (Sec_Phase_YB > LACKING_VOLT) ||
//        (Sec_Phase_BR > LACKING_VOLT) ||
//        (Pri_Phase_RY > LACKING_VOLT) ||
//        (Pri_Phase_YB > LACKING_VOLT) ||
//        (Pri_Phase_BR > LACKING_VOLT))
//    {
//        Volt_Lack_flag = 1;
//    }
//    else
//    {
//        if ((Sec_Phase_RY < (unsigned int)(LACKING_VOLT - PHASE_LACK)) ||
//            (Sec_Phase_YB < (unsigned int)(LACKING_VOLT - PHASE_LACK)) ||
//            (Sec_Phase_BR < (unsigned int)(LACKING_VOLT - PHASE_LACK)) ||
//            (Pri_Phase_RY < (unsigned int)(LACKING_VOLT - PHASE_LACK)) ||
//            (Pri_Phase_YB < (unsigned int)(LACKING_VOLT - PHASE_LACK)) ||
//            (Pri_Phase_BR < (unsigned int)(LACKING_VOLT - PHASE_LACK)))
//        {
//            Volt_Lack_flag = 0;
//        }
//    }
//
//    /*
//     * Added the Voltage Unbalance Logic
//     */
//    Sec_Phase_RY = (secondary.L2L_Volt_Phase_RY > secondary.RMS_Avg_Volt) ? (secondary.L2L_Volt_Phase_RY - secondary.RMS_Avg_Volt)
//                   : (secondary.RMS_Avg_Volt - secondary.L2L_Volt_Phase_RY);
//    Sec_Phase_YB = (secondary.L2L_Volt_Phase_YB > secondary.RMS_Avg_Volt) ? (secondary.L2L_Volt_Phase_YB - secondary.RMS_Avg_Volt)
//                   : (secondary.RMS_Avg_Volt - secondary.L2L_Volt_Phase_YB);
//    Sec_Phase_BR = (secondary.L2L_Volt_Phase_BR > secondary.RMS_Avg_Volt) ? (secondary.L2L_Volt_Phase_BR - secondary.RMS_Avg_Volt)
//                   : (secondary.RMS_Avg_Volt - secondary.L2L_Volt_Phase_BR);
//    Pri_Phase_RY = (primary.L2L_Volt_Phase_RY > primary.RMS_Avg_Volt) ? (primary.L2L_Volt_Phase_RY - primary.RMS_Avg_Volt)
//                   : (primary.RMS_Avg_Volt - primary.L2L_Volt_Phase_RY);
//    Pri_Phase_YB = (primary.L2L_Volt_Phase_YB > primary.RMS_Avg_Volt) ? (primary.L2L_Volt_Phase_YB - primary.RMS_Avg_Volt)
//                   : (primary.RMS_Avg_Volt - primary.L2L_Volt_Phase_YB);
//    Pri_Phase_BR = (primary.L2L_Volt_Phase_BR > primary.RMS_Avg_Volt) ? (primary.L2L_Volt_Phase_BR - primary.RMS_Avg_Volt)
//                   : (primary.RMS_Avg_Volt - primary.L2L_Volt_Phase_BR);
//
//    if ((Sec_Phase_RY * 100) > (secondary.RMS_Avg_Volt * UNBALANCE_RATE) ||
//        (Sec_Phase_YB * 100) > (secondary.RMS_Avg_Volt * UNBALANCE_RATE) ||
//        (Sec_Phase_BR * 100) > (secondary.RMS_Avg_Volt * UNBALANCE_RATE) ||
//        (Pri_Phase_RY * 100) > (secondary.RMS_Avg_Volt * UNBALANCE_RATE) ||
//        (Pri_Phase_YB * 100) > (secondary.RMS_Avg_Volt * UNBALANCE_RATE) ||
//        (Pri_Phase_BR * 100) > (secondary.RMS_Avg_Volt * UNBALANCE_RATE))
//    {
//        Volt_Unbalance_flag = 1;
//    }
//    else
//    {
//        Volt_Unbalance_flag = 0;
//    }
//
//    /*
//     * Added the Phase Loss Logic
//     */
//    if ((primary.L2L_Volt_Phase_RY < PHASE_LOSS) ||
//        (primary.L2L_Volt_Phase_YB < PHASE_LOSS) ||
//        (primary.L2L_Volt_Phase_BR < PHASE_LOSS) ||
//        (secondary.L2L_Volt_Phase_RY < PHASE_LOSS) ||
//        (secondary.L2L_Volt_Phase_YB < PHASE_LOSS) ||
//        (secondary.L2L_Volt_Phase_BR < PHASE_LOSS))
//    {
//        Phase_loss_flag = 1;
//    }
//    else
//    {
//        Phase_loss_flag = 0;
//    }
//
//}
//
///*
// * In this function we have created a buffer for instataneous voltage
// * But while send it to the BU board we need to implement the delay(20ms) in CAN parsing code
// */
//// void dww_volt_buff_creation()
//// {
//
//
////     volt_buff[0] = wAdcVoltBuff[0];
////     volt_buff[1] = wAdcVoltBuff[1];
////     volt_buff[2] = wAdcVoltBuff[2];
//
////     index++;
//
////     if (index >= BUFFER_SIZE)
////     {
////         index = 0;
////     }
//
//// }
//
//
//void dww_Power_Calculation(unsigned int reg)
//{
//    switch(reg)
//    {
//    case PRIMARY:
//    {
//
//        /*
//         * Calculation for the primary Toatal KW
//         */
//
//        primary.Total_KW_RYB = primary.Total_KW_R + primary.Total_KW_Y + primary.Total_KW_B;
//
//#if 0
//        /*
//         * Calculation for the primary KW
//         */
//        if((primary.RMS_Curr_Phase_R >= 5.0) || (primary.RMS_Curr_Phase_Y >= 5.0) || (primary.RMS_Curr_Phase_B >= 5.0))
//        {
//        dwMathBuff = (float)dwSumPhaseWatt[0];
//        dwMathBuff = (float)(dwMathBuff) / 3750;//(unsigned int)wGain_Kw;
//        primary.Total_KW = (float)dwMathBuff/1000;
//        dwSumPhaseWatt[0] = 0;
//       // countSample = 0;
//
//        }
//        else
//        {
//            primary.Total_KW = 0;
//            dwSumPhaseWatt[0] = 0;
//        }
//#endif
//
//
//        /*
//         * Calculation for the Primary KVA
//         */
//        dwMathBuff = ((3.0f * (primary.L2N_Volt_Phase_R * primary.RMS_Curr_Phase_R)) +
//                      (3.0f * (primary.L2N_Volt_Phase_Y * primary.RMS_Curr_Phase_Y)) +
//                      (3.0f * (primary.L2N_Volt_Phase_B * primary.RMS_Curr_Phase_B)));
//        primary.Total_KVA = (float)dwMathBuff / 1000;
//
//
//
//        /*
//         * Calculation for the Primary KVAR
//         */
//        primary.Total_KVAR = sqrtf((float)(primary.Total_KVA * primary.Total_KVA) -
//                                  (float)(primary.Total_KW_RYB * primary.Total_KW_RYB));
//
//        /*
//         * Calculation for the Total Power factor
//         */
//        if (primary.Total_KVA != 0)
//        {
//            primary.Total_PF = (float)((primary.Total_KW_RYB) / primary.Total_KVA);
//        }
//        else
//        {
//            primary.Total_PF = 0xFFFF;
//        }
//        break;
//    }
//    case SECONDARY:
//    {
//
//        /*
//         * Calculation for the primary Toatal KW
//         */
//
//        secondary.Total_KW = secondary.KW_Phase_R + secondary.KW_Phase_Y + secondary.KW_Phase_B;
//
//        /*
//         * Calculation for the KVA per phase and total KVA of the secondary
//         */
//        dwMathBuff = ((3.0f) * secondary.L2N_Volt_Phase_R * secondary.RMS_Curr_Phase_R);
//        secondary.KVA_Phase_R = (float)(dwMathBuff / 1000);
//
//        dwMathBuff = ((3.0f) * secondary.L2N_Volt_Phase_Y * secondary.RMS_Curr_Phase_Y);
//        secondary.KVA_Phase_Y = (float)(dwMathBuff / 1000);
//
//        dwMathBuff = ((3.0f) * secondary.L2N_Volt_Phase_B * secondary.RMS_Curr_Phase_B);
//        secondary.KVA_Phase_B = (float)(dwMathBuff / 1000);
//
//        secondary.Total_KVA = (float)(secondary.KVA_Phase_R + secondary.KVA_Phase_Y + secondary.KVA_Phase_B);
//
//        /*
//         * Calculation for the the KVAR per phase and Total KVAR of the secondary
//         */
//        secondary.KVAR_Phase_R = sqrtf((float)(secondary.KVA_Phase_R * secondary.KVA_Phase_R) -
//                                       (float)(secondary.KW_Phase_R * secondary.KW_Phase_R));
//
//        secondary.KVAR_Phase_Y = sqrtf((float)(secondary.KVA_Phase_Y * secondary.KVA_Phase_Y) -
//                                      (float)(secondary.KW_Phase_Y * secondary.KW_Phase_Y));
//
//        secondary.KVAR_Phase_B = sqrtf((float)(secondary.KVA_Phase_B * secondary.KVA_Phase_B) -
//                                      (float)(secondary.KW_Phase_B * secondary.KW_Phase_B));
//
//        secondary.Total_KVAR = (float)(secondary.KVAR_Phase_R + secondary.KVAR_Phase_Y + secondary.KVAR_Phase_B);
//
//        break;
//
//    }
//    }
//
//}
//
//
//#if 0
////
//// Timer function (unchanged for now; adjust as per your timing requirements)
////
//void dww_timer(void)
//{
//    if (demand_chk_1sec >= 270) // for 1 sec
//    {
//        sFlag->demand_chk_1sec = 1;
//        demand_chk_1sec = 0;
//        demand_chk_1hr++;
//    }
//    else
//    {
//        demand_chk_1sec++;
//    }
//
//    if(demand_chk_1hr >= 60) // Changed from 3600 to 60 for testing
//    {
//        sFlag->demand_chk_1hr = 1;
//        demand_chk_1hr = 0;
//        demand_chk_24hr++;
//    }
//
//    if(demand_chk_24hr >= 5) // Changed from 24 to 5 for testing
//    {
//        sFlag->demand_chk_24hr = 1;
//        demand_chk_24hr = 0;
//    }
//}
//
////
//// Demand Calculation function (unchanged)
////
//void dww_demand_Calc(void)
//{
//    int i;
//    if (sFlag->demand_chk_1sec == 1)
//    {
//                for (i = 0; i < MAX_CT_LENGTH; i++)
//        {
//            dw_CurrDemandSum[i] += primary.dww_channel[i].RMS;
//        }
//        sFlag->demand_chk_1sec = 0;
//        wDemandSumCntr[0]++;
//    }
//
//    if (sFlag->demand_chk_1hr == 1)
//    {
//        for (i = 0; i < MAX_CT_LENGTH; i++)
//        {
//            primary.dww_channel[i].CTCurrDemand_1hr = (unsigned int)(dw_CurrDemandSum[i] / wDemandSumCntr[0]);
//            dw_CurrDemandSum[i] = 0;
//        }
//        sFlag->demand_chk_1hr = 0;
//        wDemandSumCntr[0] = 0;
//    }
//
//    if (sFlag->demand_chk_24hr == 1)
//    {
//        for (i = 0; i < MAX_CT_LENGTH; i++)
//        {
//            primary.dww_channel[i].CTMaxCurrDemand_24hr = primary.dww_channel[i].CTMaxCurrDemand_1hr;
//            primary.dww_channel[i].CTMaxCurrDemand_1hr = 0;
//        }
//        sFlag->demand_chk_24hr = 0;
//    }
//}
//#endif
//
//
//void adc_init()
//
//{
//
//    struct_size = sizeof(primary);
//  //  configureGPIO28();
//    Board_init();
//
//
//
//       /*
//        * Enabling the watchdog after the reset
//        */
//       //SysCtl_enableWatchdog();
//
//       //initEPWM();
//
//       sampleCount = 0;
//    //    for(i = 0; i < PHASE_CURR; i++)
//    //    {
//    //        dwSumPhaseCurr[i] = 0;
//    //    }
//    //    for(i = 0; i < PHASE_VOLT; i++)
//    //    {
//    //        dwSumPhaseVolt[i] = 0;
//    //    }
//
//       /*
//        * Added for the kw calculation
//        */
//
//    //    for(i = 0; i < 5; i++)
//    //    {
//    //        dwSumPhaseWatt[i] = 0;
//    //    }
//
//       EINT;
//       ERTM;
//       ADC_setVREF(ADCARESULT_BASE, ADC_REFERENCE_INTERNAL, ADC_REFERENCE_3_3V);
//       EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
//       EPWM_setTimeBaseCounterMode(EPWM1_BASE, 2);
//}
//
//
//void adc_start_process ()
//{
//    //ADC functions
//            a++;
//
//                    SysCtl_serviceWatchdog();  //This function reset the watchdog timer
//                    dww_Pri_Sec_Calculation(PRIMARY);
//                    dww_Pri_Sec_Calculation(SECONDARY);
//                    dww_Max_Min_Calculation(PRIMARY);
//                    dww_Max_Min_Calculation(SECONDARY);
//                    dww_Phase_Calculation();
//                    SysCtl_serviceWatchdog();
//                    dww_Power_Calculation(PRIMARY);
//                    dww_Power_Calculation(SECONDARY);
//                    SysCtl_serviceWatchdog();
//                    dww_volt_buff_creation();
//
//                    dww_phase_sequence_error();
//                    // Additional function can be placed here.
//
//}
//
//
//void copyVoltageBuffersToRuntimeStruct(void)
//{
//    int i;
//
//    // Copy output voltage buffers to runtime structure (convert signed int to uint16_t)
//    for (i = 0; i < VOLTAGE_BUFFER_SIZE && i < VOLT_SIZE; i++)
//    {
//        // Add offset to convert signed to unsigned (2050 offset already removed, add back as positive)
//        runtimeData.rPhaseBuffer[i] = (int)(Output_Volt_R[i] + 2050);
//        runtimeData.yPhaseBuffer[i] = (int)(Output_Volt_Y[i] + 2050);
//        runtimeData.bPhaseBuffer[i] = (int)(Output_Volt_B[i] + 2050);
//    }
//
//    // Fill remaining buffer if VOLT_SIZE < VOLTAGE_BUFFER_SIZE
//    for (i = VOLT_SIZE; i < VOLTAGE_BUFFER_SIZE; i++)
//    {
//        runtimeData.rPhaseBuffer[i] = 2050;  // Default value
//        runtimeData.yPhaseBuffer[i] = 2050;
//        runtimeData.bPhaseBuffer[i] = 2050;
//    }
//}
