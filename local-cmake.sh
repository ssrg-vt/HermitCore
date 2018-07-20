#!/bin/bash

# Call this script when you want to modify the kernel and install it in the
# standard location without using the install_copiler.py script (i.e. without
# having to rebuild the entire toolchain.

INSTALL_PATH=$HOME/hermit-popcorn/

# Cleanup any existing build directory and re-create it
rm -rf build/
mkdir build

# Get into the directory and call cmake with the rgiht parameters
cd build

cmake -DHERMIT_ARCH=aarch64 \
	-DTARGET_ARCH=aarch64-hermit \
	-DCMAKE_INSTALL_PREFIX=$INSTALL_PATH \
	-DCOMPILER_BIN_DIR=$INSTALL_PATH/x86_64-host/bin \
	-DHERMIT_PREFIX=$INSTALL_PATH/x86_64-host \
	-DMIGRATION_LOG=1 \
	-DKERNEL_DEBUG=1 \
	..

cd -
