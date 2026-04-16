# 4 Tasks: KVA/KVAR Formula Change, Calibration Struct Update, Calibrated Vrms to BU

---

## Task 1: Change KVA formula

**Current**: KVA uses `apparentPower` from the metrology library (which computes it as `hypotf(activePower, reactivePower)` internally), then applies a gain.

```c
// Lines 350-353 (input), 475-478 (output)
inputParam->inputKVARP = APPLY_GAIN(pR->readings.apparentPower, cal->primaryTotalKW);
```

**New**: KVA = calibrated Vrms * calibrated Irms (per phase). Comment out the current implementation and replace with:

```
KVA_R = calibratedVrms_R * calibratedIrms_R
KVA_Y = calibratedVrms_Y * calibratedIrms_Y
KVA_B = calibratedVrms_B * calibratedIrms_B
TotalKVA = KVA_R + KVA_Y + KVA_B
```

---

## Task 2: Change KVAR formula

**Current**: KVAR uses `reactivePower` from the metrology library, then applies a gain.

```c
// Lines 355-358 (input), 480-483 (output)
inputParam->inputKVARRP = APPLY_GAIN(pR->readings.reactivePower, cal->primaryTotalKW);
```

**New**: KVAR = sqrt(KVA^2 - KW^2) per phase. Comment out the current implementation and replace with:

```
KVAR_R = sqrt(KVA_R^2 - KW_R^2)
KVAR_Y = sqrt(KVA_Y^2 - KW_Y^2)
KVAR_B = sqrt(KVA_B^2 - KW_B^2)
TotalKVAR = KVAR_R + KVAR_Y + KVAR_B
```

---

## Task 3: Change PDUData calibration struct (3 fields replaced)

**Current** (words 444-446):

```c
uint16_t primaryNeutralCurrent;      // 444
uint16_t secondaryNeutralCurrent;    // 445
uint16_t primaryTotalKW;             // 446
```

**New**: Replace these 3 fields with per-phase primary KW gains:

```c
ThreePhaseValues primaryKW;          // 444 (R), 445 (Y), 446 (B)
```

This means:

- Remove `primaryNeutralCurrent`, `secondaryNeutralCurrent`, `primaryTotalKW`
- Add `primaryKW` (R, Y, B) -- gives per-phase KW calibration for primary side (matching how secondary already has `secondaryKW.R/Y/B`)
- All code that used `cal->primaryTotalKW` for per-phase power must switch to `cal->primaryKW.R/Y/B`
- Neutral current gain references (`cal->primaryNeutralCurrent`, `cal->secondaryNeutralCurrent`) need to be removed or reassigned

---

## Task 4: Send calibrated Vrms to BU boards

**Current** (`adc_ex2_soc_epwm.c:373-375`): Sends raw Vrms from metrology:

```c
vrmsR = (uint16_t)(gMetrologyWorkingData.phases[3].readings.RMSVoltage);
```

**New**: Apply calibration gain before sending:

```c
vrmsR = (uint16_t)APPLY_GAIN(gMetrologyWorkingData.phases[3].readings.RMSVoltage, cal->primaryVoltage.R);
```

This ensures BU boards receive the same calibrated voltage values that the S-Board uses for its own power calculations.

---

## Dependencies

Task 3 (struct change) must happen first since it affects the gain field names used in Tasks 1, 2, and the existing power code. Task 4 is independent.
