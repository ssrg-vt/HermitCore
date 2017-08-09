/*
 * Copyright (c) 2010, Stefan Lankes, RWTH Aachen University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the University nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

#define SECTOR_SIZE	512

/*space fpr 2 sectors for edge-case tests */
extern unsigned int get_cpufreq(void);

inline static unsigned long long rdtsc(void)
{
	unsigned long lo, hi;
	asm volatile ("rdtsc" : "=a"(lo), "=d"(hi) :: "memory");
	return ((unsigned long long) hi << 32ULL | (unsigned long long) lo);
}

static uint8_t wbuf[SECTOR_SIZE * 2];
static uint8_t rbuf[SECTOR_SIZE * 2];

int check_sector_write(uint64_t sector)
{
	int rlen = SECTOR_SIZE;
	unsigned i;


	for (i = 0; i < SECTOR_SIZE; i++) {
		wbuf[i] = '0' + i % 10;
		rbuf[i] = 0;
	}

	if (hermit_blk_write_sync(sector, wbuf, SECTOR_SIZE) != 0)
		return -1;

	if (hermit_blk_read_sync(sector, rbuf, &rlen) != 0)
		return -2;

	if (rlen != SECTOR_SIZE)
		return -3;

	for(i = 0; i < SECTOR_SIZE; i++) {
		if (rbuf[i] != '0' + i % 10)
			/*Check failed */
			return -4;
	}

	return 0;
}
int check_write_speed(uint64_t sector)
{
	int rlen = SECTOR_SIZE;
	unsigned i;

	for (i = 0; i < SECTOR_SIZE; i++) {
		wbuf[i] = 'a' + i % 26;
	}

	if (hermit_blk_write_sync(sector, wbuf, SECTOR_SIZE) != 0)
		return -1;

	return 0;
}
int check_read_speed(uint64_t sector)
{
	int rlen = SECTOR_SIZE;
	unsigned i;

	if (hermit_blk_read_sync(sector, rbuf, &rlen) != 0)
		return -2;

	if (rlen != SECTOR_SIZE)
		return -3;

	for(i = 0; i < SECTOR_SIZE; i++) {
		if (rbuf[i] != 'a' + i % 26)
			/*Check failed */
			return -4;
	}

	return 0;
}


int main(int argc, char** argv) {
	size_t i, nsectors;
	int rlen, err;
	uint64_t start, end;
	uint32_t freq = get_cpufreq(); /* in MHz */

	printf("\n**** Hermit standalone test_blk ****\n\n");

	/* Write and read/check one tenth of the disk. */
	nsectors = hermit_blk_sectors();

	for (i = 0; i < nsectors; i += 1) {
		if ((err = check_sector_write(i)) < 0) {
			printf("check_sector_write() failed in sector %i : error %i\n", i, err);
			return -1;
		}
/*		p =  (i*100 / nsectors);
		if (p > 2*j) {
			printf(".");
			j++; 
		}*/

	}
	printf("\nall sectors writable\n\n");
	start = rdtsc();
	for (i = 0; i < nsectors; i += 1) {
		if ((err = check_write_speed(i)) < 0) {
			printf("check_write_speed() failed in sector %i : error %i\n", i, err);
			return -1;
		}

	}
	/* Check edge case: read/write of last sector on the device. */

	end = rdtsc();
	printf("Time to write %llu bytes: %llu nsec (ticks %llu)\n => %llu MB/s\n", i*SECTOR_SIZE, ((end-start)*1000ULL)/freq, end-start, i*SECTOR_SIZE*954/(((end-start)*1000ULL)/freq));
	start = rdtsc();
	for (i = 0; i < nsectors; i += 1) {
		if ((err = check_read_speed(i)) < 0) {
			printf("check_read_speed() failed in sector %i : error %i\n", i, err);
			return -1;
		}

	}
	printf("\n");
	/* Check edge case: read/write of last sector on the device. */
	end = rdtsc();
	printf("Time to read and check %llu bytes: %llu nsec (ticks %llu)\n => %llu MB/s\n", i*SECTOR_SIZE, ((end-start)*1000ULL)/freq, end-start, i*SECTOR_SIZE*954/(((end-start)*1000ULL)/freq));

	if (hermit_blk_write_sync(nsectors - 1, wbuf, SECTOR_SIZE ) != 0) {
		printf("check edge cases: write on last sector failed");
		return -2;
	}
	rlen = SECTOR_SIZE;
	if (hermit_blk_read_sync(nsectors -1, rbuf, &rlen ) != 0) {
		printf("check edge cases: read on last sector failed");
		return -3;
	}
	if (rlen != SECTOR_SIZE) {
		printf("check edge cases: too less bytes read");
		return -4;
	}

	/* Check edge cases: should not be able to read or write beyond end of device. */

	if (hermit_blk_write_sync(nsectors - 1, wbuf, (2 * SECTOR_SIZE)) >= 0) {
		printf("check edge cases: writed on sector behind device");
		return -5;
	}

	rlen = 2 * SECTOR_SIZE;
	printf(".");
	if (hermit_blk_read_sync(nsectors - 1, rbuf, &rlen) >= 0) {
		printf("check edge cases: read on sector behind device");
		return -6;
	}

	printf("\n\n");
	printf("SUCCESS\n\n");

	return 0;
}
