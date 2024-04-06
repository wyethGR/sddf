ifeq ($(strip $(MICROKIT_SDK)),)
$(error MICROKIT_SDK must be specified)
endif

BUILD_DIR ?= build
# By default we make a debug build so that the client debug prints can be seen.
MICROKIT_CONFIG ?= debug

ifeq ($(strip $(MICROKIT_BOARD)), qemu_arm_virt)
	BLOCK_DRIVER_DIR := virtio
	CPU := cortex-a53
else
$(error Unsupported MICROKIT_BOARD given)
endif

CC := clang
LD := ld.lld
MICROKIT_TOOL ?= $(MICROKIT_SDK)/bin/microkit
QEMU := qemu-system-aarch64

BOARD_DIR := $(MICROKIT_SDK)/board/$(MICROKIT_BOARD)/$(MICROKIT_CONFIG)
SYSTEM_FILE := board/$(MICROKIT_BOARD)/block.system
SDDF := $(abspath ../..)
UTIL := $(SDDF)/util

IMAGES := driver.elf client.elf
CFLAGS := -mcpu=$(CPU) \
		  -mstrict-align \
		  -nostdlib \
		  -ffreestanding \
		  -g3 \
		  -O3 \
		  -Wall -Wno-unused-function -Werror -Wno-unused-command-line-argument \
		  -I$(BOARD_DIR)/include \
		  -I$(SDDF)/include \
		  -target aarch64-none-elf
LDFLAGS := -L$(BOARD_DIR)/lib
LIBS := -lmicrokit -Tmicrokit.ld

IMAGE_FILE = $(BUILD_DIR)/loader.img
REPORT_FILE = $(BUILD_DIR)/report.txt

BLOCK_DRIVER_OBJS := driver.o printf.o putchar_debug.o
CLIENT_OBJS := client.o printf.o putchar_debug.o

BLOCK_DRIVER := $(SDDF)/drivers/blk/$(BLOCK_DRIVER_DIR)

all: $(BUILD_DIR) $(IMAGE_FILE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/client.o: client.c
	$(CC) -c $(CFLAGS) $^ -o $(BUILD_DIR)/client.o

$(BUILD_DIR)/putchar_debug.o: $(UTIL)/putchar_debug.c
	$(CC) -c $(CFLAGS) $^ -o $(BUILD_DIR)/putchar_debug.o

$(BUILD_DIR)/printf.o: $(UTIL)/printf.c
	$(CC) -c $(CFLAGS) $^ -o $(BUILD_DIR)/printf.o

$(BUILD_DIR)/driver.o: $(BLOCK_DRIVER)/block.c
	$(CC) -c $(CFLAGS) $(BLOCK_DRIVER)/block.c -o $(BUILD_DIR)/driver.o

$(BUILD_DIR)/driver.elf: $(addprefix $(BUILD_DIR)/, $(BLOCK_DRIVER_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/client.elf: $(addprefix $(BUILD_DIR)/, $(CLIENT_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(IMAGE_FILE) $(REPORT_FILE): $(addprefix $(BUILD_DIR)/, $(IMAGES)) $(SYSTEM_FILE)
	$(MICROKIT_TOOL) $(SYSTEM_FILE) --search-path $(BUILD_DIR) --board $(MICROKIT_BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

qemu: all
	$(QEMU) -machine virt,virtualization=on \
			-cpu cortex-a53 \
			-serial mon:stdio \
			-device loader,file=$(IMAGE_FILE),addr=0x70000000,cpu-num=0 \
			-m size=2G \
			-nographic \
			-global virtio-mmio.force-legacy=false \
			-d guest_errors \
			-drive file=mydisk,if=none,format=raw,id=hd \
			-device virtio-blk-device,drive=hd
