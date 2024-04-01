#
# Copyright 2023, Colias Group, LLC
#
# SPDX-License-Identifier: BSD-2-Clause
#

ifeq ($(strip $(MICROKIT_INCLUDE)),)
$(error MICROKIT_INCLUDE must be specified)
endif

ifeq ($(strip $(RUST_TARGET_PATH)),)
$(error RUST_TARGET_PATH must be specified)
endif

ifeq ($(strip $(RUST_TARGET)),)
$(error RUST_TARGET must be specified)
endif

BUILD_DIR ?= build

common_env := \
	SEL4_INCLUDE_DIRS=$(MICROKIT_INCLUDE)

common_options := \
	-Z unstable-options \
	-Z build-std=core,alloc,compiler_builtins \
	-Z build-std-features=compiler-builtins-mem \
	--target $(RUST_TARGET) \
	--release \
	--out-dir $(abspath $(BUILD_DIR))

$(BUILD_DIR)/sddf_net_virtio_driver.elf: FORCE
	$(common_env) \
		cargo build \
			$(common_options) \
			-p sddf-net-virtio-driver

FORCE: ;
