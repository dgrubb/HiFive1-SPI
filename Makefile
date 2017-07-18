TARGET = HiFive1-SPI
CFLAGS += -O2 -fno-builtin-printf -DUSE_PLIC -DUSE_M_TIME

BSP_BASE = ../../bsp

C_SRCS += HiFive1-SPI.c
C_SRCS += $(BSP_BASE)/drivers/plic/plic_driver.c

include $(BSP_BASE)/env/common.mk

