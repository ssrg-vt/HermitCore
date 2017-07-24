#ifndef __UHYVE_BLK_H__
#define __UHYVE_BLK_H__

#endif

#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/stat.h>

/* network interface */
#include <fcntl.h>
#include <sys/ioctl.h>
#include <err.h>

#include "uhyve-cpu.h"

#define __HALF_MAX_SIGNED(type) ((type)1 << (sizeof(type) * 8 - 2))
#define __MAX_SIGNED(type) (__HALF_MAX_SIGNED(type) - 1 + __HALF_MAX_SIGNED(type))
#define __MIN_SIGNED(type) (-1 - __MAX_SIGNED(type))

#define __MIN(type) ((type)-1 < 1 ? __MIN_SIGNED(type) : (type)0)
#define __MAX(type) ((type)~__MIN(type))

#define __assign(dest,src)                                                     \
    ({                                                                         \
        __typeof(src) __x = (src);                                             \
        __typeof(dest) __y = __x;                                              \
        (__x == __y && ((__x < 1) == (__y < 1)) ? (void)((dest) = __y),0 : 1); \
})

#define add_overflow(a,b,r)                                                    \
    ({                                                                         \
    __typeof(a) __a = a;                                                       \
    __typeof(b) __b = b;                                                       \
    (__b) < 1 ?                                                                \
    ((__MIN(__typeof(r)) - (__b) <= (__a)) ? __assign(r, __a + __b) : 1) :     \
    ((__MAX(__typeof(r)) - (__b) >= (__a)) ? __assign(r, __a + __b) : 1);      \
})

#define HERMIT_BLK_BS 512

static char *blk;
static int diskfd;

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
        int status;
} __attribute__((packed)) uhyve_blkstat_t;

static int uhyve_blk_init();
