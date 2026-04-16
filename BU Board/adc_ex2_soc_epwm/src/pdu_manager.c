/*
 * pdu_manager.c
 *
 * PDU Data Manager Implementation
 *
 * Manages calibration data structures for flash read/write and runtime use.
 * Provides bridge functions between flash storage (uint16_t) and
 * runtime calibration (float).
 *
 * Also provides CAN ACK functions for flash commands.
 *
 *  Created on: Feb 19, 2026
 *      Author: Parthasarathy.M
 */

#include "can_bu.h"

#ifdef CAN_ENABLED

#include "pdu_manager.h"

/* ========================================================================== */
/*                          Global Variables                                  */
/* ========================================================================== */

PDUDataManager pduManager;

/* Flash write buffer */
uint16_t Buffer[WORDS_IN_FLASH_BUFFER];

/* Flash state tracking */
uint32_t addressToWrite = 0xFFFFFFFF;
uint16_t newestData = 0;
uint16_t oldestData = 0xFFFF;
uint32_t writeAddress = 0xFFFFFFFF;
uint32_t readAddress = BANK4_START;
bool emptySectorFound = false;
size_t datasize = 0;

/* Flash operation status */
bool updateFailed = false;

/* ========================================================================== */
/*                       PDU Manager Functions                                */
/* ========================================================================== */

void initializePDUManager(void)
{
    memset(&pduManager.readWriteData, 0, sizeof(CalibData));
    memset(&pduManager.workingData, 0, sizeof(CalibData));
    pduManager.dataSync = true;
}

void syncWorkingToReadWrite(void)
{
    memcpy(&pduManager.readWriteData, &pduManager.workingData,
           sizeof(CalibData));
    pduManager.dataSync = true;
}

void syncReadWriteToWorking(void)
{
    memcpy(&pduManager.workingData, &pduManager.readWriteData,
           sizeof(CalibData));
    pduManager.dataSync = true;
}

bool updateWorkingData(const CalibData* newData)
{
    if (newData == NULL)
    {
        return false;
    }

    memcpy(&pduManager.workingData, newData, sizeof(CalibData));
    pduManager.dataSync = false;
    return true;
}

/* ========================================================================== */
/*                    Calibration Bridge Functions                             */
/* ========================================================================== */

void PDU_syncToCalibration(void)
{
    uint16_t i;

    for (i = 0; i < CAL_NUM_CTS; i++)
    {
        float cGain = (float)pduManager.workingData.currentGain[i] / CAL_GAIN_SCALE;
        float kGain = (float)pduManager.workingData.kwGain[i] / CAL_GAIN_SCALE;
        Calibration_setCurrentGain(i + 1U, cGain);
        Calibration_setKWGain(i + 1U, kGain);
    }
}

void PDU_syncFromCalibration(void)
{
    uint16_t i;

    for (i = 0; i < CAL_NUM_CTS; i++)
    {
        pduManager.workingData.currentGain[i] =
            (uint16_t)(Calibration_getCurrentGain(i + 1U) * CAL_GAIN_SCALE);
        pduManager.workingData.kwGain[i] =
            (uint16_t)(Calibration_getKWGain(i + 1U) * CAL_GAIN_SCALE);
    }

    pduManager.workingData.BUBoardID = address;
    pduManager.dataSync = false;
}

/* ========================================================================== */
/*                    CAN ACK Functions for Flash Commands                     */
/* ========================================================================== */

/*
 * Helper: Send a CAN-FD ACK frame on ID (10 + address)
 * ackByte0: First data byte (command echo)
 * ackByte1: Second data byte (status: 0x01 = success)
 */
static void sendFlashAck(uint16_t ackByte0, uint16_t ackByte1)
{
    MCAN_TxBufElement TxMsg;
    uint16_t i;

    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id  = ((uint32_t)(10U + address)) << 18U;
    TxMsg.brs = 0x1U;
    TxMsg.dlc = 15U;
    TxMsg.rtr = 0U;
    TxMsg.xtd = 0U;
    TxMsg.fdf = 0x1U;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xAAU;

    TxMsg.data[0] = ackByte0;
    TxMsg.data[1] = ackByte1;
    for (i = 2; i < 64; i++)
    {
        TxMsg.data[i] = 0;
    }

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0U, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);
    canTxWaitComplete();
}

void CAN_sendErasedBank4Ack(void)
{
    /* ACK: [0x09, 0x01] = Bank 4 erased successfully */
    sendFlashAck(0x09U, 0x01U);
}

void CAN_sendWrittenDefaultsAck(void)
{
    /* ACK: [0x0A, 0x01] = Default calibration values written */
    sendFlashAck(0x0AU, 0x01U);
}

void CAN_sendCalibUpdateSuccessAck(void)
{
    /* ACK: [0x04, 0x01] = Calibration data updated successfully */
    sendFlashAck(0x04U, 0x01U);
}

void CAN_sendCalibUpdateFailAck(void)
{
    /* ACK: [0x04, 0x00] = Calibration data update failed */
    sendFlashAck(0x04U, 0x00U);
}

#endif /* CAN_ENABLED */
