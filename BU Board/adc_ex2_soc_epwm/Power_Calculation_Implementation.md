# Power Calculation Implementation with Ping-Pong Buffers

## Overview

This implementation provides real power calculation using ping-pong buffers for both voltage (from CAN) and current (from ADC/DMA) measurements.

```
Real Power Formula:
                    N-1
            1       Σ   V[i] × I[i]
    P_real = ─── × ────────────────
             N      i=0

Where N = 1280 samples (10 cycles × 128 samples/cycle)
```

---

## Files Modified/Created

| File | Action | Description |
|------|--------|-------------|
| `src/power_calc.h` | Created | Header file with definitions and function declarations |
| `src/power_calc.c` | Created | Implementation of ping-pong buffers and power calculation |
| `src/voltage_rx.h` | Modified | Added ping-pong buffer support declarations |
| `src/voltage_rx.c` | Modified | Added `VoltageRx_copyToPingPongBuffer()` function |
| `adc_ex2_soc_epwm.c` | Modified | Integrated ping-pong buffers and power calculation |

---

## Architecture

### Ping-Pong Buffer System

```
VOLTAGE BUFFERS (from CAN):
┌─────────────────────────┐     ┌─────────────────────────┐
│   g_voltageBuffer_A     │     │   g_voltageBuffer_B     │
│   int16_t[1280]         │     │   int16_t[1280]         │
│   (ramgs0 section)      │     │   (ramgs0 section)      │
└───────────┬─────────────┘     └───────────┬─────────────┘
            │                               │
            └───────────┬───────────────────┘
                        ▼
              g_activeVoltageBuffer (BUFFER_A or BUFFER_B)

CURRENT BUFFERS (from ADC/DMA):
┌─────────────────────────┐     ┌─────────────────────────┐
│   g_currentBuffer_A     │     │   g_currentBuffer_B     │
│   int16_t[1280]         │     │   int16_t[1280]         │
│   (ramgs1 section)      │     │   (ramgs1 section)      │
└───────────┬─────────────┘     └───────────┬─────────────┘
            │                               │
            └───────────┬───────────────────┘
                        ▼
              g_activeCurrentBuffer (BUFFER_A or BUFFER_B)
```

### Data Flow Per Cycle

```
Cycle N:
═══════════════════════════════════════════════════════════════════════════════

  ┌─────────────┐   ┌─────────────────┐   ┌─────────────────────────────────┐
  │   CAPTURE   │   │    PLAYBACK     │   │           CAN RX                │
  │   (DMA)     │──►│    (DAC)        │──►│   + POWER CALC                  │
  └─────────────┘   └─────────────────┘   └─────────────────────────────────┘
        │                                               │
        ▼                                               ▼
  ADC samples go to                             Voltage from CAN goes to
  myADC0Results[]                               ACTIVE voltage buffer
        │                                               │
        ▼                                               ▼
  Copy to ACTIVE                                Swap voltage buffer
  current buffer                                        │
        │                                               ▼
        ▼                                       Power calculation uses
  Swap current buffer                           INACTIVE buffers (cycle N-1)


Buffer State During Cycle N:
────────────────────────────────────────────────────────────────────────────────
                        │  Writing (Active)  │  Reading (Inactive)  │
────────────────────────┼────────────────────┼──────────────────────┤
  Voltage (CAN)         │  Buffer A          │  Buffer B (N-1 data) │
  Current (ADC)         │  Buffer A          │  Buffer B (N-1 data) │
────────────────────────┴────────────────────┴──────────────────────┘

After Swaps (Ready for Cycle N+1):
────────────────────────────────────────────────────────────────────────────────
                        │  Writing (Active)  │  Reading (Inactive)  │
────────────────────────┼────────────────────┼──────────────────────┤
  Voltage (CAN)         │  Buffer B          │  Buffer A (N data)   │
  Current (ADC)         │  Buffer B          │  Buffer A (N data)   │
────────────────────────┴────────────────────┴──────────────────────┘
```

---

## Main Loop Flow

```c
while(1)
{
    // STEP 1: Wait for sync pulse from S board
    while(firstpulse == false) { }

    // STEP 2: ADC/DMA Capture Phase
    // - DMA transfers 1280 samples to myADC0Results[]
    done = 0;
    DMA_startChannel(myDMA0_BASE);
    EPWM_enableADCTrigger(EPWM7_BASE, EPWM_SOC_A);
    while(done == 0 && firstpulse == true) { }

    // STEP 3: Copy current to ping-pong buffer
    PowerCalc_copyCurrentFromADC(myADC0Results, RESULTS_BUFFER_SIZE);
    PowerCalc_swapCurrentBuffer();

    // STEP 4: Playback Phase (DAC output)
    done = 0;
    isPlaybackMode = 1;
    EPWM_enableADCTrigger(EPWM7_BASE, EPWM_SOC_A);
    while(done == 0 && firstpulse == true) { }

    // STEP 5: CAN RX Phase
    while(g_canRxComplete == false && firstpulse == true) {
        if(g_framesReady) {
            VoltageRx_copyToPingPongBuffer();  // Copies and swaps voltage buffer
            g_framesReady = false;
        }
    }

    // STEP 6: Power Calculation (using inactive buffers = previous cycle data)
    if(g_powerCalcEnabled && g_cycleCount > 0) {
        PowerCalc_calculateRealPower();
        g_lastRealPower = PowerCalc_getRealPower_raw();
    }

    g_cycleCount++;
}
```

---

## API Reference

### Power Calculation Module (`power_calc.h`)

#### Initialization
```c
void PowerCalc_init(void);     // Initialize module and clear buffers
void PowerCalc_reset(void);    // Reset status flags
```

#### Buffer Access
```c
volatile int16_t* PowerCalc_getActiveVoltageBuffer(void);    // Get write buffer
volatile int16_t* PowerCalc_getInactiveVoltageBuffer(void);  // Get read buffer
volatile int16_t* PowerCalc_getActiveCurrentBuffer(void);    // Get write buffer
volatile int16_t* PowerCalc_getInactiveCurrentBuffer(void);  // Get read buffer
```

#### Buffer Management
```c
void PowerCalc_swapVoltageBuffer(void);  // Swap active voltage buffer
void PowerCalc_swapCurrentBuffer(void);  // Swap active current buffer
```

#### Power Calculation
```c
void PowerCalc_calculateRealPower(void);     // Calculate P = avg(V*I)
int32_t PowerCalc_getRealPower_raw(void);    // Get result in ADC units
float PowerCalc_getRealPower_watts(void);    // Get result in Watts (scaled)
```

#### Data Copy
```c
void PowerCalc_copyCurrentFromADC(const uint16_t *adcBuffer, uint16_t count);
// Copies ADC values with offset removal (0-4095 → -2048 to +2047)
```

#### Status
```c
bool PowerCalc_isDataReady(void);      // Check if new calculation available
void PowerCalc_clearDataReady(void);   // Clear the ready flag
```

### Voltage RX Module (Updated)
```c
void VoltageRx_copyToPingPongBuffer(void);  // Copy CAN data to ping-pong buffer
```

---

## Memory Layout

```
RAM Section Allocation:
────────────────────────────────────────────────────────────────────────────────
Section      │ Size    │ Contents
─────────────┼─────────┼────────────────────────────────────────────────────────
ramgs0       │ 5120B   │ g_voltageBuffer_A[1280] + g_voltageBuffer_B[1280]
ramgs1       │ 5120B   │ g_currentBuffer_A[1280] + g_currentBuffer_B[1280]
.TI.ramfunc  │ varies  │ PowerCalc_calculateRealPower()
             │         │ PowerCalc_copyCurrentFromADC()
             │         │ VoltageRx_copyToPingPongBuffer()
────────────────────────────────────────────────────────────────────────────────
Total: ~10KB for ping-pong buffers
```

---

## Overflow Prevention

```
Power Calculation Overflow Analysis:
────────────────────────────────────────────────────────────────────────────────
Per sample:
    V_max = 2048 (12-bit half range, signed)
    I_max = 2048 (12-bit half range, signed)
    V × I = 2048 × 2048 = 4,194,304  → fits in int32_t

Accumulator for 1280 samples:
    Max sum = 4,194,304 × 1280 = 5,368,709,120
    int32_t max = 2,147,483,647  → OVERFLOW!
    int64_t max = 9.2 × 10^18   → SAFE ✓

Solution: int64_t accumulator used in PowerCalc_calculateRealPower()
────────────────────────────────────────────────────────────────────────────────
```

---

## Scaling to Real Units

To convert raw ADC units to actual Watts, configure the scaling factors in `power_calc.h`:

```c
// Example for typical sensors:
// Voltage: 230V RMS → 3.3V ADC (via PT + offset circuit)
// Current: 30A RMS → 3.3V ADC (via CT/ACS712 + offset circuit)

#define VOLTAGE_SCALE_FACTOR    (0.175f)   // Adjust based on your PT ratio
#define CURRENT_SCALE_FACTOR    (0.023f)   // Adjust based on your CT ratio
#define POWER_SCALE_FACTOR      (VOLTAGE_SCALE_FACTOR * CURRENT_SCALE_FACTOR)

// P_watts = P_raw * POWER_SCALE_FACTOR
```

---

## Watch Variables for Debugging

| Variable | Type | Description |
|----------|------|-------------|
| `g_voltageBuffer_A[]` | int16_t[1280] | Voltage ping buffer |
| `g_voltageBuffer_B[]` | int16_t[1280] | Voltage pong buffer |
| `g_currentBuffer_A[]` | int16_t[1280] | Current ping buffer |
| `g_currentBuffer_B[]` | int16_t[1280] | Current pong buffer |
| `g_activeVoltageBuffer` | enum | Active voltage buffer index |
| `g_activeCurrentBuffer` | enum | Active current buffer index |
| `g_powerResults.realPower_raw` | int32_t | Last calculated power (raw) |
| `g_powerResults.accumulator` | int64_t | Raw accumulator value |
| `g_lastRealPower` | int32_t | Copy of last power for monitoring |
| `g_cycleCount` | uint32_t | Number of complete cycles |
| `g_powerStatus.cycleCount` | uint32_t | Power calculation cycles |

---

## Notes

1. **First Cycle**: Power calculation is skipped on cycle 0 (g_cycleCount == 0) since inactive buffers contain uninitialized data.

2. **One-Cycle Delay**: Power calculation uses data from the previous cycle (inactive buffers). This is inherent to the ping-pong design.

3. **RAM Execution**: Critical functions run from RAM (`.TI.ramfunc`) for maximum speed.

4. **Offset Removal**: Both voltage and current are converted to signed int16_t with -2048 offset for proper AC signal representation.

5. **Scaling**: Default scaling factors are 1.0 (raw ADC units). Adjust based on your sensor calibration for Watts output.
