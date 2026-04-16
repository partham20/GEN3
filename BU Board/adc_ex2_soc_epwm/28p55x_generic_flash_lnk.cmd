MEMORY
{
   /* ── Bank 0 layout ──────────────────────────────────────────
    * 0x080000 - 0x081FFF  Sectors 0..7   : BOOT MANAGER (8 KB)
    *                                       — NEVER linked by this file.
    *                                       Loaded via JTAG once, read-only
    *                                       from the application's POV.
    * 0x082000 - 0x082001  BEGIN          : codestart jump (1 word + pad)
    * 0x082002 - 0x09FFFF  FLASH_BANK0    : application text/const/cinit
    *
    * The boot manager's APP_ENTRY_ADDR (boot_manager.c) must equal
    * 0x082000, so it jumps into this file's `codestart` section.
    * ────────────────────────────────────────────────────────── */
   BEGIN            : origin = 0x082000, length = 0x000002

   BOOT_RSVD        : origin = 0x000002, length = 0x000126     /* Part of M0, BOOT rom will use this for stack */
   RAMM0            : origin = 0x000128, length = 0x0002D8
   RAMM1            : origin = 0x000400, length = 0x000400

   RAMLS0           : origin = 0x008000, length = 0x000800
   RAMLS1           : origin = 0x008800, length = 0x000800
   RAMLS2           : origin = 0x009000, length = 0x001000

   RAMLS4           : origin = 0x00A000, length = 0x000800
   RAMLS5           : origin = 0x00A800, length = 0x000800
   RAMLS6           : origin = 0x00B000, length = 0x000800
   RAMLS7           : origin = 0x00B800, length = 0x000800

   /* FIX 1: Restored RAMGS3 to full 8K (was truncated to 0x4B0) */
   RAMGS0           : origin = 0x00C000, length = 0x002000
   RAMGS1           : origin = 0x00E000, length = 0x002000
   RAMGS2           : origin = 0x010000, length = 0x002000
   RAMGS3           : origin = 0x012000, length = 0x001300  /* 60% of 8K = 4,864 words */

   /* RAMGS3 upper 40% (0x013300-0x013FFF) merged into RAMLS89 since they're contiguous.
    * Total RAMLS89 = 3,328 (from RAMGS3) + 16,384 (RAMLS8+9) = 19,712 words */
   RAMLS89          : origin = 0x013300, length = 0x004D00

   // If you need separate CLA program memory, use these instead of RAMLS89:
   // RAMLS8_CLA    : origin = 0x004000, length = 0x002000
   // RAMLS9_CLA    : origin = 0x006000, length = 0x002000

   /* Flash Banks — FLASH_BANK0 starts at sector 8 (0x082002) because
    * sectors 0..7 are reserved for the BU boot manager. Length is
    * 0x1DFFE words = 120 sectors × 1 K words minus the 2-word BEGIN. */
   FLASH_BANK0     : origin = 0x082002, length = 0x1DFFE
   FLASH_BANK1     : origin = 0x0A0000, length = 0x20000
   FLASH_BANK2     : origin = 0x0C0000, length = 0x20000
   FLASH_BANK3     : origin = 0x0E0000, length = 0x20000
   FLASH_BANK4     : origin = 0x100000, length = 0x08000

   CLATOCPURAM      : origin = 0x001480,   length = 0x000080
   CPUTOCLARAM      : origin = 0x001500,   length = 0x000080
   CLATODMARAM      : origin = 0x001680,   length = 0x000080
   DMATOCLARAM      : origin = 0x001700,   length = 0x000080

   RESET            : origin = 0x3FFFC0, length = 0x000002
}


SECTIONS
{
   codestart        : > BEGIN
   .text            : >> FLASH_BANK0 | FLASH_BANK1, ALIGN(8)
   .cinit           : > FLASH_BANK0, ALIGN(8)
   .switch          : > FLASH_BANK0, ALIGN(8)
   .reset           : > RESET, TYPE = DSECT /* not used, */

   .stack           : > RAMM1

#if defined(__TI_EABI__)
   /* FIX 3: Added RAMLS4, RAMLS6, RAMLS7, RAMLS89 to .bss chain for more capacity.
    *         RAMLS89 (16K contiguous) ensures AdcRawBuffers (9216 words) can allocate. */
   .bss             : >> RAMGS1 | RAMGS2 | RAMGS3 | RAMLS4 | RAMLS6 | RAMLS7 | RAMLS89
   .bss:output      : > RAMLS2
   .init_array      : > FLASH_BANK0, ALIGN(8)
   .const           : > FLASH_BANK0, ALIGN(8)
   .data            : > RAMLS5
   .sysmem          : > RAMLS4
#else
   .pinit           : > FLASH_BANK0, ALIGN(8)
   .ebss            : >> RAMLS5 | RAMLS6
   .econst          : > FLASH_BANK0, ALIGN(8)
   .esysmem         : > RAMLS5
#endif

   ramgs0 : > RAMGS0
   ramgs1 : > RAMGS1
   ramgs2 : > RAMGS2

   /* THD FFT Buffers — must be explicitly placed, otherwise linker
    * auto-places them into BOOT_RSVD / RAMM0 which corrupts on flash boot */
   RFFTdata1 : > RAMLS89, ALIGN(1024)
   RFFTdata2 : > RAMLS89, ALIGN(8)
   RFFTdata3 : > RAMLS89, ALIGN(8)
   RFFTdata4 : > RAMLS89, ALIGN(8)

   /* FPU sine/cosine lookup tables — must be copied from flash to RAM.
    * Without this, RFFT_f32_sincostable() reads garbage and THD fails
    * in standalone boot (no JTAG). memcpy added in device.c */
   FPUmathTables : LOAD = FLASH_BANK0,
                    RUN = RAMLS0,
                    LOAD_START(FPUmathTablesLoadStart),
                    LOAD_SIZE(FPUmathTablesLoadSize),
                    RUN_START(FPUmathTablesRunStart),
                    ALIGN(8)

   #if defined(__TI_EABI__)
       .TI.ramfunc : {} LOAD = FLASH_BANK0,
                        RUN = RAMLS2,
                        LOAD_START(RamfuncsLoadStart),
                        LOAD_SIZE(RamfuncsLoadSize),
                        LOAD_END(RamfuncsLoadEnd),
                        RUN_START(RamfuncsRunStart),
                        RUN_SIZE(RamfuncsRunSize),
                        RUN_END(RamfuncsRunEnd),
                        ALIGN(8)
   #else
       .TI.ramfunc : {} LOAD = FLASH_BANK0,
                        RUN = RAMLS0,
                        LOAD_START(_RamfuncsLoadStart),
                        LOAD_SIZE(_RamfuncsLoadSize),
                        LOAD_END(_RamfuncsLoadEnd),
                        RUN_START(_RamfuncsRunStart),
                        RUN_SIZE(_RamfuncsRunSize),
                        RUN_END(_RamfuncsRunEnd),
                        ALIGN(8)
   #endif
}

/*
//===========================================================================
// End of file.
//===========================================================================
*/
