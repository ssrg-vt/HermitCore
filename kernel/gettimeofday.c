#include <hermit/syscall.h>
#include <hermit/processor.h>
#include <hermit/errno.h>
#include <asm/processor.h>
#include <asm/string.h>

extern uint64_t boot_gtod;
static uint64_t freq;

struct timeval {
	uint64_t tv_sec;
	uint64_t tv_usec;
};

void gettimeofday_init(void) {
	freq = get_cpu_frequency() * 1000000ULL;
}

int sys_gettimeofday(struct timeval *tv, void *tz) {

	if(tv) {
		uint64_t ts = 0;
#ifdef __aarch64__
		uint64_t boot_gtod_tmp;

		/* For some reason we get a relocation error from gold when we try to
		 * directly use boot_gtod. This hack copies it first in a temporary
		 * variable */
		memcpy(&boot_gtod_tmp, &boot_gtod, sizeof(uint64_t));

		ts = get_cntpct();
		uint64_t ts_us = ts/(freq/1000000ULL);
		tv->tv_sec = (boot_gtod_tmp+ts_us)/1000000ULL;
		tv->tv_usec = (boot_gtod_tmp+ts_us)%1000000ULL;
#else
		ts = rdtsc();
		uint64_t ts_us = ts/(freq/1000000ULL);
		tv->tv_sec = (boot_gtod+ts_us)/1000000ULL;
		tv->tv_usec = (boot_gtod+ts_us)%1000000ULL;
#endif
	}

	if(tz)
		return -ENOSYS;

	return 0;
}
