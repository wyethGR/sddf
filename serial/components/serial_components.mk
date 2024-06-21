#
# Copyright 2023, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#
# This Makefile snippet builds the serial RX and TX virtualisers
# it should be included into your project Makefile
#
# NOTES:
#  Generates serial_virt_rx.elf serial_virt_tx.elf
#  It relies on the variable SERIAL_NUM_CLIENTS as a C compiler flag
#  to configure the virtualisers
#

ifeq ($(strip $(SERIAL_NUM_CLIENTS)),)
$(error Specify the number of clients for the serial virtualisers.  Expect -DSERIAL_NUM_CLIENTS=3 or similar)
endif
ifeq ($(strip $(UART_DRIVER)),)
$(error The serial virtualisers need headers from the UART source. Please specify UART_DRIVER)
endif

SERIAL_IMAGES:= serial_virt_rx.elf serial_virt_tx.elf

CFLAGS_serial := -I ${SDDF}/include -I${SDDF}/util/include ${SERIAL_NUM_CLIENTS} -I${SDDF}/examples/serial/include

CHECK_SERIAL_FLAGS_MD5:=.serial_cflags-$(shell echo -- ${CFLAGS} ${CFLAGS_serial} | shasum | sed 's/ *-//')

${CHECK_SERIAL_FLAGS_MD5}:
	-rm -f .serial_cflags-*
	touch $@


serial_virt_%.elf: virt_%.o
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

virt_tx.o virt_rx.o: ${CHECK_SERIAL_FLAGS_MD5}
virt_%.o: ${SDDF}/serial/components/virt_%.c 
	${CC} ${CFLAGS} ${CFLAGS_serial} -o $@ -c $<

clean::
	rm -f serial_virt_[rt]x.[od] .serial_cflags-*

clobber::
	rm -f ${SERIAL_IMAGES}


-include virt_rx.d
-include virt_tx.d
