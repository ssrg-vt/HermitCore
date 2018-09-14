#!/bin/bash

# Call this script when you want to modify the kernel and install it in the
# standard location without using the install_copiler.py script (i.e. without
# having to rebuild the entire toolchain.

INSTALL_PATH=$HOME/hermit-popcorn/

# Cleanup any existing build directory and re-create it
rm -rf build-x86-64
rm -rf build-aarch64
mkdir build-x86-64
mkdir build-aarch64

# Get into the directory and call cmake with the rgiht parameters
cd build-x86-64
cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_PATH \
	-DCOMPILER_BIN_DIR=$INSTALL_PATH/x86_64-host/bin \
	-DCMAKE_OBJCOPY=$INSTALL_PATH/x86_64-host/bin/x86_64-hermit-objcopy \
	-DCMAKE_AR=$INSTALL_PATH/x86_64-host/bin/x86_64-hermit-ar \
	-DHERMIT_PREFIX=$INSTALL_PATH/x86_64-host \
	-DMIGRATION_LOG=1 \
	-DKERNEL_DEBUG=1 \
	..
cd -

cd build-aarch64
cmake -DHERMIT_ARCH=aarch64 \
	-DTARGET_ARCH=aarch64-hermit \
	-DCMAKE_OBJCOPY=$INSTALL_PATH/x86_64-host/bin/aarch64-hermit-objcopy \
	-DCMAKE_AR=$INSTALL_PATH/x86_64-host/bin/aarch64-hermit-ar \
	-DCMAKE_INSTALL_PREFIX=$INSTALL_PATH \
	-DCOMPILER_BIN_DIR=$INSTALL_PATH/x86_64-host/bin \
	-DHERMIT_PREFIX=$INSTALL_PATH/x86_64-host \
	-DMIGRATION_LOG=1 \
	-DKERNEL_DEBUG=1 \
	..
cd -
