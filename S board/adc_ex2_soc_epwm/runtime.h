/**
 * @file runtime.h
 * @brief Runtime voltage buffer transmission to BU boards
 */

#ifndef RUNTIME_H
#define RUNTIME_H

#include "common.h"

// Buffer sizes and frame configuration
#define VOLTAGE_BUFFER_SIZE         160     // 200 values per phase
#define VALUES_PER_FRAME           31      // 31 16-bit values per CAN frame (62 bytes / 2)
#define FRAMES_PER_PHASE           6       // 7 frames needed per phase (200/31 = 6.45, rounded up)
#define TOTAL_RUNTIME_FRAMES       18      // 3 phases * 7 frames per phase

// Phase identifiers for CAN frame first byte
#define PHASE_R                    0x01    // R phase identifier
#define PHASE_Y                    0x02    // Y phase identifier
#define PHASE_B                    0x03    // B phase identifier

// CAN ID for runtime voltage transmission
#define RUNTIME_CAN_ID             0x5     // CAN ID for voltage buffer transmission

// Runtime transmission structure
typedef struct {
    signed int rPhaseBuffer[VOLTAGE_BUFFER_SIZE];
    signed int yPhaseBuffer[VOLTAGE_BUFFER_SIZE];
    signed int bPhaseBuffer[VOLTAGE_BUFFER_SIZE];
    bool transmissionInProgress;
    bool transmissionComplete;
    uint8_t currentPhase;
    uint8_t currentFrame;
    uint16_t transmissionDelay;
    uint32_t bufferNumber; 
} RuntimeVoltageTransmission;

// Function prototypes
void initRuntimeVoltageTransmission(void);
void fillBuffersWithSineWave(void);

void processRuntimeTransmission(void);
bool isRuntimeTransmissionComplete(void);
void resetRuntimeTransmission(void);

// External variable declarations
extern RuntimeVoltageTransmission runtimeData;

#endif // RUNTIME_H
