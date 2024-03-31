#!/bin/bash

set -x

export RUST_TARGET_PATH=$(pwd)/support/targets
export RUST_TARGET=aarch64-sel4-microkit
export MICROKIT_INCLUDE=/home/ivanv/ts/sddf/microkit-sdk-1.2.6/board/qemu_arm_virt/debug/include

rm -r build
make

