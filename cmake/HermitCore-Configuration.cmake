set(PACKAGE_VERSION "0.2.9" CACHE STRING
	"HermitCore current version")

set(MAX_CORES "16" CACHE STRING
	"Maximum number of cores that can be managed")

set(MAX_TASKS "32" CACHE STRING
	"Maximum number of tasks")

set(MAX_ISLE "8" CACHE STRING
	"Maximum number of NUMA isles")

set(MAX_FNAME "128" CACHE STRING
	"Define the maximum length of a file name")

set(KERNEL_STACK_SIZE 8192 CACHE STRING
	"Kernel stack size in bytes")

set(DEFAULT_STACK_SIZE 1048576 CACHE STRING
	"Task stack size in bytes")

set(MAX_ARGC_ENVC 128 CACHE STRING
	"Maximum number of command line parameters and enviroment variables
	forwarded to uhyve")

option(DYNAMIC_TICKS
	"Don't use a periodic timer event to keep track of time" OFF)

option(SAVE_FPU
	"Save FPU registers on context switch" ON)

set(HAVE_ARCH_MEMSET "1" CACHE STRING
	"Use machine specific version of memset")
set(HAVE_ARCH_MEMCPY "1" CACHE STRING
	"Use machine specific version of memcpy")
set(HAVE_ARCH_STRLEN "1" CACHE STRING
	"Use machine specific version of strlen")
if("${HERMIT_ARCH}" STREQUAL "aarch64")
set(HAVE_ARCH_STRCPY "0" CACHE STRING
	"Use machine specific version of strcpy")
set(HAVE_ARCH_STRNCPY "0" CACHE STRING
	"Use machine specific version of strncpy")
else()
set(HAVE_ARCH_STRCPY  "0" CACHE STRING
	"Use machine specific version of strcpy")
set(HAVE_ARCH_STRNCPY "0" CACHE STRING
	"Use machine specific version of strncpy")
endif()

option(KERNEL_DEBUG "Compile the kernel with -g and -O0"  OFF)
option(MIGRATION_LOG "Enable migration events logging"  OFF)
