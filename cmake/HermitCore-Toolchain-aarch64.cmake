include(${CMAKE_CURRENT_LIST_DIR}/HermitCore-Utils.cmake)
include_guard()

# let user provide a different path to the toolchain
set_default(HERMIT_PREFIX /opt/hermit)
set_default(COMPILER_BIN_DIR ${HERMIT_PREFIX}/usr/local/bin)

set(TARGET_ARCH aarch64-hermit)
set(HERMIT_KERNEL_FLAGS
					-Wall -g -O0 -fno-builtin -mgeneral-regs-only
					-fomit-frame-pointer -finline-functions -ffreestanding
					-nostdinc -fno-stack-protector
					-falign-jumps=1 -falign-loops=1
					-fno-common -Wframe-larger-than=1024
					-fno-strict-aliasing -fno-asynchronous-unwind-tables
					-fno-strict-overflow -target aarch64-hermit)

set(HERMIT_APP_FLAGS
					-g -O0 -fno-builtin -ftree-vectorize -target aarch64-hermit -fopenmp=libgomp)

set(CMAKE_SYSTEM_NAME Generic)

# point CMake to our toolchain
set(CMAKE_C_COMPILER ${COMPILER_BIN_DIR}/clang)
set(CMAKE_CXX_COMPILER ${COMPILER_BIN_DIR}/clang++)
#set(CMAKE_Fortran_COMPILER ${TOOLCHAIN_BIN_DIR}/${TARGET_ARCH}-gfortran)
#set(CMAKE_Go_COMPILER ${TOOLCHAIN_BIN_DIR}/${TARGET_ARCH}-gccgo)

# hinting the prefix and location is needed in order to correctly detect
# binutils
set(_CMAKE_TOOLCHAIN_PREFIX "${TARGET_ARCH}-")
set(_CMAKE_TOOLCHAIN_LOCATION ${HERMIT_PREFIX}/bin)
