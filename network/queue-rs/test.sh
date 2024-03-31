#!/bin/bash

set -x

export RUST_TARGET_PATH=$(pwd)/support/targets

cargo build \
	-Z build-std=core,alloc,compiler_builtins \
	-Z build-std-features=compiler-builtins-mem \
	--target aarch64-sel4-microkit \
