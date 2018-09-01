#include <asm/atomic.h>
#include <asm/uhyve.h>
#include <hermit/tasks_types.h>
#include <hermit/logging.h>
#include <hermit/syscall.h>
#include <asm/page.h>
#include <hermit/migration-chkpt.h>
#include <hermit/string.h>
#include <asm/tasks.h>
#include <hermit/migration-fd.h>
#include <hermit/errno.h>
#include <hermit/stdio.h>
#include <hermit/tasks.h>

/* Set this to 1 and use the following env. variables to disable remote memory:
 * HERMIT_MIGRATE_PORT=0 (source), HERMIT_MIGRATE_SERVER=0 (target), and don't
 * forget to transfer the full checkpoint */
#define REMOTE_SERVER_DISABLED 0

#ifdef __aarch64__
#include <hermit/migration-aarch64-regs.h>
#else
#include <hermit/migration-x86-regs.h>
#endif

#include <hermit/migration.h>

#ifdef __aarch64__
#define current_stack_pointer ({ \
	uint64_t sp; \
	asm volatile("mov x15, sp; str x15, %0" : "=m" (sp) : : "x15"); \
	sp; \
})
#else
#define current_stack_pointer ({ \
	register unsigned long rsp asm("rsp"); \
	asm("" : "=r"(rsp)); \
	rsp; \
})
#endif

/* The data structure we pass to uhyve when we ask for migration */
typedef struct {
	uint64_t heap_size;
	uint64_t bss_size;
} __attribute__ ((packed)) uhyve_migration_t;

/* atomic variable set to 1 when application should migrate */
extern atomic_int32_t should_migrate;

/* are we resuming from migration? */
extern uint32_t mig_resuming;

/* markers for the address space areas */
extern const void kernel_start;
extern const void __bss_start;
extern const void __data_start;
extern const void __data_end;

/* Main Core on which we put the background thread pulling remote pages  */
extern uint32_t boot_processor;

/* Keep track of the number of threads running. When a thread enters the
 * migration code, there is a barrier waiting for all running threads to reach
 * a migration point, i.e. to also enterr migration code. During that period
 * some threads might exit, and others might be created, so this variable
 * is atomically updated from the thread creation/destruction code in task.c.
 * The barrier is impelmented as follows: every thread entering migration
 * code decrements that variable, and when it is 0 the barrier is reached and
 * threads can proceed. */
atomic_int32_t running_threads;

/* Track secondary threads ready for migration. Once a thread enters migration
 * code we increment this variable to keep track of how many threads actually
 * entered migration. This is later used to implement a second barrier to
 * synchronize everyone at the end of the migration code. */
static atomic_int32_t sec_threads_ready;

/* Track threads ready on the resume path */
static atomic_int32_t threads_to_resume;

/* Keep track of primary thread id */
static int primary_thread_id = -1;

/* Contains migration metadata */
chkpt_metadata_t md;

void init_threads_to_resume(int num_threads) {
	atomic_int32_set(&threads_to_resume, num_threads);
}

void set_primary_thread_id(int id) {
	MIGLOG("Primary thread id is %d\n", id);
	primary_thread_id = id;
}

void incr_running_threads(void) {
	atomic_int32_inc(&running_threads);
}

void dec_running_threads(void) {
	atomic_int32_dec(&running_threads);
}

static int restore_data(uint64_t data_size) {
	MIGLOG("Restore data from 0x%llx, size 0x%llx\n", (size_t)&__data_start,
			data_size);

	return migrate_restore_area(CHKPT_DATA_FILE, (size_t)&__data_start,
		data_size);
}

static int restore_bss(uint64_t bss_size) {
	MIGLOG("Restore bss from 0x%llx, size 0x%llx\n", (size_t)&__bss_start,
			bss_size);

	return migrate_restore_area(CHKPT_BSS_FILE, (size_t)&__bss_start, bss_size);
}

static int restore_heap(uint64_t heap_start, uint64_t heap_size) {
	size_t heap = HEAP_START;
	task_t* curr_task = per_core(current_task);

	if (!curr_task->heap)
		curr_task->heap = (vma_t*) kmalloc(sizeof(vma_t));

	if (BUILTIN_EXPECT(!curr_task->heap, 0)) {
		MIGERR("restore_heap: heap is missing!\n");
		return -1;
	}

	curr_task->heap->flags = VMA_HEAP|VMA_USER;
	curr_task->heap->start = PAGE_CEIL(heap);
	curr_task->heap->end = PAGE_CEIL(heap);

	/* Check that the heap is located at the same address as previously */
	if(curr_task->heap->start != heap_start) {
		MIGERR("Heap starts at 0x%x but was previously located at 0x%x\n",
                                curr_task->heap->start, heap_start);
		return -EINVAL;
	}

	// region is already reserved for the heap, we have to change the
	// property of the first page
	vma_free(curr_task->heap->start, curr_task->heap->start+PAGE_SIZE);
	vma_add(curr_task->heap->start, curr_task->heap->start+PAGE_SIZE,
			VMA_HEAP|VMA_USER);

	if(sys_sbrk(heap_size) == -ENOMEM) {
		MIGERR("Cannot reallocate heap, out of memory\n");
		return -1;
	}

	/* We just write here info about the migrated heap, and will actually
	* populate the content later (see page_fault_handler in mm/page.c */
	curr_task->migrated_heap = heap_size;

#if REMOTE_SERVER_DISABLED == 1
	/* Good old checkpointed heap */
	curr_task->migrated_heap = 0;
	return migrate_restore_area_not_contiguous(CHKPT_HEAP_FILE,
						curr_task->heap->start, heap_size);
#endif

	return 0;
}

int checkpoint_heap(void) {

	task_t* task = per_core(current_task);

	MIGLOG("Checkpoint heap from 0x%llx, size 0x%llx\n", task->heap->start,
			task->heap->end - task->heap->start);

	return migrate_chkpt_area_not_contiguous(task->heap->start,
			task->heap->end - task->heap->start, CHKPT_HEAP_FILE, 1);
}

int restore_tls(uint64_t tls_size, int mig_id) {
	uint64_t local_tls_start;
	char tls_chkpt_file[32];

	if(tls_size) {
		if(init_tls()) {
			MIGERR("Migration: cannot allocate TLS\n");
			return -1;
		}
#ifdef __aarch64__
		thread_block_t *tcb = (thread_block_t *)get_tpidr();
		local_tls_start = (uint64_t)tcb->dtv;
#else
		local_tls_start = get_tls() - tls_size;
#endif
		ksprintf(tls_chkpt_file, "%s.%d", CHKPT_TLS_FILE, mig_id);
		MIGLOG("Restoring TLS - start: 0x%llx, size: 0x%llx from %s\n",
				local_tls_start, tls_size, tls_chkpt_file);

		return migrate_restore_area(tls_chkpt_file, local_tls_start,
				tls_size);
	}

	return 0;
}

int save_stack(uint64_t stack_pointer) {
	char stack_chkpt_file[32];
	task_t *task = per_core(current_task);
	uint64_t stack_base = (uint64_t)task->stack;
	uint64_t used_stack_portion = stack_base + DEFAULT_STACK_SIZE -
		stack_pointer;

	md.stack_offset[task->id] = used_stack_portion;
	md.stack_base[task->id] = stack_base;

	ksprintf(stack_chkpt_file, "%s.%d", CHKPT_STACK_FILE, task->id);

	MIGLOG("Checkpoint stack from 0x%llx, size 0x%llx, SP=0x%llx\n", stack_base,
			DEFAULT_STACK_SIZE, stack_pointer);

	return migrate_chkpt_area(stack_base, DEFAULT_STACK_SIZE, stack_chkpt_file);
}

int save_tls(uint64_t tls_size, int task_id) {
	if(tls_size > 0) {
		char tls_chkpt_file[32];
		uint64_t local_tls_start;

#ifdef __aarch64__
		thread_block_t *tcb = (thread_block_t *)get_tpidr();
		local_tls_start = (uint64_t)tcb->dtv;
#else
		local_tls_start = get_tls() - tls_size;
#endif

		ksprintf(tls_chkpt_file, "%s.%d", CHKPT_TLS_FILE, task_id);
		MIGLOG("Saving TLS - start: 0x%llx, size: 0x%llx in %s\n",
				local_tls_start, tls_size, tls_chkpt_file);

		return migrate_chkpt_area(local_tls_start, tls_size, tls_chkpt_file);
	}

	return 0;
}

/* A dedicated thread calls this function. This function reads the heap page by page.
 * If any page is absent, a page fault occures which retrives the page from the source
 * machine. It helps prefetching heap after migration.
 */
#define REMOTE_MEM_THREAD_DELAY_MS	200
#define VALID_ADDRESSES_TO_TRY		16
int periodic_page_access(void *arg)
{
	uint64_t i;
	char j;
	uint64_t start, stop;
	int valid_addreses_found = 0;

	MIGLOG("Remote memory thread starts\n");
	task_t *task = per_core(current_task);

	/* The main task uses clone_task to create this thread, thus it inheritates
	 * info about the heap */
	start = task->heap->start;
	stop = start + task->migrated_heap;

	/* First check the page tables to see if the page is not already there. If
	 * it is, check the next one, up to VALID_ADDRESSES_TO_TRY pages per
	 * iteration */
	for(i = start; i<stop; i += PAGE_SIZE) {
		// MIGLOG("periodic: request 0x%llx\n", i);
		if(check_pagetables(i)) {
			if(++valid_addreses_found == VALID_ADDRESSES_TO_TRY) {
				goto rmem_sleep;
			} else { continue; }}

		j = *((char *)i);

rmem_sleep:
		valid_addreses_found = 0;
		sys_msleep(REMOTE_MEM_THREAD_DELAY_MS);
	}

	MIGLOG("Remote memory thread done\n");
	return 0;
}

/* Returns -1 if migration attemp failed (on the source), 0 if we are back from
 * successful migration on the target, -2 if something went wrong during state
 * restoration on target
 */
int sys_migrate(void *regset) {
	/* This is the stack pointer value not exactly at the beginning of this
	 * function, but right after the preamble inserted by the compiler (which
	 * number of instructions can vary so we cannot simply rely on the function
	 * address to set the resuming IP). Thus we use the label below (migrate:)
	 * which address is completely consistent with the stack pointer we get in
	 * the stack pointer variable. */
	register uint64_t stack_pointer = current_stack_pointer;

migrate_resume_entry_point:

	if(mig_resuming) {
		task_t *task = per_core(current_task);
		static int primary_flag = 1;

		MIGLOG("Thread %d (%s) enters resume code\n", task->id,
				primary_flag ? "primary" : "secondary");
		if(primary_flag) {
			/* FIXME: we probably don't need to call this here, there is a bug
			 * somewhere */
			set_primary_thread_id(task->id);
			primary_flag = 0;
			primary_thread_id = task->id;

			/* get metadata */
			if(migrate_restore_area(CHKPT_MDATA_FILE, (uint64_t)&md,
					sizeof(chkpt_metadata_t))) {
				return -2;
			}

			if(restore_bss(md.bss_size)) {
				MIGERR("Cannot restore BSS after migration\n");
				return -2;
			}

			if(restore_data(md.data_size)) {
				MIGERR("Cannot restore .data after migration\n");
				return -2;
			}

			if(restore_heap(md.heap_start, md.heap_size)) {
				MIGERR("Cannot restore heap after migration\n");
				return -2;
			}

			if(migrate_restore_fds(CHKPT_FDS_FILE)) {
				MIGERR("Cannot restore file descriptors after migration\n");
				return -2;
			}

#if 0
			int i = 1;
			while(md.task_ids[i] != 0) {
				unsigned int id;
				clone_task(&id, (void *)md.ip, (void *) &(md.task_ids[i]),
							per_core(current_task)->prio);
				MIGLOG("Created thread %d for old %d (should be the same)\n",
						id, md.task_ids[i]);
				i++;
			}
#endif
		}

		if(restore_tls(md.tls_size, task->id)) {
			MIGERR("Cannot restore TLS after migration\n");
			return -2;
		}
#if 0
		/* Barrier: all threads sync here before resuming */
		int ret = atomic_int32_dec(&threads_to_resume);
		while(ret != 0) {
			reschedule();
			ret = atomic_int32_read(&threads_to_resume);
		}
#endif

		MIGLOG("Thread %u (%s): state restored, back to execution\n", task->id,
				(task->id == primary_thread_id) ? "primary" : "secondary");
#if 0
		if(task->id == primary_thread_id)
#endif
			mig_resuming = 0;

		/* mig_resuming needs to be set to 0 for the next clone_task to
		 * succeed */
		__asm__ __volatile__ ("" ::: "memory");

		unsigned int created_remote_mem_thread_id;
		clone_task(&created_remote_mem_thread_id, periodic_page_access, NULL,
				LOW_PRIO);
		set_remote_mem_thread_id(created_remote_mem_thread_id);

		/* Restore callee-saved registers or the full set of popcorn regs */
#ifdef __aarch64__
		if(md.popcorn_regs_valid) {
			MIGLOG("Detected popcorn register set\n");
			struct regset_aarch64 *rs =
				(struct regset_aarch64 *)&(md.popcorn_arm_regs);

			SET_REGS_AARCH64(*rs);
			SET_FRAME_AARCH64((*rs).x[29], (*rs).sp);
			SET_PC_REG((*rs).pc);

			MIGERR("Should not reach here!\n");
		} else {
			/* Old homogeneous migration stuff, to be removed */
			// x19 -> x28 + frame pointer (x29) + link register (ret. addr, x30)
			SET_X19(md.x19[task->id]);
			SET_X20(md.x20[task->id]);
			SET_X21(md.x21[task->id]);
			SET_X22(md.x22[task->id]);
			SET_X23(md.x23[task->id]);
			SET_X24(md.x24[task->id]);
			SET_X25(md.x25[task->id]);
			SET_X26(md.x26[task->id]);
			SET_X27(md.x27[task->id]);
			SET_X28(md.x28[task->id]);
			SET_X29(md.x29[task->id]);
			SET_X30(md.x30[task->id]);
		}
#else
		if(md.popcorn_regs_valid) {
			MIGLOG("Detected popcorn register set\n");
			struct regset_x86_64 *rs =
				(struct regset_x86_64 *)&(md.popcorn_x86_regs);

			/* TODO update this to use a simpler macro */
			SET_RAX(rs->rax);
			SET_RDX(rs->rdx);
			SET_RCX(rs->rcx);
			SET_RBX(rs->rbx);
			SET_RSI(rs->rsi);
			SET_RDI(rs->rdi);
			SET_R8(rs->r8);
			SET_R9(rs->r9);
			SET_R10(rs->r10);
			SET_R11(rs->r11);
			SET_R12(rs->r12);
			SET_R13(rs->r13);
			SET_R14(rs->r14);
			SET_R15(rs->r15);

			SET_RSP(rs->rsp);
			SET_RBP(rs->rbp);
			SET_RIP_REG(rs->rip);

			MIGERR("Should not reach here!\n");
		} else {
			/* Old homogeneous migration stuff, to be removed */
			SET_R12(md.r12[task->id]);
			SET_R13(md.r13[task->id]);
			SET_R14(md.r14[task->id]);
			SET_R15(md.r15[task->id]);
			SET_RBX(md.rbx[task->id]);
			SET_RBP(md.rbp[task->id]);
		}
#endif


		return 0;
	}

	task_t* task = per_core(current_task);
	int is_main_task = (task->id == primary_thread_id) ? 1 : 0;
	MIGLOG("Thread %u (%s) entering checkpoint process\n", task->id,
			is_main_task ? "primary" : "secondary" );

	/* Stack and TLS are saved on a per-thread basis */
	if(save_stack(stack_pointer))
		return -1;

	/* Save TLS */
	if(task->tls_size)
		if(save_tls(task->tls_size, task->id))
			return -1;

#if 0
	/* After that point only the main thread continues */
	if(!is_main_task) {
		atomic_int32_dec(&sec_threads_ready);
		MIGLOG("Thread %d (secondary) done with checkpointing, waiting for "
				"primary\n", task->id);
		while(1) sys_msleep(1000);
	}
#endif

	/* The main thread is responsible for saving heap, bss, data, file
	 * descriptors, and the instruction pointer (same for all threads) */
	uint64_t bss_size, data_size;
#if 0
	uint32_t sec_threads_barrier;
#endif

	md.heap_size = task->heap->end - task->heap->start;
	md.heap_start = task->heap->start;
	if(checkpoint_heap())
		return -1;

	/* Save .bss */
	bss_size = (size_t) &kernel_start + image_size - (size_t) &__bss_start;
	MIGLOG("Checkpoint bss from 0x%llx, size 0x%llx\n", &__bss_start, bss_size);
	if(migrate_chkpt_area(((uint64_t)&__bss_start), bss_size, CHKPT_BSS_FILE))
		return -1;
	md.bss_size = bss_size;

	/* Save .data */
	data_size = (size_t) &__data_end - (size_t) &__data_start;
	MIGLOG("Checkpoint data from 0x%llx, size 0x%llx\n", &__data_start,
			data_size);
	if(migrate_chkpt_area(((uint64_t)&__data_start), data_size,
				CHKPT_DATA_FILE))
		return -1;
	md.data_size = data_size;

	/* Save tls size (same for all the threads) */
	md.tls_size = task->tls_size;

	/* Save instruction pointer FIXME on ARM+x86 that will be different */
	md.ip = (uint64_t)&&migrate_resume_entry_point;

	/* Save task ids, note that this function always puts the primary task id
	 * in the first slot */
	if(get_tasks_ids(md.task_ids, MAX_TASKS)) {
		MIGERR("Error getting list of tasks ids\n");
		return -1;
	}

	/* We need to checkpoint the callee-saved registers as they might contain
	 * data that is not synced with memory, and there is no guarantee that
	 * these are pushed on the stack by the current function */
#ifdef __aarch64__
	GET_X19(md.x19[task->id]);
	GET_X20(md.x20[task->id]);
	GET_X21(md.x21[task->id]);
	GET_X22(md.x22[task->id]);
	GET_X23(md.x23[task->id]);
	GET_X24(md.x24[task->id]);
	GET_X25(md.x25[task->id]);
	GET_X26(md.x26[task->id]);
	GET_X27(md.x27[task->id]);
	GET_X28(md.x28[task->id]);
	GET_X29(md.x29[task->id]);
	GET_X30(md.x30[task->id]);
#else
	GET_R12(md.r12[task->id]);
	GET_R13(md.r13[task->id]);
	GET_R14(md.r14[task->id]);
	GET_R15(md.r15[task->id]);
	GET_RBX(md.rbx[task->id]);
	GET_RBP(md.rbp[task->id]);
#endif

	/* Checkpoint popcorn registers */
	if(regset) {
		MIGLOG("Writing popcorn register set in metadata\n");
#ifdef __aarch64__
		memcpy(&(md.popcorn_arm_regs), regset, sizeof(struct regset_aarch64));
		memcpy(&(md.popcorn_x86_regs), regset, sizeof(struct regset_x86_64));
#else
		memcpy(&(md.popcorn_x86_regs), regset, sizeof(struct regset_x86_64));
		memcpy(&(md.popcorn_arm_regs), regset, sizeof(struct regset_aarch64));
#endif
		md.popcorn_regs_valid = 1;
	}
	else {
		MIGLOG("WARNING: no popcorn register set passed\n");
		md.popcorn_regs_valid = 0;
	}

#if 0
	/* Wait for secondary threads to finish their part of the migration */
	sec_threads_barrier = atomic_int32_read(&sec_threads_ready);
	while(sec_threads_barrier != 1) {
		reschedule();
		sec_threads_barrier = atomic_int32_read(&sec_threads_ready);
	}
#endif

	/* Save fds here, as we know all the secondary threads are done,
	 * because we don't want to checkpoint the fds corresponding to
	 * stack and tls data! */
	if(migrate_chkpt_fds(CHKPT_FDS_FILE))
		return -1;

	/* Finally write metadata file */
	if(migrate_chkpt_area((uint64_t)&md, sizeof(chkpt_metadata_t),
			CHKPT_MDATA_FILE))
		return -1;

	/* Finally the main task sends the migration signal to uhyve */
	MIGLOG("Thread %d (primary) done with migration\n", task->id);
	uhyve_migration_t arg = {md.heap_size, md.bss_size};
	uhyve_send(UHYVE_PORT_MIGRATE, (unsigned)virt_to_phys((size_t)&arg));

	/* Should never reach here */
	MIGERR("Migration - source: should not reach that point\n");
	return -1;
}

void migrate_init(void) {

	/* Set migration flag to 0 */
	atomic_int32_set(&should_migrate, 0);

	/* Keep track of the number of running threads. Initialized to 1 as we are
	 * sure to have at least a main task, and incremented each time a thread is
	 * created (by clone_task). We do not count the idle task here */
	atomic_int32_set(&running_threads, 1);

	atomic_int32_set(&sec_threads_ready, 0);

	/* Init fd migration system */
	migration_fd_init();
	MIGLOG("Initializing migration subsystem\n");
}

/* Returns:
 * 0 if successfully back from migration
 * 1 if migration did not take place because the flag is not set
 * -1 if issue saving state (on source machine)
 * -2 if issue restoring state (on target machine)
 */
int migrate_if_needed(void *regset) {

	if(atomic_int32_read(&should_migrate) == 1) {
		/* Wait for every thread to be in a migratable state TODO use sem? */
		int ret = atomic_int32_dec(&running_threads);
		atomic_int32_inc(&sec_threads_ready);
		while(ret != 0) {
			reschedule();
			ret = atomic_int32_read(&running_threads);
		}

		/* reset migration flag */
		atomic_int32_set(&should_migrate, 0);
		return sys_migrate(regset);
	}

	return 1;
}

void force_migration_flag(int val) {
	atomic_int32_set(&should_migrate, val);
}
