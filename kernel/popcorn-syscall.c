#include <hermit/syscall.h>
#include <hermit/tasks.h>
#include <hermit/errno.h>
#include <hermit/migration.h>
#include <asm/atomic.h>

#define MAX_POPCORN_NODES	32 /* Needs to be consistent with libmigrate */

extern atomic_int32_t should_migrate;

/* Need to be consistent with popcorn-compiler/common */
enum arch {
  ARCH_UNKNOWN = -1,
  ARCH_AARCH64,
  ARCH_POWERPC64,
  ARCH_X86_64,
  NUM_ARCHES
};

/* Thread migration status information. Warning: this needs to be consistent
 * with libmigrate (see migrate.c) */
struct popcorn_thread_status {
	int current_nid;
	int proposed_nid;
	int peer_nid;
	int peer_pid;
} status;

/* Nodes info. Warning: needs to be consistent with libmigrate definition
 * (migrate.c) */
struct node_info {
	unsigned int status;
	int arch;
	int distance;
};

/* Returns the address at the beginning of the area reserved for the stack of
 * the calling thread */
void* sys_stackaddr(void) {
	task_t* task = per_core(current_task);
	return task->stack;
}

/* Returns the calling thread stack size */
size_t sys_stacksize(void) {
	return DEFAULT_STACK_SIZE;
}

/* Returns information about the popcorn nodes */
int sys_get_node_info(int *origin_nid, struct node_info* ni) {
	*origin_nid = 0;

	for(int i=0; i<MAX_POPCORN_NODES; i++)
		ni[i].status = 0;

	/* For now, we hardcode node 0 is the x86 server and node 1 is the arm
	 * board */
	ni[0].status = ni[1].status = 1;
	ni[0].arch = ARCH_X86_64;
	ni[1].arch = ARCH_AARCH64;

#ifdef __aarch64__
	ni[0].distance = 1;
	ni[1].distance = 0;
#else
	ni[0].distance = 0;
	ni[1].distance = 1;
#endif

	return 0;
}

/* Return thread status */
int sys_get_thread_status(struct popcorn_thread_status *status) {
	int mig_flag_set = atomic_int32_read(&should_migrate);

	LOG_INFO("flag: %d\n", mig_flag_set);

	/* For now, we hardcode node 0 is the x86 server and node 1 is the arm
	 * board */

#ifdef __aarch64__
	status->current_nid = 1;
	if(mig_flag_set)
		status->proposed_nid = 0;
#else
	status->current_nid = 0;
	if(mig_flag_set)
		status->proposed_nid = 1;
#endif

	status->peer_nid = status->peer_pid = 0; /* FIXME what is this ? */

	return 0;
}

