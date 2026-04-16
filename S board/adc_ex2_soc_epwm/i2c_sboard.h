//
// #############################################################################
//
//  FILE:   tca9555_i2c_controller.c
//
//  TITLE:  TCA9555 I/O Expander Communication
//
// #############################################################################
//
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "c2000ware_libraries.h"

//
// Defines for TCA9555
//
#define TCA9555_ADDRESS        (0x27)  // I2C address (may vary based on A0, A1, A2 pins)
#define TCA9555_INPUT_PORT0    (0x00)  // Input port 0 register
#define TCA9555_INPUT_PORT1    (0x01)  // Input port 1 register
#define TCA9555_OUTPUT_PORT0   (0x02)  // Output port 0 register
#define TCA9555_OUTPUT_PORT1   (0x03)  // Output port 1 register
#define TCA9555_POLARITY_PORT0 (0x04)  // Polarity inversion port 0 register
#define TCA9555_POLARITY_PORT1 (0x05)  // Polarity inversion port 1 register
#define TCA9555_CONFIG_PORT0   (0x06)  // Configuration port 0 register
#define TCA9555_CONFIG_PORT1   (0x07)  // Configuration port 1 register

#define I2CA_SDA_PIN           (10)    // GPIO pin for I2C SDA
#define I2CA_SCL_PIN           (9)     // GPIO pin for I2C SCL
#define DEBUG_PIN              (29)    // GPIO pin for debug

//
// Globals
//
extern uint16_t outputValues[2] ;  // Initial output values for PORT0 and PORT1
extern uint16_t inputValues[2] ;   // Input values from PORT0 and PORT1
extern volatile uint16_t count_i2c ;              // General counter
extern volatile bool readInputs ;         // Flag to trigger input reading

extern uint16_t count_read,count_write,count_int,count_int1,count_int2,count_int3,count_int4,count_int5,error_flag,error_flag1,error_flag2;
//
// Function Prototypes
//
__interrupt void INT_I2CA_ISR(void);
void INTERRUPT_init_i2c(void);
void I2C_init(void);
void Pinmux_init(void);
void TCA9555_init(void);
void TCA9555_writeOutputs(void);
void TCA9555_readInputs(void);
void TCA9555_toggleOutputs(void);
void i2c_main();
void i2c_loop();

