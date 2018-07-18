include(${CMAKE_CURRENT_LIST_DIR}/HermitCore-Utils.cmake)
include_guard()

# let user provide a different path to the toolchain
set_default(HERMIT_PREFIX /opt/hermit)
set_default(COMPILER_BIN_DIR ${HERMIT_PREFIX}/usr/local/bin)

# Flags not supported by clang/llvm: -finline-functions -falign-jumps=1
# -falign-loops=1
set(TARGET_ARCH aarch64-hermit)
set(HERMIT_KERNEL_FLAGS
					-Wall -O2 -g -mgeneral-regs-only
					-fomit-frame-pointer -ffreestanding
					-nostdinc -fno-stack-protector
					-fno-common -Wframe-larger-than=2048
					-fno-strict-aliasing -fno-asynchronous-unwind-tables
					-fno-strict-overflow -target aarch64-hermit
					-Wno-asm-operand-widths
					-Wno-shift-negative-value)

# Pierre: debug options and migration log
if(KERNEL_DEBUG)
	set(HERMIT_KERNEL_FLAGS
		-g -ggdb3 -O0 ${HERMIT_KERNEL_FLAGS})
else(KERNEL_DEBUG)
	set(HERMIT_KERNEL_FLAGS
		-O2 -fno-schedule-insns -fno-schedule-insns2 ${HERMIT_KERNEL_FLAGS})
endif(KERNEL_DEBUG)

if(MIGRATION_LOG)
	set(HERMIT_KERNEL_FLAGS
		-DHAVE_MIG_LOG ${HERMIT_KERNEL_FLAGS})
endif(MIGRATION_LOG)

set(HERMIT_APP_FLAGS
					-O2 -ftree-vectorize -target aarch64-hermit -fopenmp=libgomp)

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
