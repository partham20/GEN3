################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.obj: ../%.asm $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: C2000 Compiler'
	"C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla2 --float_support=fpu32 --tmu_support=tmu1 --vcu_support=vcrc -Ooff --include_path="D:/GEN3/BU Board/bu_boot_manager/device" --include_path="C:/ti/c2000/C2000Ware_6_00_01_00/libraries/flash_api/f28p55x/include" --include_path="C:/ti/c2000/C2000Ware_6_00_01_00/libraries/flash_api/f28p55x/include/FlashAPI" --include_path="D:/GEN3/BU Board/bu_boot_manager" --include_path="D:/GEN3/BU Board/adc_ex2_soc_epwm/CPU1_FLASH/device" --include_path="C:/ti/c2000/C2000Ware_6_00_01_00" --include_path="C:/ti/c2000/C2000Ware_6_00_01_00/driverlib/f28p55x/driverlib" --include_path="C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/include" --define=DEBUG --define=_FLASH --diag_suppress=10063 --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --abi=eabi --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" --include_path="D:/GEN3/BU Board/bu_boot_manager/CPU1_FLASH/syscfg" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

%.obj: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: C2000 Compiler'
	"C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla2 --float_support=fpu32 --tmu_support=tmu1 --vcu_support=vcrc -Ooff --include_path="D:/GEN3/BU Board/bu_boot_manager/device" --include_path="C:/ti/c2000/C2000Ware_6_00_01_00/libraries/flash_api/f28p55x/include" --include_path="C:/ti/c2000/C2000Ware_6_00_01_00/libraries/flash_api/f28p55x/include/FlashAPI" --include_path="D:/GEN3/BU Board/bu_boot_manager" --include_path="D:/GEN3/BU Board/adc_ex2_soc_epwm/CPU1_FLASH/device" --include_path="C:/ti/c2000/C2000Ware_6_00_01_00" --include_path="C:/ti/c2000/C2000Ware_6_00_01_00/driverlib/f28p55x/driverlib" --include_path="C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/include" --define=DEBUG --define=_FLASH --diag_suppress=10063 --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --abi=eabi --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" --include_path="D:/GEN3/BU Board/bu_boot_manager/CPU1_FLASH/syscfg" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


