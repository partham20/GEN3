//###########################################################################
//
// FILE:    ex_4_cpu1brom_pll.c
//
// TITLE:   PLL Enable and Power up Functions
//
//###########################################################################
// $TI Release: $
// 
//###########################################################################

//
// Included Files
//
#include "ex_4_cpu1bootrom.h"
#include "ex_4_cpu1brom_utils.h"

extern uint32_t CPU1BROM_bootStatus;
uint32_t pllMultiplier, pllDivider;

//
// Function Prototypes
//
uint16_t BROMDCC_verifySingleShotClock(DCC_Count0ClockSource clk0src,
                                       DCC_Count1ClockSource clk1src, uint32_t dccCounterSeed0,
                                       uint32_t dccCounterSeed1, uint32_t dccValidSeed0);


/**
* CPU1BROM_triggerSysPLLLock - Power up and lock the SYS PLL.
* The "divider" configured in this routine is PLL Output Divider (ODIV)
* and not "SYSCLKDIVSEL".
*
*
* \brief PLL Lock function
*
* Design: \ref did_trigger_apll_lock_usecase did_enable_pll_lock_algo
*              did_pll_lock_fail_status_algo
* Requirement: REQ_TAG(C2000BROM-214), REQ_TAG(C2000BROM-164)
*
* PLL Lock function
*
*/
void CPU1BROM_triggerSysPLLLock(uint32_t clkSource, uint32_t multiplier, uint32_t divider)
{
	pllMultiplier = multiplier;
	pllDivider = divider;

	EALLOW;

    //
    // Bypass PLL from SYSCLK
    //
    HWREGH(CLKCFG_BASE + SYSCTL_O_SYSPLLCTL1) &= ~SYSCTL_SYSPLLCTL1_PLLCLKEN;

    //
    // Delay of at least 120 OSCCLK cycles required post PLL bypass
    //
    NOP_CYCLES(120);

    //
    // Use INTOSC2 (~10 MHz) as the main PLL clock source
    //
    HWREGH(CLKCFG_BASE + SYSCTL_O_CLKSRCCTL1) &= ~SYSCTL_CLKSRCCTL1_OSCCLKSRCSEL_M;
	NOP_CYCLES(45);
	HWREGH(CLKCFG_BASE + SYSCTL_O_CLKSRCCTL1) |= (clkSource & SYSCTL_CLKSRCCTL1_OSCCLKSRCSEL_M);

    //
    // Delay of at least 300 OSCCLK cycles after clock source change
    //
    NOP_CYCLES(150);
    NOP_CYCLES(150);

	//
	// Turn off PLL and delay for power down
	//
	HWREGH(CLKCFG_BASE + SYSCTL_O_SYSPLLCTL1) &= ~SYSCTL_SYSPLLCTL1_PLLEN;

    //
    // Delay 66 cycles from powerdown to powerup
    //
    NOP_CYCLES(66);

    //
    // Set PLL Multiplier and Output Clock Divider (ODIV)
    //
    HWREG(CLKCFG_BASE + SYSCTL_O_SYSPLLMULT) =
                     ((HWREG(CLKCFG_BASE + SYSCTL_O_SYSPLLMULT) &
                     ~(SYSCTL_SYSPLLMULT_ODIV_M | SYSCTL_SYSPLLMULT_IMULT_M)) |
                              (divider << SYSCTL_SYSPLLMULT_ODIV_S) |
                              (multiplier << SYSCTL_SYSPLLMULT_IMULT_S));

	//
	// Enable the sys PLL
	//
	HWREGH(CLKCFG_BASE + SYSCTL_O_SYSPLLCTL1) |= SYSCTL_SYSPLLCTL1_PLLEN;

    //
    // 200 Cycles after enabling PLL
    //
    NOP_CYCLES(200);

    EDIS;
}

/**
* \brief Switch to PLL output
*
* Design: \ref did_safety_switch_to_pll_clock_usecase
* Requirement: REQ_TAG(C2000BROM-215), REQ_TAG(C2000BROM-164)
*
* PLL Lock function
*
*/
uint16_t CPU1BROM_switchToPLL(uint32_t pllInputClockMhz)
{
	uint16_t count = 1024; // timeout
	uint16_t dccStatus;
    uint32_t dccCnt0Seed, dccCnt1Seed, dccValid0Seed;

    //
    // Setup DCC Values
    //
    dccCnt0Seed = 104U;
    dccValid0Seed = 32U;

    //
    // + below is to convert bit field values to actual divider values
    //
	// Conterseed1 = window * (Fclk1/Fclk0)
	// window - 120, fclk0 is 10Mhz
	dccCnt1Seed = (120UL * (((pllInputClockMhz * pllMultiplier)/(pllDivider + 1UL)) / 10UL));

	//
	// Wait for the SYSPLL lock counter
	//
	while(((HWREGH(CLKCFG_BASE + SYSCTL_O_SYSPLLSTS) &
			SYSCTL_SYSPLLSTS_LOCKS) == 0U) && (count != 0U))
	{
		count--;
	}

	//
	// Using DCC to verify the PLL clock
	//
	SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_DCC0);
	dccStatus = BROMDCC_verifySingleShotClock((DCC_Count0ClockSource)DCC_COUNT0SRC_INTOSC2,
											  (DCC_Count1ClockSource)DCC_COUNT1SRC_PLL,
											  dccCnt0Seed, dccCnt1Seed, dccValid0Seed);
	SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_DCC0);

	//
    // If DCC failed to verify PLL clock is running correctly, update status
	// and power down PLL
    //
    if(ERROR == dccStatus)
    {
        //
        // Turn off PLL and delay for power down
        //
        EALLOW;
        HWREGH(CLKCFG_BASE + SYSCTL_O_SYSPLLCTL1) &= ~SYSCTL_SYSPLLCTL1_PLLEN;
        EDIS;

        //
        // Delay 120 cycles
        //
        NOP_CYCLES(120);
    }
	else
	{
	    //
	    // Switch sysclk to PLL clock
	    //
		EALLOW;
	    HWREGH(CLKCFG_BASE + SYSCTL_O_SYSPLLCTL1) |= SYSCTL_SYSPLLCTL1_PLLCLKEN;
        EDIS;
        
        //
        // ~120 PLLSYSCLK delay to allow voltage regulator to stabilize
        //
        NOP_CYCLES(120);
    }
    return (dccStatus);
}

uint16_t BROMDCC_verifySingleShotClock(DCC_Count0ClockSource clk0src,
                                       DCC_Count1ClockSource clk1src, uint32_t dccCounterSeed0,
                                       uint32_t dccCounterSeed1, uint32_t dccValidSeed0)
{
    uint32_t j = dccCounterSeed1;
    uint16_t status;

    //
    // Clear DONE and ERROR flags
    //
    EALLOW;
    HWREGH(DCC0_BASE + DCC_O_STATUS) = 3U;
    EDIS;

    //
    // Disable DCC
    //
    DCC_disableModule(DCC0_BASE);

    //
    // Disable Error Signal
    //
    DCC_disableErrorSignal(DCC0_BASE);

    //
    // Disable Done Signal
    //
    DCC_disableDoneSignal(DCC0_BASE);

    //
    // Configure Clock Source0 to whatever set as a clock source for PLL
    //
    DCC_setCounter0ClkSource(DCC0_BASE, clk0src);

    //
    // Configure Clock Source1 to PLL
    //
    DCC_setCounter1ClkSource(DCC0_BASE, clk1src);

    //
    // Configure COUNTER-0, COUNTER-1 & Valid Window
    //
    DCC_setCounterSeeds(DCC0_BASE, dccCounterSeed0, dccValidSeed0,
                        dccCounterSeed1);

    //
    // Enable Single Shot mode
    //
    DCC_enableSingleShotMode(DCC0_BASE, DCC_MODE_COUNTER_ZERO);

    //
    // Enable DCC to start counting
    //
    DCC_enableModule(DCC0_BASE);

    //
    // Wait until Error or Done Flag is generated or timeout
    //
    while(((HWREGH(DCC0_BASE + DCC_O_STATUS) &
           (DCC_STATUS_ERR | DCC_STATUS_DONE)) == 0U) && (j != 0U))
    {
		// j is decremented to give enough timeout for HW to complete
		// the comparision. The result will be determined from the values
		// in status register.
       j--;
    }

    //
    // Returns NO_ERROR if DCC completes without error
    //
    if((HWREGH(DCC0_BASE + DCC_O_STATUS) &
            (DCC_STATUS_ERR | DCC_STATUS_DONE)) == DCC_STATUS_DONE)
	{
		status = NO_ERROR;
	}
	else
	{
		status = ERROR;
	}

    return status;
}


//
// End of File
//
