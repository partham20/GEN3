################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.obj: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: C2000 Compiler'
	"C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla2 --float_support=fpu32 --tmu_support=tmu1 --vcu_support=vcrc -Ooff --include_path="D:/GEN3/S board/adc_ex2_soc_epwm/metrology" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/flash_api/f28p55x/include" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/flash_api/f28p55x/include/FlashAPI" --include_path="D:/GEN3/S board/adc_ex2_soc_epwm" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00" --include_path="D:/GEN3/S board/adc_ex2_soc_epwm/device" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/driverlib/f28p55x/driverlib" --include_path="C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/include" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/flash_api/f28p55x/include" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/flash_api/f28p55x/include/FlashAPI" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/dsp/FPU/c28/examples/common" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/dsp/FPU/c28/examples/common/f28p55x" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/dsp/FPU/c28/include" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/control/DCL/c28/examples/F28P55x_DF22" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/control/DCL/c28/include" --define=DEBUG --define=_FLASH --diag_suppress=10063 --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --abi=eabi --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" --include_path="D:/GEN3/S board/adc_ex2_soc_epwm/CPU1_FLASH/syscfg" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

build-1627764449: ../adc_ex2_soc_epwm.syscfg
	@echo 'Building file: "$<"'
	@echo 'Invoking: SysConfig'
	"C:/ti/sysconfig_1.25.0/sysconfig_cli.bat" --script "D:/GEN3/S board/adc_ex2_soc_epwm/adc_ex2_soc_epwm.syscfg" -o "syscfg" -s "C:/ti/c2000/C2000Ware_5_04_00_00/.metadata/sdk.json" -d "F28P55x" -p "128PDT" -r "F28P55x_128PDT" --compiler ccs
	@echo 'Finished building: "$<"'
	@echo ' '

syscfg/board.c: build-1627764449 ../adc_ex2_soc_epwm.syscfg
syscfg/board.h: build-1627764449
syscfg/board.cmd.genlibs: build-1627764449
syscfg/board.opt: build-1627764449
syscfg/board.json: build-1627764449
syscfg/pinmux.csv: build-1627764449
syscfg/epwm.dot: build-1627764449
syscfg/adc.dot: build-1627764449
syscfg/c2000ware_libraries.cmd.genlibs: build-1627764449
syscfg/c2000ware_libraries.opt: build-1627764449
syscfg/c2000ware_libraries.c: build-1627764449
syscfg/c2000ware_libraries.h: build-1627764449
syscfg/clocktree.h: build-1627764449
syscfg: build-1627764449

syscfg/%.obj: ./syscfg/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: C2000 Compiler'
	"C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla2 --float_support=fpu32 --tmu_support=tmu1 --vcu_support=vcrc -Ooff --include_path="D:/GEN3/S board/adc_ex2_soc_epwm/metrology" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/flash_api/f28p55x/include" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/flash_api/f28p55x/include/FlashAPI" --include_path="D:/GEN3/S board/adc_ex2_soc_epwm" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00" --include_path="D:/GEN3/S board/adc_ex2_soc_epwm/device" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/driverlib/f28p55x/driverlib" --include_path="C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/include" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/flash_api/f28p55x/include" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/flash_api/f28p55x/include/FlashAPI" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/dsp/FPU/c28/examples/common" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/dsp/FPU/c28/examples/common/f28p55x" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/dsp/FPU/c28/include" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/control/DCL/c28/examples/F28P55x_DF22" --include_path="C:/ti/c2000/C2000Ware_5_04_00_00/libraries/control/DCL/c28/include" --define=DEBUG --define=_FLASH --diag_suppress=10063 --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --abi=eabi --preproc_with_compile --preproc_dependency="syscfg/$(basename $(<F)).d_raw" --include_path="D:/GEN3/S board/adc_ex2_soc_epwm/CPU1_FLASH/syscfg" --obj_directory="syscfg" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


