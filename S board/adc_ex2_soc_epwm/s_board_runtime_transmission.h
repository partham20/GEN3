  #ifndef S_BOARD_RUNTIME_MBOARD_H
#define S_BOARD_RUNTIME_MBOARD_H

#include "common.h"
#include "runtimedata.h"
#include "s_board_adc.h"

// S-Board runtime transmission configuration
#define SBOARD_RUNTIME_TRANSMISSION_INTERVAL   50      // 500ms / 10ms = 50 iterations
#define SBOARD_RUNTIME_CAN_ID                  0x5     // CAN ID for S-Board runtime to M-Board

// S-Board identification
#define SBOARD_ID                              0x01    // S-Board ID
// S-Board firmware version — change this value for each firmware release
// Stored as a variable (not #define) so CCS always picks up the change without Clean Build
extern const uint8_t SBOARD_VERSION;

// S-Board runtime transmission state
typedef struct {
    uint32_t transmissionCounter;
    bool transmissionInProgress;
    uint8_t currentFrameIndex;
    bool allFramesSent;
} SBoardRuntimeTx_t;

// Function prototypes
void initSBoardRuntimeToMBoard(void);
void processSBoardRuntimeToMBoard(void);
void sendSBoardInputParamFrame1(void);
void sendSBoardInputParamFrame2(void);
void sendSBoardOutputParamFrame1(void);
void sendSBoardOutputParamFrame2(void);
void sendSBoardBranchFramesToMBoard(void);
void sendSingleSBoardRuntimeFrame(const uint8_t* frameData);
bool shouldTransmitSBoardRuntime(void);

// External variable declarations
extern SBoardRuntimeTx_t sBoardRuntimeTx;

#endif // S_BOARD_RUNTIME_MBOARD_H
