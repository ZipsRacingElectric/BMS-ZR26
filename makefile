# Project name
PROJECT = stub

# Imported files
CHIBIOS  := $(CHIBIOS_SOURCE_PATH)

# Directories
CONFDIR  	:= ./config
BUILDDIR 	:= ./build
DEPDIR   	:= ./build/dep
BOARDDIR	:= ./build/board
COMMONDIR	:= ./common

# Includes
ALLINC += src

# Source files
CSRC =	$(ALLCSRC)						\
		src/main.c						\
										\
		src/peripherals.c				\
		src/peripherals/eeprom_map.c	\
										\
		src/can_charger.c				\
		src/can_vehicle.c				\
		src/can/receive.c				\
		src/can/transmit.c				\
										\
		src/monitor_thread.c			\
										\
		src/watchdog.c

# Common library includes
include common/src/debug.mk
include common/src/fault_handler.mk

include common/src/can/can_thread.mk
include common/src/can/eeprom_can.mk
include common/src/can/tc_hk_lf_540_14.mk

include common/src/controls/rolling_average.mk

include common/src/peripherals/adc/analog_linear.mk
include common/src/peripherals/adc/dhab_s124.mk
include common/src/peripherals/adc/stm_adc.mk
include common/src/peripherals/adc/thermistor_pulldown.mk
include common/src/peripherals/i2c/mc24lc32.mk
include common/src/peripherals/spi/ltc6811.mk

# Compiler flags
USE_OPT = -Og -Wall -Wextra -lm

# C macro definitions
UDEFS =

# ASM definitions
UADEFS =

# Include directories
UINCDIR =

# Library directories
ULIBDIR =

# Libraries
ULIBS =

# Common toolchain includes
include common/common.mk
include common/make/openocd.mk

# ChibiOS compilation hooks
PRE_MAKE_ALL_RULE_HOOK: $(BOARD_FILES) $(CLANGD_FILE)