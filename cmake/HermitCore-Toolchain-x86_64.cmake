include(${CMAKE_CURRENT_LIST_DIR}/HermitCore-Utils.cmake)
include_guard()

# let user provide a different path to the toolchain
set_default(TOOLCHAIN_BIN_DIR /opt/hermit/bin)

set(TARGET_ARCH x86_64-hermit)
set(HERMIT_KERNEL_FLAGS
					-m64 -Wall -O2 -mno-red-zone
					-fomit-frame-pointer -ffreestanding
					-nostdinc -fno-stack-protector -mno-sse -mno-mmx
					-mno-sse2 -mno-3dnow -mno-avx
					-fno-common -Wframe-larger-than=2048
					-fno-strict-aliasing -fno-asynchronous-unwind-tables
					-fno-strict-overflow -target x86_64-hermit)

# Pierre: disabling this warning temporarily
set(HERMIT_KERNEL_FLAGS -Wno-shift-negative-value ${HERMIT_KERNEL_FLAGS})

if(KERNEL_DEBUG)
	set(HERMIT_KERNEL_FLAGS
		-g -O0 ${HERMIT_KERNEL_FLAGS})
else(KERNEL_DEBUG)
	set(HERMIT_KERNEL_FLAGS
		-O2 ${HERMIT_KERNEL_FLAGS})
endif(KERNEL_DEBUG)

if(KERNEL_DEBUG)
	set(HERMIT_KERNEL_FLAGS
		-g -ggdb3 -O0 ${HERMIT_KERNEL_FLAGS})
else(KERNEL_DEBUG)
	set(HERMIT_KERNEL_FLAGS
		-O2	-fno-schedule-insns -fno-schedule-insns2
		${HERMIT_KERNEL_FLAGS})
endif(KERNEL_DEBUG)

if(MIGRATION_LOG)
	set(HERMIT_KERNEL_FLAGS
		-DHAVE_MIG_LOG ${HERMIT_KERNEL_FLAGS})
endif(MIGRATION_LOG)

set(HERMIT_APP_FLAGS
					-m64 -mtls-direct-seg-refs -O3 -ftree-vectorize)

set(CMAKE_SYSTEM_NAME Generic)

# point CMake to our toolchain
set(CMAKE_C_COMPILER ${COMPILER_BIN_DIR}/clang)
set(CMAKE_CXX_COMPILER ${COMPILER_BIN_DIR}/clang++)
# set(CMAKE_Fortran_COMPILER ${TOOLCHAIN_BIN_DIR}/${TARGET_ARCH}-gfortran)
# set(CMAKE_Go_COMPILER ${TOOLCHAIN_BIN_DIR}/${TARGET_ARCH}-gccgo)

# hinting the prefix and location is needed in order to correctly detect
# binutils
set(_CMAKE_TOOLCHAIN_PREFIX "${TARGET_ARCH}-")
set(_CMAKE_TOOLCHAIN_LOCATION ${TOOLCHAIN_BIN_DIR})
