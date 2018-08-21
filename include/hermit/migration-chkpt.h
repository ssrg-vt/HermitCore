#ifndef MIGRATION_CHKPT_H
#define MIGRATION_CHKPT_H

#include <hermit/stddef.h>

#define CHKPT_MDATA_FILE	"mdata.bin"
#define CHKPT_STACK_FILE	"stack.bin"
#define CHKPT_BSS_FILE		"bss.bin"
#define CHKPT_DATA_FILE		"data.bin"
#define CHKPT_HEAP_FILE		"heap.bin"
#define CHKPT_TLS_FILE		"tls.bin"
#define CHKPT_FDS_FILE		"fds.bin"

/* WARNING: this should be consistent with stack transformation definition
 * Defines an abstract register set for the x86-64 ISA, used for finding data
 * and virtually unwinding the stack.  Laid out to be compatible with kernel's
 * struct pt_regs for x86-64.
 */
struct regset_x86_64
{
  /* Program counter/instruction pointer */
  void* rip;

  /* General purpose registers */
  uint64_t rax, rdx, rcx, rbx,
           rsi, rdi, rbp, rsp,
           r8, r9, r10, r11,
           r12, r13, r14, r15;

  /* Multimedia-extension (MMX) registers */
  uint64_t mmx[8];

  /* Streaming SIMD Extension (SSE) registers */
  unsigned __int128 xmm[16];

  /* x87 floating point registers */
  long double st[8];

  /* Segment registers */
  uint32_t cs, ss, ds, es, fs, gs;

  /* Flag register */
  uint64_t rflags;

  // TODO control registers
};

/* WARNING: Should be consistent with stack_transformation definition
 * Defines an abstract register set for the aarch64 ISA, used for finding data
 * and virtually unwinding the stack.  Laid out to be compatible with kernel's
 * struct pt_regs for arm64.
 */
struct regset_aarch64
{
  /* Stack pointer & program counter */
  void* sp;
  void* pc;

  /* General purpose registers */
  uint64_t x[31];

  /* FPU/SIMD registers */
  unsigned __int128 v[32];

  // TODO ELR_mode register
};

typedef struct {
	/* Offset from the base of the stack to rsp: */
	uint64_t stack_offset[MAX_TASKS];
	/* Location of the stack in the virtual address space */
	uint64_t stack_base[MAX_TASKS];
	/* List of migrated task ids: */
	tid_t task_ids[MAX_TASKS];
	uint64_t ip;			/* Instruction pointer to resume to */
	uint64_t bss_size;		/* Size of bss area to restore */
	uint64_t data_size;		/* Size of the data section to restore */
	uint64_t heap_size;		/* Heap size */
	uint64_t tls_size;		/* tls size */

	/* Callee-saved register set: x86_64 */
	uint64_t r12[MAX_TASKS];
	uint64_t r13[MAX_TASKS];
	uint64_t r14[MAX_TASKS];
	uint64_t r15[MAX_TASKS];
	uint64_t rbx[MAX_TASKS];
	uint64_t rbp[MAX_TASKS];

	/* Callee-saved register set: aarch64 */
	uint64_t x19[MAX_TASKS];
	uint64_t x20[MAX_TASKS];
	uint64_t x21[MAX_TASKS];
	uint64_t x22[MAX_TASKS];
	uint64_t x23[MAX_TASKS];
	uint64_t x24[MAX_TASKS];
	uint64_t x25[MAX_TASKS];
	uint64_t x26[MAX_TASKS];
	uint64_t x27[MAX_TASKS];
	uint64_t x28[MAX_TASKS];
	uint64_t x29[MAX_TASKS];
	uint64_t x30[MAX_TASKS];

	/* popcorn regsets TODO replace what is above */
	uint8_t popcorn_regs_valid;
	struct regset_aarch64 popcorn_arm_regs;
	struct regset_x86_64 popcorn_x86_regs;

} chkpt_metadata_t;

int migrate_chkpt_area(uint64_t addr, uint64_t size, const char *filename);
int migrate_restore_area(const char *filename, uint64_t addr, uint64_t size);
int migrate_chkpt_area_not_contiguous(uint64_t addr, uint64_t size,
		const char *filename, int mapped_on_demand);
int migrate_restore_area_not_contiguous(const char *filename, uint64_t addr,
		uint64_t size);

#endif /* MIGRATION_CHKPT_H */
