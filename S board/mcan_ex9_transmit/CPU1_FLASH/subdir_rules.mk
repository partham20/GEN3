################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
build-2066964895: ../c2000.syscfg
	@echo 'Building file: "$<"'
	@echo 'Invoking: SysConfig'
	"C:/ti/sysconfig_1.25.0/sysconfig_cli.bat" --script "D:/GEN3/S board/mcan_ex9_transmit/c2000.syscfg" -o "syscfg" -s "C:/ti/c2000/C2000Ware_6_00_01_00/.metadata/sdk.json" -d "F28P55x" --package 128PDT --part F28P55x_128PDT --compiler ccs
	@echo 'Finished building: "$<"'
	@echo ' '

syscfg/board.c: build-2066964895 ../c2000.syscfg
syscfg/board.h: build-2066964895
syscfg/board.cmd.genlibs: build-2066964895
syscfg/board.opt: build-2066964895
syscfg/board.json: build-2066964895
syscfg/pinmux.csv: build-2066964895
syscfg/c2000ware_libraries.cmd.genlibs: build-2066964895
syscfg/c2000ware_libraries.opt: build-2066964895
syscfg/c2000ware_libraries.c: build-2066964895
syscfg/c2000ware_libraries.h: build-2066964895
syscfg/clocktree.h: build-2066964895
syscfg: build-2066964895

syscfg/%.obj: ./syscfg/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: C2000 Compiler'
	"C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla2 --float_support=fpu32 --tmu_support=tmu1 --vcu_support=vcrc -Ooff --include_path="D:/GEN3/S board/mcan_ex9_transmit" --include_path="C:/ti/c2000/C2000Ware_6_00_01_00" --include_path="D:/GEN3/S board/mcan_ex9_transmit/device" --include_path="C:/ti/c2000/C2000Ware_6_00_01_00/driverlib/f28p55x/driverlib" --include_path="C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/include" --define=DEBUG --define=_FLASH --diag_suppress=10063 --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --abi=eabi --preproc_with_compile --preproc_dependency="syscfg/$(basename $(<F)).d_raw" --include_path="D:/GEN3/S board/mcan_ex9_transmit/CPU1_FLASH/syscfg" --obj_directory="syscfg" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

%.obj: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: C2000 Compiler'
	"C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla2 --float_support=fpu32 --tmu_support=tmu1 --vcu_support=vcrc -Ooff --include_path="D:/GEN3/S board/mcan_ex9_transmit" --include_path="C:/ti/c2000/C2000Ware_6_00_01_00" --include_path="D:/GEN3/S board/mcan_ex9_transmit/device" --include_path="C:/ti/c2000/C2000Ware_6_00_01_00/driverlib/f28p55x/driverlib" --include_path="C:/ti/ccs1281/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/include" --define=DEBUG --define=_FLASH --diag_suppress=10063 --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --abi=eabi --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" --include_path="D:/GEN3/S board/mcan_ex9_transmit/CPU1_FLASH/syscfg" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


