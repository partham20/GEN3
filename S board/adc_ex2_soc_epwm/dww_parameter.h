

#ifndef DWW_PARAMETER_H
#define DWW_PARAMETER_H

#define PRIMARY     0
#define SECONDARY   1
#define PHASE       3


typedef struct {
    unsigned int demand_chk_1sec :1;
    unsigned int demand_chk_1hr :1;
    unsigned int demand_chk_24hr :1;

} dww_structflag;


typedef struct {
    unsigned int Ground_current;
    unsigned int Nominal_IP_Volt; //Configurable
    unsigned int Nominal_OP_Volt; // Configurable
    unsigned int PowerCapacity; //Configurable

} DWW_SYSTEM_PARAMETER;
/*
 * Added Structure for the primary parameter
 */
typedef struct {

    float L2L_Volt_Phase_RY;
    float L2L_Volt_Phase_YB;
    float L2L_Volt_Phase_BR;

    float L2N_Volt_Phase_R;
    float L2N_Volt_Phase_Y;
    float L2N_Volt_Phase_B;

    float RMS_Curr_Phase_R;
    float RMS_Curr_Phase_Y;
    float RMS_Curr_Phase_B;

    float Neutral_Current;
    float RMS_Avg_Curr;
    float RMS_Avg_Volt;


    float Freq_R_Phase;
    float Freq_Y_Phase;
    float Freq_B_Phase;

    float MAX_Curr_Phase_R;
    float MAX_Curr_Phase_Y;
    float MAX_Curr_Phase_B;

    float MIN_Curr_Phase_R;
    float MIN_Curr_Phase_Y;
    float MIN_Curr_Phase_B;

    float MAX_Volt_Phase_R;
    float MAX_Volt_Phase_Y;
    float MAX_Volt_Phase_B;

    float MIN_Volt_Phase_R;
    float MIN_Volt_Phase_Y;
    float MIN_Volt_Phase_B;

    float Total_KVA;
    float Total_KVAR;
    float Total_KW_R;
    float Total_KW_Y;
    float Total_KW_B;
    float Total_KW_RYB;
    float Total_PF;

    float Volt_buff[PHASE];

} DWW_PRIMARY_PARAMETER;
/*
 * Added Structure for the secondary parameter
 */
typedef struct {

    float L2L_Volt_Phase_RY;
    float L2L_Volt_Phase_YB;
    float L2L_Volt_Phase_BR;

    float L2N_Volt_Phase_R;
    float L2N_Volt_Phase_Y;
    float L2N_Volt_Phase_B;

    float RMS_Curr_Phase_R;
    float RMS_Curr_Phase_Y;
    float RMS_Curr_Phase_B;

    float Neutral_Current;
    float RMS_Avg_Curr;
    float RMS_Avg_Volt;
    float RMS_Avg_L2N_Volt;


    float Crest_Factor_Phase_R;
    float Crest_Factor_Phase_Y;
    float Crest_Factor_Phase_B;

    float Freq_R_Phase;
    float Freq_Y_Phase;
    float Freq_B_Phase;

    float MAX_Curr_Phase_R;
    float MAX_Curr_Phase_Y;
    float MAX_Curr_Phase_B;

    float MIN_Curr_Phase_R;
    float MIN_Curr_Phase_Y;
    float MIN_Curr_Phase_B;

    float MAX_Volt_Phase_R;
    float MAX_Volt_Phase_Y;
    float MAX_Volt_Phase_B;

    float MIN_Volt_Phase_R;
    float MIN_Volt_Phase_Y;
    float MIN_Volt_Phase_B;

    float KVA_Phase_R;
    float KVA_Phase_Y;
    float KVA_Phase_B;
    float Total_KVA;

    float KVAR_Phase_R;
    float KVAR_Phase_Y;
    float KVAR_Phase_B;
    float Total_KVAR;

    float KW_Phase_R;
    float KW_Phase_Y;
    float KW_Phase_B;
    float Total_KW;

    float Volt_buff[PHASE];

} DWW_SECONDARY_PARAMETER;

/*
 * Added structure for the Threshold
 */
typedef struct
{
    unsigned int UnderVolt_PhaseR;
    unsigned int UnderVolt_PhaseY;
    unsigned int UnderVolt_PhaseB;
    unsigned int OverVolt_PhaseR;
    unsigned int OverVolt_PhaseY;
    unsigned int OverVolt_PhaseB;

    unsigned int UnderCurr_PhaseR;
    unsigned int UnderCurr_PhaseY;
    unsigned int UnderCurr_PhaseB;
    unsigned int OverCurr_PhaseR;
    unsigned int OverCurr_PhaseY;
    unsigned int OverCurr_PhaseB;

    unsigned int Neutral_OverCurr;

} DWW_THRESHOLD_PARAMETER;

typedef union
{
    unsigned int array[16];
    DWW_THRESHOLD_PARAMETER word;

}DWW_THRESHOLD_UNION;


#endif
