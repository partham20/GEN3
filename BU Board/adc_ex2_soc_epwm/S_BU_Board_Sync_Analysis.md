# S Board and BU Board Synchronization Analysis

## Desired Flow

```
TIME ──────────────────────────────────────────────────────────────────────────►

S BOARD:    ┌─────────┐   ┌─────────┐   ┌──────────┐   ┌─────────────┐
            │ SYNC TX │──►│ CAPTURE │──►│ PLAYBACK │──►│ CAN TX      │
            │ (GPIO)  │   │ (DMA)   │   │ (DAC)    │   │ (42 frames) │
            └─────────┘   └─────────┘   └──────────┘   └─────────────┘
                 │                                            │
                 │ GPIO28                                     │ CAN ID 5
                 ▼                                            ▼
BU BOARD:   ┌─────────┐   ┌─────────┐   ┌──────────┐   ┌─────────────┐
            │ SYNC RX │──►│ CAPTURE │──►│ PLAYBACK │──►│ CAN RX      │
            │ (GPIO)  │   │ (DMA)   │   │ (DAC)    │   │ (store)     │
            └─────────┘   └─────────┘   └──────────┘   └─────────────┘
```

---

## Current Issues

| Board | Current Flow | Problem |
|-------|--------------|---------|
| S Board | Capture → Playback → CAN TX | Missing sync pulse output |
| BU Board | Sync → Capture → **CAN RX** → Playback | CAN RX is before playback (causes 54ms lag) |

---

## Changes Needed

### S Board Changes

#### 1. Add GPIO for sync pulse output

Add to initialization (after `Board_init()` or in a new function):

```c
// Add to Board_init() or after it
void initSyncGPIO(void)
{
    // Configure GPIO29 as output for sync pulse to BU board
    GPIO_setPinConfig(GPIO_29_GPIO29);
    GPIO_setDirectionMode(29, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(29, GPIO_PIN_TYPE_STD);
    GPIO_writePin(29, 1);  // Idle high
}
```

#### 2. Send sync pulse at start of each cycle

Modify the main `while(1)` loop:

```c
while(1)
{
    //
    // ==================== SEND SYNC PULSE ====================
    // Generate falling edge on GPIO29 to sync BU board
    //
    GPIO_writePin(29, 0);           // Pull low (falling edge triggers BU)
    DEVICE_DELAY_US(10);            // Hold low for 10us
    GPIO_writePin(29, 1);           // Return high

    //
    // ==================== CAPTURE PHASE ====================
    //
    done = 0;
    isPlaybackMode = 0;

    // ... rest of capture code ...

    //
    // ==================== PLAYBACK PHASE ====================
    //
    done = 0;
    playbackIndex = 0;
    isPlaybackMode = 1;

    // ... rest of playback code ...

    //
    // ==================== CAN TX PHASE (AFTER PLAYBACK) ====================
    //
    sendVoltageBufferToBUBoard();
}
```

---

### BU Board Changes

#### 1. Move CAN RX to after playback

Edit `adc_ex2_soc_epwm.c` main loop:

```c
while(1)
{
    counter++;

    //
    // ==================== STEP 1: WAIT FOR SYNC SIGNAL ====================
    //
    while(firstpulse == false)
    {
        // Wait for GPIOISR to set firstpulse = true
    }

    //
    // ==================== STEP 2: ADC/DMA CAPTURE PHASE ====================
    //
    done = 0;
    isPlaybackMode = 0;

    // ... capture setup code (unchanged) ...

    DMA_startChannel(myDMA0_BASE);
    EPWM_enableADCTrigger(EPWM7_BASE, EPWM_SOC_A);

    //
    // ==================== STEP 3: WAIT FOR DMA COMPLETE ====================
    //
    while(done == 0 && firstpulse == true)
    {
    }

    if(firstpulse == false)
    {
        EPWM_disableADCTrigger(EPWM7_BASE, EPWM_SOC_A);
        continue;
    }

    EPWM_disableADCTrigger(EPWM7_BASE, EPWM_SOC_A);

    //
    // ==================== STEP 4: PLAYBACK PHASE ====================
    // (Moved BEFORE CAN RX)
    //
    done = 0;
    playbackIndex = 0;
    isPlaybackMode = 1;

    ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER1);
    EPWM_clearADCTriggerFlag(EPWM7_BASE, EPWM_SOC_A);

    if(firstpulse == false)
    {
        continue;
    }

    EPWM_enableADCTrigger(EPWM7_BASE, EPWM_SOC_A);

    while(done == 0 && firstpulse == true)
    {
    }

    EPWM_disableADCTrigger(EPWM7_BASE, EPWM_SOC_A);

    //
    // ==================== STEP 5: CAN RX PHASE (AFTER PLAYBACK) ====================
    // Receive voltage data from S board for storage/next cycle
    //
#ifdef CAN_ENABLED
    while(g_canRxComplete == false && firstpulse == true)
    {
        if(g_framesReady)
        {
            VoltageRx_copyToBuffer();
            g_framesReady = false;
        }
    }

    if(firstpulse == false)
    {
        continue;
    }

    g_canRxComplete = false;
#endif

    //
    // Loop back for next cycle
    //
}
```

---

## Summary of Changes

| Board | File | Change |
|-------|------|--------|
| **S Board** | main.c | Add `initSyncGPIO()` - configure GPIO29 as output |
| **S Board** | main.c (while loop) | Add sync pulse generation at START of loop |
| **S Board** | main.c (while loop) | Ensure `sendVoltageBufferToBUBoard()` is called AFTER playback |
| **BU Board** | adc_ex2_soc_epwm.c | Move CAN RX section to AFTER playback section |

---

## Timing Diagram After Changes

```
S Board:  [SYNC]──[CAPTURE 250ms]──[PLAYBACK 250ms]──[CAN TX ~20ms]──[SYNC]...
              │         │                │                  │
              │         │                │                  │
              ▼         ▼                ▼                  ▼
BU Board: [SYNC]──[CAPTURE 250ms]──[PLAYBACK 250ms]──[CAN RX ~20ms]──[SYNC]...
```

Both boards now run in parallel for capture and playback, with CAN transfer happening at the end when it won't cause timing issues.

---

## Previous Optimizations Applied to BU Board

The following optimizations were already applied to reduce ISR latency:

1. **RAM pragma for `VoltageRx_copyToBuffer()`** - Executes from RAM instead of FLASH (2-3x faster)
2. **Moved heavy processing out of ISR** - ISR just sets `g_framesReady` flag, main loop calls `VoltageRx_copyToBuffer()`
3. **Removed redundant buffer copy** - Eliminated 1,280-iteration copy loop

These changes reduced CAN ISR duration from ~54ms to <1ms.
