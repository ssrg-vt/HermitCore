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

#define _FOPEN          (-1)    /* from sys/file.h, kernel use only */
#define _FREAD          0x0001  /* read enabled */
#define _FWRITE         0x0002  /* write enabled */
#define _FAPPEND        0x0008  /* append (writes guaranteed at the end) */
#define _FMARK          0x0010  /* internal; mark during gc() */
#define _FDEFER         0x0020  /* internal; defer for next gc pass */
#define _FASYNC         0x0040  /* signal pgrp when data ready */
#define _FSHLOCK        0x0080  /* BSD flock() shared lock present */
#define _FEXLOCK        0x0100  /* BSD flock() exclusive lock present */
#define _FCREAT         0x0200  /* open with file create */
#define _FTRUNC         0x0400  /* open with truncation */
#define _FEXCL          0x0800  /* error on open if file exists */
#define _FNBIO          0x1000  /* non blocking I/O (sys5 style) */
#define _FSYNC          0x2000  /* do all writes synchronously */
#define _FNONBLOCK      0x4000  /* non blocking I/O (POSIX style) */
#define _FNDELAY        _FNONBLOCK      /* non blocking I/O (4.2 style) */
#define _FNOCTTY        0x8000  /* don't assign a ctty on this open */

/*open flags*/
#define O_RDONLY        0
#define O_WRONLY        1
#define O_RDWR          2
#define O_APPEND        _FAPPEND
#define O_CREAT         _FCREAT
#define O_TRUNC         _FTRUNC
#define O_EXCL          _FEXCL
#define O_SYNC          _FSYNC
#define O_NONBLOCK      _FNONBLOCK
#define O_NOCTTY        _FNOCTTY

extern unsigned int get_cpufreq(void);

inline static unsigned long long rdtsc(void)
{
	unsigned long lo, hi;
	asm volatile ("rdtsc" : "=a"(lo), "=d"(hi) :: "memory");
	return ((unsigned long long) hi << 32ULL | (unsigned long long) lo);
}

int main(int argc, char** argv) {

	int err = 0;
	int fd;
	int fd2;
	uint64_t start, end;
	uint32_t freq = get_cpufreq(); /* in MHz */
	int number = 50;
	int num_files = 26;
	int blf = 12;
	int sector_size = hermit_blk_sector_size();
	uint8_t wbuf_sector[sector_size];
	uint8_t rbuf_sector[sector_size];
	uint8_t wbuf_blocklist[sector_size*blf];
	uint8_t rbuf_blocklist[sector_size*blf];
	uint8_t wbuf_bigger[sector_size*number];
	uint8_t rbuf_bigger[sector_size*number];

	printf("Starting filesystem test\n\n");

	// create a directory
	printf("create directory '/tmp/fstest': ");
	if(!(err = fs_mkdir("/tmp/fstest"))) {
		printf("created\n");
	} else {
		printf("error %i\n", err);
		goto error;
	}

	// create a file in this directory
	printf("create empty file '/tmp/fstest/testfile.txt': ");
	if(fd = fs_open("/tmp/fstest/testfile.txt", O_CREAT)) {
		printf("created\n");
	} else {
		printf("failed\n");
		goto error;
	}

	// write into the file
	printf("write 20 Bytes (testbytesfortestfile) into '/tmp/fstest/testfile.txt': ");
	err = fs_write(fd, "testbytesfortestfile\0", 21);
	if (err == 21) {
		printf("%i bytes writen\n", err);
	} else {
		printf("error %i\n", err);
		goto error;
	}

	// close the file
	printf("close file '/tmp/fstest/testfile.txt': ");
	fs_close(fd);
	printf("closed\n");

	// open the file
	printf("open file '/tmp/fstest/testfile.txt': ");
	if(fd = fs_open("/tmp/fstest/testfile.txt", O_EXCL)) {
		printf("opened\n");
	} else {
		printf("failed\n");
		goto error;
	}
	printf("\n");

	// read from the file
	printf("read 20 Bytes from '/tmp/fstest/testfile.txt': ");
	char buffer20[20];
	err = fs_read(fd, buffer20, 20);
	if (err == 20) {
		printf("%s\n", buffer20);
	} else {
		printf("error %i\n", err);
	}

	// close the file
	printf("close file '/tmp/fstest/testfile.txt': ");
	fs_close(fd);
	printf("closed\n");
	printf("\n");

	// open the file to append
	printf("open file '/tmp/fstest/testfile.txt': ");
	if(fd = fs_open("/tmp/fstest/testfile.txt", O_APPEND)) {
		printf("opened\n");
	} else {
		printf("failed\n");
		goto error;
	}

	// append bytes to the file
	printf("append 6 Bytes (append) into '/tmp/fstest/testfile.txt': ");
	err = fs_write(fd, "append\0", 7);
	if (err == 7) {
		printf("%i bytes writen\n", err);
	} else {
		printf("error %i\n", err);
		goto error;
	}

	// close the file
	printf("close file '/tmp/fstest/testfile.txt': ");
	fs_close(fd);
	printf("closed\n");
	printf("\n");

	// open the file
	printf("open file '/tmp/fstest/testfile.txt': ");
	if(fd = fs_open("/tmp/fstest/testfile.txt", O_EXCL)) {
		printf("created\n");
	} else {
		printf("failed\n");
		goto error;
	}

	// read from the file
	printf("read 26 Bytes from '/tmp/fstest/testfile.txt': ");
	char buffer26[26];
	err = fs_read(fd, buffer26, 26);
	if (err == 26) {
		printf("%s\n", buffer26);
	} else {
		printf("error %i\n", err);
	}

	// close the file
	printf("close file '/tmp/fstest/testfile.txt': ");
	fs_close(fd);
	printf("closed\n");
	printf("\n");


	// open the file to overwrite 
	printf("open file to overwrite '/tmp/fstest/testfile.txt': ");
	if(fd = fs_open("/tmp/fstest/testfile.txt", 0)) {
		printf("opened\n");
	} else {
		printf("failed\n");
		goto error;
	}

	// write into the file
	printf("overwrite first 9 Bytes (overwrite) into '/tmp/fstest/testfile.txt': ");
	err = fs_write(fd, "overwrite", 9);
	if (err == 9) {
		printf("%i bytes writen\n", err);
	} else {
		printf("error %i\n", err);
		goto error;
	}

	// close the file
	printf("close file '/tmp/fstest/testfile.txt': ");
	fs_close(fd);
	printf("closed\n");
	printf("\n");

	// open the file
	printf("open file '/tmp/fstest/testfile.txt': ");
	if(fd = fs_open("/tmp/fstest/testfile.txt", O_EXCL)) {
		printf("opened\n");
	} else {
		printf("failed\n");
		goto error;
	}

	// read from the file
	printf("read 26 Bytes from '/tmp/fstest/testfile.txt': ");
	err = fs_read(fd, buffer26, 26);
	if (err == 26) {
		printf("%s\n", buffer26);
	} else {
		printf("error %i\n", err);
	}

	// close the file
	printf("close file '/tmp/fstest/testfile.txt': ");
	fs_close(fd);
	printf("closed\n");
	printf("\n");
/*
	// truncate the file
	printf("truncate the file /tmp/fstest/testfile.txt': ");
	if(fd = fs_open("/tmp/fstest/testfile.txt", O_TRUNC)) {
		printf("truncated\n");
	} else {
		printf("failed\n");
		goto error;
	}

	// close the file
	printf("close file '/tmp/fstest/testfile.txt': ");
	fs_close(fd);
	printf("closed\n");
	printf("\n");
*/
	/* testing file which needs the whole sector*/

	for (int i = 0; i < sector_size; i++) {
		wbuf_sector[i] = 'a' + i % 26;
		rbuf_sector[i] = '0';
	}
//	kprintf needs '\0'  to find the end of the string, otherwise kprintf reads the following bits in the memory
	wbuf_sector[sector_size -1] = '\0';
	rbuf_sector[sector_size -1] = '\0';

	start = rdtsc();
	// create a file in this directory
	printf("create empty file '/tmp/fstest/sectorfile.txt': ");
	if(fd = fs_open("/tmp/fstest/sectorfile.txt", O_CREAT)) {
		printf("created\n");
	} else {
		printf("failed\n");
		goto error;
	}

	// write into the file
	printf("write %i Bytes ( 1 sector ) into '/tmp/fstest/sectorfile.txt': ", sector_size);
	err = fs_write(fd, wbuf_sector, sector_size);
	if (err == sector_size) {
		printf("%i bytes writen\n wbuf:\n%s\n\n rbuf:\n%s\n\n", err, wbuf_sector, rbuf_sector);
	} else {
		printf("error %i\n", err);
		goto error;
	}

	// close the file
	printf("close file '/tmp/fstest/sectorfile.txt': ");
	fs_close(fd);
	printf("closed\n");
	end = rdtsc();
	// To calculate MB/s with the given bytes and nano seconds we use 1(s)/1(MB) = 1000*1000*1000(ns)/1024*1024(B) = 953,6743164 =~ 954
	printf("Time to write a file with %llu bytes: %llu nsec (ticks %llu)\n => %llu MB/s\n\n", sector_size, ((end-start)*1000ULL)/freq, end-start, sector_size*954/(((end-start)*1000ULL)/freq));
	start = rdtsc();
	// open the file
	printf("open file '/tmp/fstest/sectorfile.txt': ");
	if(fd = fs_open("/tmp/fstest/sectorfile.txt", O_EXCL)) {
		printf("opened\n");
	} else {
		printf("failed\n");
		goto error;
	}
	printf("\n");

	// read from the file
	printf("read %i Bytes from '/tmp/fstest/sectorfile.txt': ", sector_size);
	err = fs_read(fd, rbuf_sector, sector_size);
	if (err == sector_size) {
		printf("%i Bytes read, rbuf:\n%s\n\n", err, rbuf_sector);
	} else {
		printf("error %i\n", err);
	}

	// close the file
//	printf("close file '/tmp/fstest/sectorfile.txt': ");
	fs_close(fd);
//	printf("closed\n");
	printf("\n");
	end = rdtsc();
	printf("Time to read a file with %llu bytes: %llu nsec (ticks %llu)\n => %llu MB/s\n\n", sector_size, ((end-start)*1000ULL)/freq, end-start, sector_size*954/(((end-start)*1000ULL)/freq));

	/* testing file which needs 12 sektors -> 1 blocklist */

/**/
	for (int i = 0; i < sector_size*blf; i++) {
		wbuf_blocklist[i] = 'A' + i % 26;
		rbuf_blocklist[i] = '0';
	}
//	kprintf needs '\0'  to find the end of the string, otherwise kprintf reads the following bits in the memory
	wbuf_blocklist[sector_size*blf -1] = '\0';
	rbuf_blocklist[sector_size*blf -1] = '\0';

	start = rdtsc();
	// create a file in this directory
//	printf("create empty file '/tmp/fstest/blocklistfile.txt': ");
	if(fd = fs_open("/tmp/fstest/blocklistfile.txt", O_CREAT)) {
//		printf("created\n");
	} else {
		printf("failed\n");
		goto error;
	}

	// write into the file
//	printf("write %i Bytes ( 12 sectors, 1 Blocklist ) into '/tmp/fstest/blocklistfile.txt': ", sector_size*blf);
	err = fs_write(fd, wbuf_blocklist, sector_size*blf);
	if (err == sector_size*blf) {
//		printf("%i bytes writen\n wbuf:\n%s\n\n rbuf:\n%s\n\n", err, wbuf_blocklist, rbuf_blocklist);
	} else {
		printf("error %i\n", err);
		goto error;
	}

	// close the file
//	printf("close file '/tmp/fstest/blocklistfile.txt': ");
	fs_close(fd);
//	printf("closed\n");
	end =rdtsc();
	printf("Time to write a file with %llu bytes: %llu nsec (ticks %llu)\n => %llu MB/s\n\n", sector_size*blf, ((end-start)*1000ULL)/freq, end-start, sector_size*blf*954/(((end-start)*1000ULL)/freq));

	start = rdtsc();
	// open the file
//	printf("open file '/tmp/fstest/blocklistfile.txt': ");
	if(fd = fs_open("/tmp/fstest/blocklistfile.txt", O_EXCL)) {
//		printf("opened\n");
	} else {
		printf("failed\n");
		goto error;
	}
//	printf("\n");

	// read from the file
//	printf("read %i Bytes from '/tmp/fstest/blocklistfile.txt': ", sector_size*blf);
	err = fs_read(fd, rbuf_blocklist, sector_size*blf);
	if (err == sector_size*blf) {
//		printf("%i Bytes read, rbuf:\n%s\n\n", err, rbuf_blocklist);
	} else {
		printf("error %i\n", err);
		goto error;
	}

	// close the file
//	printf("close file '/tmp/fstest/blocklistfile.txt': ");
	fs_close(fd);
//	printf("closed\n");
//	printf("\n");
	end = rdtsc();
	printf("Time to read a file with %llu bytes: %llu nsec (ticks %llu)\n => %llu MB/s\n\n", sector_size*blf, ((end-start)*1000ULL)/freq, end-start, sector_size*blf*954/(((end-start)*1000ULL)/freq));


	/* testing file which needs $number sektors -> more blocklist */

/**/
	for (int i = 0; i < sector_size*number; i++) {
		wbuf_bigger[i] = 'A' + i % 10;
		rbuf_bigger[i] = '0';
	}
//	kprintf needs '\0'  to find the end of the string, otherwise kprintf reads the following bits in the memory
	wbuf_bigger[sector_size*number -1] = '\0';
	rbuf_bigger[sector_size*number -1] = '\0';

	// write into the file
	int bl = number / 12;
	if (number % 12) 
		bl += 1;
	char filename[29];
	strncpy(filename, "/tmp/fstest/biggerfile000.txt", 29);
	start = rdtsc();
	for (int t = 0; t < num_files; t++) {
		filename[24] = '0' + t % 10;
		filename[23] = '0' + t/10 % 10;
		filename[22] = '0' + t/100 % 10;
//		printf("%s\n", filename);
		// create a file in this directory

		if(fd2 = fs_open(filename, O_CREAT)) {
//			printf("create empty file '%s': ", filename);
//			printf("created\n");
		} else {
			printf("create empty file '%s': ", filename);
			printf("failed\n");
		goto error;
		}

		err = fs_write(fd2, wbuf_bigger, sector_size*number);
		if (err == sector_size*number) {
//			printf("write %i Bytes ( %i sectors, %i Blocklist ) into '%s': ", sector_size*number, number, bl, filename);
//			printf("%i bytes writen\n", err, wbuf_bigger, rbuf_bigger);
		} else {
			printf("write %i Bytes ( %i sectors, %i Blocklist ) into '%s': ", sector_size*number, number, bl, filename);
			printf("error %i\n", err);
			goto error;
		}



		// close the file
//		printf("close file '%s': ", filename);
		fs_close(fd2);
//		printf("closed\n");
	}
	end = rdtsc();
	printf("Time to write %i files (%llu bytes): %llu nsec (ticks %llu)\n => %llu MB/s\n\n", num_files, sector_size*number*num_files, ((end-start)*1000ULL)/freq, end-start, sector_size*number*num_files*954/(((end-start)*1000ULL)/freq));

	start = rdtsc();
	for (int t = 0; t < num_files; t++) {
		filename[24] = '0' + t % 10;
		filename[23] = '0' + t/10 % 10;
		filename[22] = '0' + t/100 % 10;
//		printf("%s\n", filename);

//		printf("\n");
		// open the file
		if(fd2 = fs_open(filename, O_EXCL)) {
//			printf("open file '%s': ", filename);
//			printf("opened\n");
		} else {
			printf("open file '%s': ", filename);
			printf("failed\n");
			goto error;
		}

		// read from the file
		err = fs_read(fd2, rbuf_bigger, sector_size*number);
		if (err == sector_size*number) {
//			printf("read %i Bytes from '%s': ", sector_size*number, filename);
//			printf("%i Bytes read, success\n", err);
		} else {
			printf("read %i Bytes from '%s': ", sector_size*number, filename);
			printf("error %i\n", err);
			goto error;
		}


		// close the file
//		printf("close file '%s': ", filename);
		fs_close(fd);
//		printf("closed\n");
//		printf("\n");
	}
	end = rdtsc();
	printf("Time to read %i files (%llu bytes): %llu nsec (ticks %llu)\n => %llu MB/s\n\n", num_files, sector_size*number*num_files, ((end-start)*1000ULL)/freq, end-start, sector_size*number*num_files*954/(((end-start)*1000ULL)/freq));

	for (int i = 0; i < sector_size*12; i++) {
		wbuf_blocklist[i] = 'a' + i % 26;
		rbuf_blocklist[i] = '0';
	}
//	kprintf needs '\0'  to find the end of the string, otherwise kprintf reads the following bits in the memory
	wbuf_blocklist[sector_size*12 -1] = '\0';
	rbuf_blocklist[sector_size*12 -1] = '\0';

	// create a file in this directory
//	printf("create empty file '/tmp/fstes/biggestfile.txt': ");
	if(fd = fs_open("/tmp/fstest/biggestfile.txt", O_CREAT)) {
//		printf("created\n");
	} else {
		printf("failed\n");
		goto error;
	}

	start = rdtsc();
	int l = 0;
	for (l; l < 75; l++) {
		// write into the file
		//	printf("write %i Bytes ( 12 sectors, 1 Blocklist ) into '/tmp/fstest/blocklistfile.txt': ", sector_size*12);
		err = fs_write(fd, wbuf_blocklist, sector_size*12);
		if (err == sector_size*12) {
	//		printf("%i bytes writen\n wbuf:\n%s\n\n rbuf:\n%s\n\n", err, wbuf_blocklist, rbuf_blocklist);
		} else {
			printf("error %i\n", err);
			goto error;
		}
	}
	end = rdtsc();
	printf("Time to write %llu bytes: %llu nsec (ticks %llu)\n => %llu MB/s\n\n", l*12*sector_size, ((end-start)*1000ULL)/freq, end-start, sector_size*l*12*954/(((end-start)*1000ULL)/freq));
	// close the file
//	printf("close file '/tmp/fstest/biggestfile.txt': ");
	fs_close(fd);
//	printf("closed\n");

	// open the file
//	printf("open file '/tmp/fstest/biggestfile.txt': ");
	if(fd = fs_open("/tmp/fstest/biggestfile.txt", O_EXCL)) {
//		printf("opened\n");
	} else {
		printf("failed\n");
		goto error;
	}
//	printf("\n");
	l = 0;
	start = rdtsc();
	for (l; l < 75; l++) {

		// read from the file
//		printf("read %i Bytes from '/tmp/fstest/blocklistfile.txt': ", sector_size*12);
		err = fs_read(fd, rbuf_blocklist, sector_size*12);
		if (err == sector_size*12) {
//			printf("%i Bytes read, rbuf:\n%s\n\n", err, rbuf_blocklist);
		} else {
			printf("error %i\n", err);
			goto error;
		}
	}
	end = rdtsc();
	printf("Time to read %llu bytes: %llu nsec (ticks %llu)\n => %llu MB/s\n\n", l*12*sector_size, ((end-start)*1000ULL)/freq, end-start, sector_size*l*12*954/(((end-start)*1000ULL)/freq));

	// close the file
//	printf("close file '/tmp/fstest/blocklistfile.txt': ");
	fs_close(fd);
//	printf("closed\n");
//	printf("\n");

/**/
//	list_fs(50, 3);


	printf("\n\n");
	printf("SUCCESS\n\n");

	return 0;

error:
	printf("\n\n");
	printf("FAILED\n\n");
	return -1;
}
