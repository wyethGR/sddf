#
# Copyright 2024, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Include this snippet in your project Makefile to build the IMX8 uSDHC driver.
# Assumes libsddf_util_debug.a is in ${LIBS}.
# Requires usdhc_regs to be set to the mapped base of the uSDHC registers
# in the system description file.

USDHC_DRIVER_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

usdhc_driver.elf: usdhc/imx/usdhc_driver.o
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

usdhc/imx/usdhc_driver.o: ${USDHC_DRIVER_DIR}/usdhc.c |usdhc/imx
	$(CC) -c $(CFLAGS) -I${USDHC_DRIVER_DIR}/include -o $@ $<

-include usdhc_driver.d

usdhc/imx:
	mkdir -p $@

clean::
	rm -f usdhc/imx/usdhc_driver.[do]

clobber::
	rm -rf usdhc
