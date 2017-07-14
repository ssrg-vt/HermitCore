/* 
 * Copyright 2010 Stefan Lankes, Chair for Operating Systems,
 *                               RWTH Aachen University
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
 *
 * This code based mostly on the online manual http://www.lowlevel.eu/wiki/RTL8139
 */

#ifndef __NET_UHYVE_BLK_H__
#define __NET_UHYVE_BLK_H__

#include <hermit/stddef.h>
#include <hermit/spinlock.h>

#define MIN(a, b)	(a) < (b) ? (a) : (b)

#define UHYVE_PORT_BLKINFO	0x509
#define UHYVE_PORT_BLKWRITE	0x50a
#define UHYVE_PORT_BLKREAD      0x50b
#define UHYVE_PORT_BLKSTAT	0x50c

// UHYVE_PORT_BLKINFO
typedef struct {
        /* OUT */
        size_t sector_size;
        size_t num_sectors;
        int rw;
} __attribute__((packed)) uhyve_blkinfo_t;

// UHYVE_PORT_BLKWRITE
typedef struct {
        /* IN */
        size_t sector;
        const void* data;
        size_t len;

        /* OUT */
        int ret;
} __attribute__((packed)) uhyve_blkwrite_t;

// UHYVE_PORT_BLKREAD
typedef struct {
        /* IN */
        size_t sector;
        void* data;
        /* IN / OUT */
        size_t len;
        /* OUT */
        int ret;
} __attribute__((packed)) uhyve_blkread_t;

// UHYVE_PORT_BLKSTAT
typedef struct {
        /* IN */
        int status
} __attribute__((packed)) uhyve_blkstat_t;


static int uhyve_blk_init_ok = 0;

#endif
