//
//#############################################################################
//
// FILE:   i2c_scanner.h
//
// TITLE:  I2C Bus Scanner Module - Public API
//
// DESCRIPTION:
//   Reusable I2C bus scanner for C2000 F28P55x.
//   - Scans a configurable address range.
//   - Detects devices via write-probe.
//   - Reads back a 16-bit register value (2 bytes) from each device found.
//
//   Usage:
//     1. Call I2C_Scanner_init() once after Device_init / Board_init.
//     2. Call I2C_Scanner_scanBus() to scan an address range.
//        - Or call I2C_Scanner_probeDevice() to test a single address.
//     3. Inspect results via I2C_Scanner_getResults() / getDevicesFound().
//
//#############################################################################
//

#ifndef I2C_SCANNER_H
#define I2C_SCANNER_H

#include "driverlib.h"
#include "device.h"

//
// Configuration - override before #include if needed
//

#ifndef I2C_SCANNER_BASE
#define I2C_SCANNER_BASE        I2CA_BASE
#endif

#ifndef I2C_SCANNER_SDA_PIN
#define I2C_SCANNER_SDA_PIN     10
#endif

#ifndef I2C_SCANNER_SCL_PIN
#define I2C_SCANNER_SCL_PIN     9
#endif

#ifndef I2C_SCANNER_SDA_PINCONFIG
#define I2C_SCANNER_SDA_PINCONFIG   GPIO_10_I2CA_SDA
#endif

#ifndef I2C_SCANNER_SCL_PINCONFIG
#define I2C_SCANNER_SCL_PINCONFIG   GPIO_9_I2CA_SCL
#endif

#ifndef I2C_SCANNER_SDA_PINCONFIG_GPIO
#define I2C_SCANNER_SDA_PINCONFIG_GPIO   GPIO_10_GPIO10
#endif

#ifndef I2C_SCANNER_SCL_PINCONFIG_GPIO
#define I2C_SCANNER_SCL_PINCONFIG_GPIO   GPIO_9_GPIO9
#endif

#ifndef I2C_SCANNER_CLOCK_HZ
#define I2C_SCANNER_CLOCK_HZ    250000
#endif

// Maximum number of results that can be stored.
#ifndef I2C_SCANNER_MAX_RESULTS
#define I2C_SCANNER_MAX_RESULTS 128
#endif

#ifndef I2C_SCANNER_INT_NUMBER
#define I2C_SCANNER_INT_NUMBER  INT_I2CA
#endif

//
// Types
//

typedef struct {
    uint16_t address;
    bool     responded;
    bool     nack_received;
    uint16_t write_ok;       // 1 if write-probe was ACKed
    uint16_t read_value;     // 16-bit value read back (0xFFFF = error)
} I2C_ScanResult;

//
// Public API
//

//
// I2C_Scanner_init
//   Configures pin mux, I2C peripheral, and registers the NACK/ARB-LOST ISR.
//   Call once after Device_init() / Board_init().
//
void I2C_Scanner_init(void);

//
// I2C_Scanner_checkBusIdle
//   Temporarily switches SDA/SCL pins to GPIO inputs and reads their state.
//   Returns: bit 1 = SCL, bit 0 = SDA.  0x03 means both high (bus idle).
//   Restores I2C pin mux before returning.
//
uint16_t I2C_Scanner_checkBusIdle(void);

//
// I2C_Scanner_probeDevice
//   Tests whether a single 7-bit address responds.
//   Writes one byte (regAddr) then reads TWO bytes (16-bit) back.
//   Populates *result with outcome.
//   Returns true if the device ACKed the Write phase.
//
bool I2C_Scanner_probeDevice(uint16_t address, uint16_t regAddr,
                             I2C_ScanResult *result);

//
// I2C_Scanner_scanBus
//   Scans addresses [startAddr, endAddr) inclusive-exclusive.
//   Results are stored internally  - retrieve with getResults / getDevicesFound.
//   regAddr is the register byte sent during each probe.
//   Returns the number of devices that responded.
//
uint16_t I2C_Scanner_scanBus(uint16_t startAddr, uint16_t endAddr,
                             uint16_t regAddr);

//
// I2C_Scanner_getResults
//   Returns a pointer to the internal results array.
//   Valid entries: 0 .. (endAddr - startAddr - 1) from the last scanBus call.
//
const I2C_ScanResult* I2C_Scanner_getResults(void);

//
// I2C_Scanner_getDevicesFound
//   Returns the count of devices that ACKed during the last scanBus call.
//
uint16_t I2C_Scanner_getDevicesFound(void);

//
// I2C_Scanner_ISR
//   NACK / arbitration-lost interrupt handler.
//
__interrupt void I2C_Scanner_ISR(void);

#endif // I2C_SCANNER_H
