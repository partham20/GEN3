################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Add inputs and outputs from these tool invocations to the build variables 
CMD_SRCS += \
../ex_4_ram_lnk.cmd 

LIB_SRCS += \
../FAPI_F28P55x_EABI_v4.00.00.lib \
C:/ti/c2000/C2000Ware_6_00_01_00/driverlib/f28p55x/driverlib/ccs/Debug/driverlib.lib 

ASM_SRCS += \
../flash_kernel_ex4_codestartbranch.asm 

C_SRCS += \
../MCAN_Boot.c \
../ex_4_cpu1brom_pll.c \
../flash_kernel_ex4_boot.c \
../flash_kernel_ex4_can_flash_kernel.c 

C_DEPS += \
./MCAN_Boot.d \
./ex_4_cpu1brom_pll.d \
./flash_kernel_ex4_boot.d \
./flash_kernel_ex4_can_flash_kernel.d 

OBJS += \
./MCAN_Boot.obj \
./ex_4_cpu1brom_pll.obj \
./flash_kernel_ex4_boot.obj \
./flash_kernel_ex4_can_flash_kernel.obj \
./flash_kernel_ex4_codestartbranch.obj 

ASM_DEPS += \
./flash_kernel_ex4_codestartbranch.d 

OBJS__QUOTED += \
"MCAN_Boot.obj" \
"ex_4_cpu1brom_pll.obj" \
"flash_kernel_ex4_boot.obj" \
"flash_kernel_ex4_can_flash_kernel.obj" \
"flash_kernel_ex4_codestartbranch.obj" 

C_DEPS__QUOTED += \
"MCAN_Boot.d" \
"ex_4_cpu1brom_pll.d" \
"flash_kernel_ex4_boot.d" \
"flash_kernel_ex4_can_flash_kernel.d" 

ASM_DEPS__QUOTED += \
"flash_kernel_ex4_codestartbranch.d" 

C_SRCS__QUOTED += \
"../MCAN_Boot.c" \
"../ex_4_cpu1brom_pll.c" \
"../flash_kernel_ex4_boot.c" \
"../flash_kernel_ex4_can_flash_kernel.c" 

ASM_SRCS__QUOTED += \
"../flash_kernel_ex4_codestartbranch.asm" 


