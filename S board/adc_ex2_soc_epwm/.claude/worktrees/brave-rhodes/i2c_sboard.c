//
// #############################################################################
//
//  FILE:   tca9555_i2c_controller.c
//
//  TITLE:  TCA9555 I/O Expander Communication
//
// #############################################################################
//

#include "i2c_sboard.h"
//
// Globals
//
uint16_t outputValues[2] = {0x00, 0x00};  // Initial output values for PORT0 and PORT1
uint16_t inputValues[2] = {0x00, 0x00};   // Input values from PORT0 and PORT1
volatile uint16_t count_i2c = 0;              // General counter
volatile bool readInputs = false;         // Flag to trigger input reading

uint16_t count_read=0,count_write=0,count_int=0,count_int1=0,count_int2=0,count_int3=0,count_int4=0,count_int5=0,error_flag=0,error_flag1=0,error_flag2=0;


void i2c_main(void)
{

    // C2000Ware Library initialization
    C2000Ware_libraries_init();

    // Configure SDA pin
    GPIO_setPinConfig(GPIO_10_I2CA_SDA);
    GPIO_setPadConfig(I2CA_SDA_PIN, GPIO_PIN_TYPE_STD );
    GPIO_setQualificationMode(I2CA_SDA_PIN, GPIO_QUAL_ASYNC);

    // Configure SCL pin
    GPIO_setPinConfig(GPIO_9_I2CA_SCL);
    GPIO_setPadConfig(I2CA_SCL_PIN, GPIO_PIN_TYPE_STD );
    GPIO_setQualificationMode(I2CA_SCL_PIN, GPIO_QUAL_ASYNC);
    // Initialize I2C and Interrupts
    Pinmux_init();
    INTERRUPT_init();
    I2C_init();

    // Enable Global Interrupt (INTM) and real-time interrupt (DBGM)
    EINT;
    ERTM;

    // Initialize TCA9555
    TCA9555_init();


}



void i2c_loop ()
{
    if(error_flag==1)
          {  DEVICE_DELAY_US(10000);  // 100ms delay
          INTERRUPT_init_i2c();
          I2C_init();
          // Enable Global Interrupt (INTM) and real-time interrupt (DBGM)
          EINT;
          ERTM;
              TCA9555_init();
              error_flag=0;
          }

          // Toggle debug pin to monitor main loop execution
          GPIO_togglePin(DEBUG_PIN);

          // Write new output values
          TCA9555_writeOutputs();

          // Read input values
          TCA9555_readInputs();

          // Wait a bit before next cycle
          DEVICE_DELAY_US(100000);  // 100ms delay
}



//
// Initialize TCA9555 - Configure PORT0 as outputs and PORT1 as inputs
//
void TCA9555_init(void)
{
    // Ensure I2C module is ready
 //   while(I2C_getStatus(I2CA_BASE) & I2C_STS_BUS_BUSY);

    // Configure PORT0 as outputs (0x00 = all outputs)
    I2C_setConfig(I2CA_BASE, I2C_CONTROLLER_SEND_MODE);
    I2C_setTargetAddress(I2CA_BASE, TCA9555_ADDRESS);
    I2C_setDataCount(I2CA_BASE, 2);  // Command byte + 1 data byte
    I2C_sendStartCondition(I2CA_BASE);
    I2C_putData(I2CA_BASE, TCA9555_CONFIG_PORT0);  // Command byte
    I2C_putData(I2CA_BASE, 0x00);  // 0 = output, 1 = input
    I2C_sendStopCondition(I2CA_BASE);

    // Wait for transaction to complete
 //   while(I2C_getStatus(I2CA_BASE) & I2C_STS_BUS_BUSY);
    DEVICE_DELAY_US(10000);  // 1ms delay

    // Configure PORT1 as inputs (0xFF = all inputs)
    I2C_setConfig(I2CA_BASE, I2C_CONTROLLER_SEND_MODE);
    I2C_setTargetAddress(I2CA_BASE, TCA9555_ADDRESS);
    I2C_setDataCount(I2CA_BASE, 2);
    I2C_sendStartCondition(I2CA_BASE);
    I2C_putData(I2CA_BASE, TCA9555_CONFIG_PORT1);  // Command byte
    I2C_putData(I2CA_BASE, 0x00);  // 0 = output, 1 = input
    I2C_sendStopCondition(I2CA_BASE);

    // Wait for transaction to complete
  //  while(I2C_getStatus(I2CA_BASE) & I2C_STS_BUS_BUSY);
    DEVICE_DELAY_US(1000);  // 1ms delay

    // Initialize output values on PORT0
    TCA9555_writeOutputs();
}

//
// Write output values to TCA9555 PORT0
//
void TCA9555_writeOutputs(void)
{// Write to PORT0
    I2C_setConfig(I2CA_BASE, I2C_CONTROLLER_SEND_MODE);
    I2C_setTargetAddress(I2CA_BASE, TCA9555_ADDRESS);
    I2C_setDataCount(I2CA_BASE, 2);  // Command byte + 1 data byte
    I2C_sendStartCondition(I2CA_BASE);
    I2C_putData(I2CA_BASE, TCA9555_OUTPUT_PORT0);  // Command byte
    I2C_putData(I2CA_BASE, outputValues[0]);  // Data byte
    I2C_sendStopCondition(I2CA_BASE);

    DEVICE_DELAY_US(5000);  // 1ms delay

    // Write to PORT1
    I2C_setConfig(I2CA_BASE, I2C_CONTROLLER_SEND_MODE);
    I2C_setTargetAddress(I2CA_BASE, TCA9555_ADDRESS);
    I2C_setDataCount(I2CA_BASE, 2);  // Command byte + 1 data byte
    I2C_sendStartCondition(I2CA_BASE);
    I2C_putData(I2CA_BASE, TCA9555_OUTPUT_PORT1);  // Command byte
    I2C_putData(I2CA_BASE, outputValues[1]);  // Data byte
    I2C_sendStopCondition(I2CA_BASE);
    count_write++;
}

//
// Read input values from TCA9555 PORT0 and PORT1
// Note: Even though configured as outputs, we can still read the current state
//
void TCA9555_readInputs(void)
{
    // Read from PORT0
    I2C_setConfig(I2CA_BASE, I2C_CONTROLLER_SEND_MODE);
    I2C_setTargetAddress(I2CA_BASE, TCA9555_ADDRESS);
    I2C_setDataCount(I2CA_BASE, 1);  // Only sending command byte
    I2C_sendStartCondition(I2CA_BASE);
    I2C_putData(I2CA_BASE, TCA9555_INPUT_PORT0);  // Command byte
    I2C_sendStopCondition(I2CA_BASE);

    DEVICE_DELAY_US(5000);  // 1ms delay

    // Read the data from PORT0
    I2C_setConfig(I2CA_BASE, I2C_CONTROLLER_RECEIVE_MODE);
    I2C_setTargetAddress(I2CA_BASE, TCA9555_ADDRESS);
    I2C_setDataCount(I2CA_BASE, 1);  // Reading 1 byte
    I2C_sendStartCondition(I2CA_BASE);
    inputValues[0] = I2C_getData(I2CA_BASE);
    I2C_sendStopCondition(I2CA_BASE);

    DEVICE_DELAY_US(5000);  // 1ms delay

    // Read from PORT1
    I2C_setConfig(I2CA_BASE, I2C_CONTROLLER_SEND_MODE);
    I2C_setTargetAddress(I2CA_BASE, TCA9555_ADDRESS);
    I2C_setDataCount(I2CA_BASE, 1);  // Only sending command byte
    I2C_sendStartCondition(I2CA_BASE);
    I2C_putData(I2CA_BASE, TCA9555_INPUT_PORT1);  // Command byte
    I2C_sendStopCondition(I2CA_BASE);

    DEVICE_DELAY_US(5000);  // 1ms delay

    // Read the data from PORT1
    I2C_setConfig(I2CA_BASE, I2C_CONTROLLER_RECEIVE_MODE);
    I2C_setTargetAddress(I2CA_BASE, TCA9555_ADDRESS);
    I2C_setDataCount(I2CA_BASE, 1);  // Reading 1 byte
    I2C_sendStartCondition(I2CA_BASE);
    inputValues[1] = I2C_getData(I2CA_BASE);
    I2C_sendStopCondition(I2CA_BASE);
    count_read++;
}

//
// Toggle output values to create a pattern
//
void TCA9555_toggleOutputs(void)
{
    // Toggle all bits in PORT0
  //  outputValues[0] = ~outputValues[0];
}

//
// I2C Interrupt Handler
//
__interrupt void INT_I2CA_ISR(void)
{count_int++;
    uint16_t status;

    // Read the interrupt status
    status = I2C_getInterruptStatus(I2CA_BASE);
    count_int1++;
    // Handle interrupts based on status
    if(status & I2C_INT_TX_DATA_RDY)
    {count_int2++;
        I2C_clearInterruptStatus(I2CA_BASE, I2C_INT_TX_DATA_RDY);
    }

    if(status & I2C_INT_RX_DATA_RDY)
    {count_int3++;
        I2C_clearInterruptStatus(I2CA_BASE, I2C_INT_RX_DATA_RDY);
    }

    if(status & I2C_INT_STOP_CONDITION)
    {count_int4++;
        I2C_clearInterruptStatus(I2CA_BASE, I2C_INT_STOP_CONDITION);
    }

  //   Check for error conditions
    if(status & (I2C_INT_NO_ACK))
    {count_int5++;
        // Clear error flags
        I2C_clearInterruptStatus(I2CA_BASE, I2C_INT_NO_ACK);
        count_int5++;
        // Send stop condition to recover
        I2C_sendStopCondition(I2CA_BASE);
        count_int5++;
    }

    if(status & (I2C_INT_ARB_LOST ))
    {error_flag++;
        // Clear error flags
        I2C_clearInterruptStatus(I2CA_BASE, I2C_INT_ARB_LOST);
        error_flag1++;
        // Send stop condition to recover
        I2C_sendStopCondition(I2CA_BASE);
        error_flag2++;
    }
    // Ensure the interrupt keeps triggering
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP8);
}

//
// I2C Initialization
//
void I2C_init()
{
    I2C_disableModule(I2CA_BASE);

    // Initialize I2C in controller mode with 250kHz clock
    I2C_initController(I2CA_BASE, DEVICE_SYSCLK_FREQ, 250000, I2C_DUTYCYCLE_50);

    // Initial configuration (will be changed based on operation)
    I2C_setConfig(I2CA_BASE, I2C_CONTROLLER_SEND_MODE);
    I2C_setTargetAddress(I2CA_BASE, TCA9555_ADDRESS);
    I2C_setOwnAddress(I2CA_BASE, 0x00);

    // Set for 8-bit data
    I2C_setBitCount(I2CA_BASE, I2C_BITCOUNT_8);
    I2C_setAddressMode(I2CA_BASE, I2C_ADDR_MODE_7BITS);

    // No FIFO
    I2C_disableFIFO(I2CA_BASE);

    // Enable interrupts
    I2C_enableInterrupt(I2CA_BASE, I2C_INT_TX_DATA_RDY | I2C_INT_RX_DATA_RDY |
                       I2C_INT_STOP_CONDITION | I2C_INT_ARB_LOST | I2C_INT_NO_ACK);

    // Set emulation mode
    I2C_setEmulationMode(I2CA_BASE, I2C_EMULATION_STOP_SCL_LOW);

    // Enable I2C module
    I2C_enableModule(I2CA_BASE);
}

//
// Interrupt Setup
//
void INTERRUPT_init_i2c()
{
    // Disable I2C interrupt before configuration
    Interrupt_disable(INT_I2CA);

    // Register I2C interrupt handler
    Interrupt_register(INT_I2CA, &INT_I2CA_ISR);

    // Enable I2C interrupt
    Interrupt_enable(INT_I2CA);
}

//
// GPIO Configuration for I2C
//
void Pinmux_init()
{
    // Configure SDA pin
    GPIO_setPinConfig(GPIO_10_I2CA_SDA);
    GPIO_setPadConfig(I2CA_SDA_PIN, GPIO_PIN_TYPE_STD | GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(I2CA_SDA_PIN, GPIO_QUAL_ASYNC);

    // Configure SCL pin
    GPIO_setPinConfig(GPIO_9_I2CA_SCL);
    GPIO_setPadConfig(I2CA_SCL_PIN, GPIO_PIN_TYPE_STD | GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(I2CA_SCL_PIN, GPIO_QUAL_ASYNC);

    // Configure debug pin
    GPIO_setPinConfig(GPIO_20_GPIO20);
    GPIO_setDirectionMode(DEBUG_PIN, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(DEBUG_PIN, GPIO_PIN_TYPE_STD);
}
