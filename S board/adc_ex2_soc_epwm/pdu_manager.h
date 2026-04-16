/**
 * @file pdu_manager.h
 * @brief PDU data structures and management functions
 *
 * Wire format (S-Board <-> M-Board): PDUData (451 words)
 * Flash format: sectorIndex(1) + PDUData(451) = 452 words
 *
 * PDUData field order starting at word 432:
 *   432 primaryVoltage.R
 *   433 primaryVoltage.Y
 *   434 primaryVoltage.B
 *   435 secondaryVoltage.R
 *   436 secondaryVoltage.Y
 *   437 secondaryVoltage.B
 *   438 primaryCurrent.R
 *   439 primaryCurrent.Y
 *   440 primaryCurrent.B
 *   441 secondaryCurrent.R
 *   442 secondaryCurrent.Y
 *   443 secondaryCurrent.B
 *   444 primaryKW.R
 *   445 primaryKW.Y
 *   446 primaryKW.B
 *   447 secondaryKW.R
 *   448 secondaryKW.Y
 *   449 secondaryKW.B
 *   450 groundCurrent
 */

#ifndef PDU_MANAGER_H
#define PDU_MANAGER_H

#include "common.h"

typedef struct {
    uint16_t R;
    uint16_t Y;
    uint16_t B;
} ThreePhaseValues;

typedef struct {
    uint16_t ctGain[numberOfCTs];
    uint16_t kwGain[numberOfCTs];
} CalibrationGains;

// PDUData: 451 words — field order matches M-Board wire format
struct PDUData {
    CalibrationGains gains;              // words 0-431
    ThreePhaseValues primaryVoltage;     // 432-434
    ThreePhaseValues secondaryVoltage;   // 435-437
    ThreePhaseValues primaryCurrent;     // 438-440
    ThreePhaseValues secondaryCurrent;   // 441-443
    ThreePhaseValues primaryKW;          // 444-446
    ThreePhaseValues secondaryKW;        // 447-449
    uint16_t groundCurrent;              // 450
};

// CalibrationData: 452 words (what gets stored in flash)
struct CalibrationData {
    uint16_t sectorIndex;
    PDUData pduData;
};

struct PDUDataManager {
    CalibrationData readWriteData;
    CalibrationData workingData;
    bool dataSync;
};

struct CANFrame {
    uint8_t data[64];
    uint16_t order;
};

// Initialization
void initializePDUManager(void);

// Data sync between working and read/write structures
void syncWorkingToReadWrite(void);
void syncReadWriteToWorking(void);
bool updateWorkingData(const CalibrationData *newData);

// Calibration defaults and validation
void writeDefaultCalibrationValues(void);
void validateCalibrationGains(void);

// Serialize PDUData into CAN frames (storedFrames[])
void prepareCalibrationFramesForTx(const PDUData *pduData);

// Low-level: pack a word buffer into CAN frames (storedFrames[])
void prepareFramesForTransmission(uint16_t *buffer, uint16_t bufferSizeWords);

// Process received calibration data from M-Board:
// copy to struct -> write to flash -> sync -> relay to BU boards -> ACK
void processReceivedCalibrationData(void);

// External variables
extern PDUDataManager pduManager;
extern CANFrame storedFrames[NUM_FRAMES];
extern uint16_t receivedBuffer[TOTAL_BUFFER_SIZE];

#endif // PDU_MANAGER_H
