/* Copyright (c) 2015, IBM
 * Author(s): Dan Williams <djwillia@us.ibm.com>
 *            Ricardo Koller <kollerr@us.ibm.com>
 * Copyright (c) 2017, RWTH Aachen University
 * Author(s): Stefan Lankes <slankes@eonerc.rwth-aachen.de>
 *            Tim van de Kamp <tim.van.de.kamp@rwth-aachen.de>
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* We used several existing projects as guides
 * kvmtest.c: http://lwn.net/Articles/658512/
 * lkvm: http://github.com/clearlinux/kvmtool
 */

/*
 * 15.1.2017: extend original version (https://github.com/Solo5/solo5)
 *            for HermitCore
 */


#include <hermit/stddef.h>
#include <hermit/stdio.h>
#include <hermit/tasks.h>
#include <hermit/errno.h>
#include <hermit/syscall.h>
#include <hermit/spinlock.h>
#include <hermit/semaphore.h>
#include <hermit/time.h>
#include <hermit/rcce.h>
#include <hermit/memory.h>
#include <hermit/signal.h>
#include <hermit/mailbox.h>
#include <hermit/logging.h>
#include <asm/io.h>
#include <lwip/sys.h>
#include <lwip/err.h>
#include <lwip/stats.h>

#include "uhyve-blk.h"


int hermit_blk_write_sync(uint64_t sec, uint8_t *data, int n)
{
	volatile uhyve_blkwrite_t uhyve_blkwrite;
	uhyve_blkwrite.sector = sec;
	uhyve_blkwrite.data = (uint8_t*)virt_to_phys((size_t)data);
	uhyve_blkwrite.len = n;
	uhyve_blkwrite.ret = 0;

	outportl(UHYVE_PORT_BLKWRITE, (unsigned)virt_to_phys((size_t)&uhyve_blkwrite));

	return uhyve_blkwrite.ret;
}
int hermit_blk_stat() {
        volatile uhyve_blkstat_t uhyve_blkstat;
        outportl(UHYVE_PORT_BLKSTAT, (unsigned)virt_to_phys((size_t)&uhyve_blkstat));

        return uhyve_blkstat.status;
}

int hermit_blk_read_sync(uint64_t sec, uint8_t *data, int *n)
{
	volatile uhyve_blkread_t uhyve_blkread;
	uhyve_blkread.sector = sec;
	uhyve_blkread.data = (uint8_t*)virt_to_phys((size_t)data);
	uhyve_blkread.len = *n;
	uhyve_blkread.ret = 0;

	outportl(UHYVE_PORT_BLKREAD, (unsigned)virt_to_phys((size_t)&uhyve_blkread));

	*n = uhyve_blkread.len;
	return uhyve_blkread.ret;
}

int hermit_blk_sector_size(void)
{
	volatile uhyve_blkinfo_t uhyve_blkinfo;

	outportl(UHYVE_PORT_BLKINFO, (unsigned)virt_to_phys((size_t)&uhyve_blkinfo));

	return uhyve_blkinfo.sector_size;
}

int hermit_blk_sectors(void)
{
	volatile uhyve_blkinfo_t uhyve_blkinfo;

	outportl(UHYVE_PORT_BLKINFO, (unsigned)virt_to_phys((size_t)&uhyve_blkinfo));

	return uhyve_blkinfo.num_sectors;
}

int hermit_blk_rw(void)
{
	volatile uhyve_blkinfo_t uhyve_blkinfo;

	outportl(UHYVE_PORT_BLKINFO, (unsigned)virt_to_phys((size_t)&uhyve_blkinfo));

	return uhyve_blkinfo.rw;
}
