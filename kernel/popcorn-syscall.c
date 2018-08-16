#include <hermit/syscall.h>
#include <hermit/tasks.h>
#include <hermit/errno.h>

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
	return -ENOSYS;
}

/* Return thread status */
int sys_get_thread_status(struct popcorn_thread_status *status) {
	return -ENOSYS;
}

