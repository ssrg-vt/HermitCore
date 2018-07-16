/*
 * This file was adapted from the solo5/ukvm code base, initial copyright block
 * follows:
 */

/*
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
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

#ifndef UHYVE_GDB_H
#define UHYVE_GDB_H

#include <stdint.h>
#include <inttypes.h>

/* GDB breakpoint/watchpoint types */
typedef enum _gdb_breakpoint_type {
    /* Do not change these. The values have to match on the GDB client
     * side. */
    GDB_BREAKPOINT_SW = 0,
    GDB_BREAKPOINT_HW,
    GDB_WATCHPOINT_WRITE,
    GDB_WATCHPOINT_READ,
    GDB_WATCHPOINT_ACCESS,
    GDB_BREAKPOINT_MAX
} gdb_breakpoint_type;

#define GDB_SIGNAL_FIRST         0
#define GDB_SIGNAL_QUIT          3
#define GDB_SIGNAL_KILL          9
#define GDB_SIGNAL_TRAP          5
#define GDB_SIGNAL_SEGV          11
#define GDB_SIGNAL_TERM          15
#define GDB_SIGNAL_IO            23
#define GDB_SIGNAL_DEFAULT       144

/* prototypes */
int uhyve_gdb_enable_ss(int vcpufd);
int uhyve_gdb_disable_ss(int vcpufd);
int uhyve_gdb_read_registers(int vcpufd, uint8_t *reg, size_t *len);
int uhyve_gdb_write_registers(int vcpufd, uint8_t *reg, size_t len);
int uhyve_gdb_add_breakpoint(int vcpufd, gdb_breakpoint_type type,
		uint64_t addr, size_t len);
int uhyve_gdb_remove_breakpoint(int vcpufd, gdb_breakpoint_type type,
		uint64_t addr, size_t len);
int uhyve_gdb_guest_virt_to_phys(int vcpufd, const uint64_t virt,
		uint64_t *phys);

/* interface with uhyve.c */
void uhyve_gdb_handle_exception(int vcpufd, int sigval);
void uhyve_gdb_handle_term(void);
int uhyve_gdb_init(int vcpufd);

#ifdef __aarch64__
struct uhyve_gdb_regs {
	uint64_t x[31];
	uint64_t sp;
	uint64_t pc;
	uint32_t cpsr;
	/* we need to pack this structure so that the compiler does not insert any
	 * padding (for example to have the structure size being a multiple of 8
	 * bytes. Indeed, when asking the server for the values of the registers,
	 * gdb client looks at the size of the response (function of the size of
	 * this structure) to determine which registers are concerned. They are
	 * furthermore determine by a convention of order and sizes according to
	 * the following: https://github.com/bminor/binutils-gdb/blob/master/gdb/
	 * features/aarch64-core.xml */
} __attribute__ ((__packed__));
#else
struct uhyve_gdb_regs {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rsp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint64_t eflags;

    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
};
#endif /* ISA switch */

#endif /* UHYVE_GDB_H */
