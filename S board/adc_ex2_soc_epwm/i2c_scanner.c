//
//#############################################################################
//
// FILE:   i2c_scanner.c
//
// TITLE:  I2C Bus Scanner Module - Implementation (16-bit Support)
//
//#############################################################################
//

#include "i2c_scanner.h"

//
// Private state
//

static I2C_ScanResult scanResults[I2C_SCANNER_MAX_RESULTS];
static uint16_t       devicesFound = 0;
static uint16_t       resultCount  = 0;
static volatile uint16_t isrNackFlag = 0;

//
// Internal helpers
//

static void waitBusReady(void)
{
    uint16_t timeout = 0;
    while ((I2C_getStatus(I2C_SCANNER_BASE) & I2C_STS_BUS_BUSY) &&
           (timeout < 5000))
    {
        DEVICE_DELAY_US(1);
        timeout++;
    }
}

static void clearAllFlags(void)
{
    isrNackFlag = 0;
    I2C_clearInterruptStatus(I2C_SCANNER_BASE,
        I2C_INT_TX_DATA_RDY | I2C_INT_RX_DATA_RDY |
        I2C_INT_STOP_CONDITION | I2C_INT_ARB_LOST | I2C_INT_NO_ACK);
}

//
// Returns true if a NACK was detected (ISR flag or status register).
//
static bool nackDetected(void)
{
    return (isrNackFlag != 0) ||
           ((I2C_getStatus(I2C_SCANNER_BASE) & I2C_STS_NO_ACK) != 0);
}

//
// Pin mux (internal)
//

static void configurePinMux(void)
{
    // SDA
    GPIO_setPinConfig(I2C_SCANNER_SDA_PINCONFIG);
    GPIO_setPadConfig(I2C_SCANNER_SDA_PIN,
                      GPIO_PIN_TYPE_STD | GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(I2C_SCANNER_SDA_PIN, GPIO_QUAL_ASYNC);

    // SCL
    GPIO_setPinConfig(I2C_SCANNER_SCL_PINCONFIG);
    GPIO_setPadConfig(I2C_SCANNER_SCL_PIN,
                      GPIO_PIN_TYPE_STD | GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(I2C_SCANNER_SCL_PIN, GPIO_QUAL_ASYNC);
}

//
// Public API
//

void I2C_Scanner_init(void)
{
    // Pin mux
    configurePinMux();

    // Interrupt
    Interrupt_disable(I2C_SCANNER_INT_NUMBER);
    Interrupt_register(I2C_SCANNER_INT_NUMBER, &I2C_Scanner_ISR);
    Interrupt_enable(I2C_SCANNER_INT_NUMBER);

    // I2C peripheral
    I2C_disableModule(I2C_SCANNER_BASE);
    I2C_initController(I2C_SCANNER_BASE, DEVICE_SYSCLK_FREQ,
                       I2C_SCANNER_CLOCK_HZ, I2C_DUTYCYCLE_50);
    I2C_setConfig(I2C_SCANNER_BASE, I2C_CONTROLLER_SEND_MODE);
    I2C_setOwnAddress(I2C_SCANNER_BASE, 0x00);
    I2C_setBitCount(I2C_SCANNER_BASE, I2C_BITCOUNT_8);
    I2C_setAddressMode(I2C_SCANNER_BASE, I2C_ADDR_MODE_7BITS);

    // Note: FIFO is disabled. We poll RX_DATA_RDY in the read loop.
    I2C_disableFIFO(I2C_SCANNER_BASE);

    I2C_enableInterrupt(I2C_SCANNER_BASE,
                        I2C_INT_ARB_LOST | I2C_INT_NO_ACK);
    I2C_setEmulationMode(I2C_SCANNER_BASE, I2C_EMULATION_STOP_SCL_LOW);
    I2C_enableModule(I2C_SCANNER_BASE);
}

uint16_t I2C_Scanner_checkBusIdle(void)
{
    // Temporarily switch to GPIO inputs to read line levels
    GPIO_setPinConfig(I2C_SCANNER_SDA_PINCONFIG_GPIO);
    GPIO_setDirectionMode(I2C_SCANNER_SDA_PIN, GPIO_DIR_MODE_IN);
    uint16_t sda = GPIO_readPin(I2C_SCANNER_SDA_PIN);

    GPIO_setPinConfig(I2C_SCANNER_SCL_PINCONFIG_GPIO);
    GPIO_setDirectionMode(I2C_SCANNER_SCL_PIN, GPIO_DIR_MODE_IN);
    uint16_t scl = GPIO_readPin(I2C_SCANNER_SCL_PIN);

    // Restore I2C pin mux
    configurePinMux();

    return (scl << 1) | sda;  // 0x03 = both high = idle
}

bool I2C_Scanner_probeDevice(uint16_t address, uint16_t regAddr,
                             I2C_ScanResult *result)
{
    result->address       = address;
    result->responded     = false;
    result->nack_received = false;
    result->write_ok      = 0;
    result->read_value    = 0xFFFF; // Default error value

    clearAllFlags();
    waitBusReady();

    //
    // Phase 1: Write-probe - send one byte (register address).
    //
    I2C_setConfig(I2C_SCANNER_BASE, I2C_CONTROLLER_SEND_MODE);
    I2C_setTargetAddress(I2C_SCANNER_BASE, address);
    I2C_setDataCount(I2C_SCANNER_BASE, 1);
    I2C_sendStartCondition(I2C_SCANNER_BASE);

    DEVICE_DELAY_US(1000);  // wait for address phase

    if (nackDetected())
    {
        result->nack_received = true;
        I2C_clearInterruptStatus(I2C_SCANNER_BASE, I2C_INT_NO_ACK);
        I2C_sendStopCondition(I2C_SCANNER_BASE);
        DEVICE_DELAY_US(2000);
        return false;
    }

    // Send the register byte
    I2C_putData(I2C_SCANNER_BASE, regAddr);

    // Wait for TX ready (or NACK)
    {
        uint16_t txTimeout = 0;
        while (!(I2C_getStatus(I2C_SCANNER_BASE) & I2C_STS_REG_ACCESS_RDY) &&
               !nackDetected() && (txTimeout < 5000))
        {
            DEVICE_DELAY_US(1);
            txTimeout++;
        }
    }

    if (nackDetected())
    {
        result->nack_received = true;
        I2C_clearInterruptStatus(I2C_SCANNER_BASE, I2C_INT_NO_ACK);
        I2C_sendStopCondition(I2C_SCANNER_BASE);
        DEVICE_DELAY_US(2000);
        return false;
    }

    result->write_ok  = 1;
    result->responded = true;

    // Stop before read (standard probe) or omit for Repeated Start.
    // We'll use Repeated Start logic below.
    I2C_sendStopCondition(I2C_SCANNER_BASE);
    DEVICE_DELAY_US(2000);

    //
    // Phase 2: Read-back (16-bit)
    //
    clearAllFlags();
    waitBusReady();

    // 1. Write register pointer again (for Repeated Start flow)
    I2C_setConfig(I2C_SCANNER_BASE, I2C_CONTROLLER_SEND_MODE);
    I2C_setTargetAddress(I2C_SCANNER_BASE, address);
    I2C_setDataCount(I2C_SCANNER_BASE, 1);
    I2C_sendStartCondition(I2C_SCANNER_BASE);

    // Wait for address
    DEVICE_DELAY_US(500);

    if (nackDetected())
    {
        I2C_sendStopCondition(I2C_SCANNER_BASE);
        return true; // Write succeeded previously, so we return true
    }

    I2C_putData(I2C_SCANNER_BASE, regAddr);

    {
        uint16_t txTimeout = 0;
        while (!(I2C_getStatus(I2C_SCANNER_BASE) & I2C_STS_REG_ACCESS_RDY) &&
               !nackDetected() && (txTimeout < 5000))
        {
            DEVICE_DELAY_US(1);
            txTimeout++;
        }
    }

    if (nackDetected())
    {
        I2C_clearInterruptStatus(I2C_SCANNER_BASE, I2C_INT_NO_ACK);
        I2C_sendStopCondition(I2C_SCANNER_BASE);
        return true;
    }

    // 2. Repeated-START read of TWO bytes
    clearAllFlags();

    I2C_setConfig(I2C_SCANNER_BASE, I2C_CONTROLLER_RECEIVE_MODE);
    I2C_setTargetAddress(I2C_SCANNER_BASE, address);

    // --- CHANGE: Set count to 2 ---
    I2C_setDataCount(I2C_SCANNER_BASE, 2);

    I2C_sendStartCondition(I2C_SCANNER_BASE);
    // Send STOP immediately: Controller will stop automatically after Count (2) reaches 0
    I2C_sendStopCondition(I2C_SCANNER_BASE);

    uint16_t lowByte = 0;
    uint16_t highByte = 0;
    bool readFailed = false;

    // --- Wait for Byte 1 (LSB) ---
    {
        uint16_t rxTimeout = 0;
        while (!(I2C_getStatus(I2C_SCANNER_BASE) & I2C_STS_RX_DATA_RDY))
        {
            if (nackDetected() || (rxTimeout > 10000)) {
                readFailed = true;
                break;
            }
            DEVICE_DELAY_US(1);
            rxTimeout++;
        }
    }

    if (!readFailed)
    {
        lowByte = I2C_getData(I2C_SCANNER_BASE); // Read LSB

        // --- Wait for Byte 2 (MSB) ---
        uint16_t rxTimeout = 0;
        while (!(I2C_getStatus(I2C_SCANNER_BASE) & I2C_STS_RX_DATA_RDY))
        {
            if (nackDetected() || (rxTimeout > 10000)) {
                readFailed = true;
                break;
            }
            DEVICE_DELAY_US(1);
            rxTimeout++;
        }

        if (!readFailed)
        {
            highByte = I2C_getData(I2C_SCANNER_BASE); // Read MSB
            // Combine bytes
            result->read_value = (highByte << 8) | lowByte;
        }
    }

    DEVICE_DELAY_US(2000);

    return true;
}

uint16_t I2C_Scanner_scanBus(uint16_t startAddr, uint16_t endAddr,
                             uint16_t regAddr)
{
    uint16_t addr;
    uint16_t idx = 0;

    devicesFound = 0;
    resultCount  = 0;

    for (addr = startAddr; addr < endAddr && idx < I2C_SCANNER_MAX_RESULTS; addr++)
    {
        bool found = I2C_Scanner_probeDevice(addr, regAddr, &scanResults[idx]);

        if (found)
        {
            devicesFound++;
        }

        idx++;
        DEVICE_DELAY_US(5000);  // 5 ms between addresses
    }

    resultCount = idx;
    return devicesFound;
}

const I2C_ScanResult* I2C_Scanner_getResults(void)
{
    return scanResults;
}

uint16_t I2C_Scanner_getDevicesFound(void)
{
    return devicesFound;
}

//
// ISR
//

__interrupt void I2C_Scanner_ISR(void)
{
    uint16_t status = I2C_getInterruptStatus(I2C_SCANNER_BASE);

    if (status & I2C_INT_NO_ACK)
    {
        isrNackFlag = 1;
        I2C_clearInterruptStatus(I2C_SCANNER_BASE, I2C_INT_NO_ACK);
    }

    if (status & I2C_INT_ARB_LOST)
    {
        I2C_clearInterruptStatus(I2C_SCANNER_BASE, I2C_INT_ARB_LOST);
    }

    I2C_clearInterruptStatus(I2C_SCANNER_BASE, status);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP8);
}
