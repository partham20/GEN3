/**
 * @file runtime.c
 * @brief Runtime voltage buffer transmission to BU boards implementation
 */

#include "runtime.h"
#include "s_board_adc.h"
#include "can_driver.h"
#include "bu_board.h"
#include <math.h>
#include "metro.h"

// Global variables
RuntimeVoltageTransmission runtimeData;

/**
 * @brief Initialize runtime voltage transmission
 */
void initRuntimeVoltageTransmission(void)
{
    memset(&runtimeData, 0, sizeof(RuntimeVoltageTransmission));
    runtimeData.transmissionInProgress = false;
    runtimeData.transmissionComplete = false;
    runtimeData.currentPhase = 0;
    runtimeData.currentFrame = 0;
    runtimeData.transmissionDelay = 0;
    

    // Fill buffers with dummy sine wave values
    //fillBuffersWithSineWave();


}

/**
 * @brief Fill voltage buffers with dummy sine wave values
 */


void processRuntimeTransmission(void)
{
    if (!runtimeData.transmissionInProgress) {
        return;
    }

    // Add delay counter for non-blocking operation
    if (runtimeData.transmissionDelay > 0) {
        runtimeData.transmissionDelay--;
        return;
    }

    // Determine which buffer to use based on current phase
    uint16_t *currentBuffer;
    switch (runtimeData.currentPhase)
    {
        case PHASE_R:
            currentBuffer = runtimeData.rPhaseBuffer;
            break;
        case PHASE_Y:
            currentBuffer = runtimeData.yPhaseBuffer;
            break;
        case PHASE_B:
            currentBuffer = runtimeData.bPhaseBuffer;
            break;
        default:
            runtimeData.transmissionInProgress = false;
            return;
    }

    // Calculate start index for current frame
    uint16_t startIndex = (runtimeData.currentFrame - 1) * VALUES_PER_FRAME;

    // Send current frame
  //  sendSingleVoltageFrame(runtimeData.currentPhase, runtimeData.currentFrame, currentBuffer, startIndex);

    // Move to next frame
    runtimeData.currentFrame++;
    if (runtimeData.currentFrame > FRAMES_PER_PHASE)
    {
        // Move to next phase
        runtimeData.currentFrame = 1;
        if (runtimeData.currentPhase == PHASE_R) {
            runtimeData.currentPhase = PHASE_Y;
        } else if (runtimeData.currentPhase == PHASE_Y) {
            runtimeData.currentPhase = PHASE_B;
        } else {
            // All phases completed
            runtimeData.transmissionInProgress = false;
            runtimeData.transmissionComplete = true;
        }
    }

    // Set delay for next transmission
    runtimeData.transmissionDelay = 10; // Approximately 100us delay
}

/**
 * @brief Check if runtime transmission is complete
 * @return true if transmission is complete, false otherwise
 */
bool isRuntimeTransmissionComplete(void)
{
    return runtimeData.transmissionComplete;
}

/**
 * @brief Reset runtime transmission state
 */
void resetRuntimeTransmission(void)
{
    runtimeData.transmissionInProgress = false;
    runtimeData.transmissionComplete = false;
    runtimeData.currentPhase = 0;
    runtimeData.currentFrame = 0;
    runtimeData.transmissionDelay = 0;
}

/**
 * @brief Update voltage buffers with new data (to be called from ADC ISR or main loop)
 * @param rBuffer Pointer to R phase buffer
 * @param yBuffer Pointer to Y phase buffer
 * @param bBuffer Pointer to B phase buffer
 */
void updateVoltageBuffers(uint16_t *rBuffer, uint16_t *yBuffer, uint16_t *bBuffer)
{
    if (!runtimeData.transmissionInProgress) {
        memcpy(runtimeData.rPhaseBuffer, rBuffer, VOLTAGE_BUFFER_SIZE * sizeof(uint16_t));
        memcpy(runtimeData.yPhaseBuffer, yBuffer, VOLTAGE_BUFFER_SIZE * sizeof(uint16_t));
        memcpy(runtimeData.bPhaseBuffer, bBuffer, VOLTAGE_BUFFER_SIZE * sizeof(uint16_t));
    }}
