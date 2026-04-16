/*
 * pdu_manager.h
 *
 * PDU Data Manager for Calibration Data
 *
 * Manages calibration data storage and retrieval:
 *   - CalibData: Flash-stored calibration structure (uint16_t gains)
 *   - PDUDataManager: Dual-structure pattern (readWriteData for flash, workingData for runtime)
 *   - Bridge functions: Convert between flash uint16_t and runtime float gains
 *
 *  Created on: Feb 19, 2026
 *      Author: Parthasarathy.M
 */

#ifndef SRC_PDU_MANAGER_H_
#define SRC_PDU_MANAGER_H_

#include "can_bu.h"

#ifdef CAN_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "flash_config.h"
#include "calibration.h"

/* ========================================================================== */
/*                             Type Definitions                               */
/* ========================================================================== */

/*
 * Calibration data structure (stored in flash)
 * sectorIndex:  Incremented each write for wear-leveling tracking (word 0)
 * BUBoardID:    Board identifier from DIP switches (word 1)
 * currentGain:  18 CT current calibration gains (words 2-19)
 * kwGain:       18 CT power calibration gains (words 20-37)
 */
typedef struct {
    uint16_t sectorIndex;
    uint16_t BUBoardID;
    uint16_t currentGain[FLASH_NUM_BRANCHES];
    uint16_t kwGain[FLASH_NUM_BRANCHES];
} CalibData;

/*
 * PDU Data Manager
 * readWriteData: Structure used for flash read/write operations
 * workingData:   Structure for active runtime operations
 * dataSync:      Flag tracking if both structures are in sync
 */
typedef struct {
    CalibData readWriteData;
    CalibData workingData;
    bool dataSync;
} PDUDataManager;

/* ========================================================================== */
/*                          Global Variables                                  */
/* ========================================================================== */

extern PDUDataManager pduManager;

/* Flash write buffer */
extern uint16_t Buffer[WORDS_IN_FLASH_BUFFER];

/* Flash state tracking */
extern uint32_t addressToWrite;
extern uint16_t newestData;
extern uint16_t oldestData;
extern uint32_t writeAddress;
extern uint32_t readAddress;
extern bool emptySectorFound;
extern size_t datasize;

/* Flash operation status */
extern bool updateFailed;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/* Initialize PDU manager (zero both structures) */
void initializePDUManager(void);

/* Copy working data to read/write structure */
void syncWorkingToReadWrite(void);

/* Copy read/write data to working structure */
void syncReadWriteToWorking(void);

/* Update working data with new CalibData */
bool updateWorkingData(const CalibData* newData);

/**
 * @brief   Sync PDU working data to runtime g_calibration
 *          Converts uint16_t flash gains to float and sets g_calibration
 *          Call after loading data from flash
 */
void PDU_syncToCalibration(void);

/**
 * @brief   Sync runtime g_calibration back to PDU working data
 *          Converts float gains to uint16_t for flash storage
 *          Call before saving calibration to flash
 */
void PDU_syncFromCalibration(void);

/* CAN ACK functions for flash commands */
void CAN_sendErasedBank4Ack(void);
void CAN_sendWrittenDefaultsAck(void);
void CAN_sendCalibUpdateSuccessAck(void);
void CAN_sendCalibUpdateFailAck(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_ENABLED */

#endif /* SRC_PDU_MANAGER_H_ */
