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

} chkpt_metadata_t;

int migrate_chkpt_area(uint64_t addr, uint64_t size, const char *filename);
int migrate_restore_area(const char *filename, uint64_t addr, uint64_t size);
int migrate_chkpt_area_not_contiguous(uint64_t addr, uint64_t size,
		const char *filename, int mapped_on_demand);
int migrate_restore_area_not_contiguous(const char *filename, uint64_t addr,
		uint64_t size);

#endif /* MIGRATION_CHKPT_H */
