#ifndef MIGRATION_H
#define MIGRATION_H

/* Functions needed in both kernel and userspace */
int migrate_if_needed(void);
void force_migration_flag(int val);

#ifdef __KERNEL__

#include <hermit/logging.h>

/* Kernel migration interface */
void migrate_init(void);
int sys_migrate(void);
void incr_running_threads(void);
void dec_running_threads(void);
void set_primary_thread_id(int id);
void init_threads_to_resume(int num_threads);

//#define DIE() {asm("int $3");}
#define DIE() {volatile uint64_t xyz = 0; *(int*)xyz = 0;}

/* To activate logging, invokes the kernel cmake with -DMIGRATION_LOG */
#ifdef HAVE_MIG_LOG
#define MIGLOG(...) LOG_INFO("[MIGRATION] " __VA_ARGS__)
#define MIGERR(...) { LOG_ERROR("[MIGRATION] " __VA_ARGS__); DIE(); }
#else
#define MIGLOG(...)
#define MIGERR(...)
#endif /* HAVE_MIG_LOG */

#else  /* __KERNEL__ */

/* Userspace migration interface */
#include <stdio.h>
#include <stdlib.h>

__attribute__ ((no_instrument_function)) static inline int hermit_migpoint(void) {
	int r = migrate_if_needed();
	if(r < 0) {
		if(r == -1)
			fprintf(stderr, "Error saving state during migration\n");
		if(r == -2)
			fprintf(stderr, "Error restoring state after migration\n");
		exit(-1);
	}
	return r;
}

__attribute__ ((no_instrument_function)) static inline int hermit_force_migration(void) {
	force_migration_flag(1);
	return hermit_migpoint();
}

/* Use this macro for easy migration point insertion in the code */
#define HERMIT_MIGPOINT() 			hermit_migpoint()
/* Use this macro to force migration */
#define HERMIT_FORCE_MIGRATION()	hermit_force_migration()

__attribute__((no_instrument_function)) static void __cyg_profile_func_enter (void *this_fn,
                               void *call_site) {
	HERMIT_MIGPOINT();
}

__attribute__((no_instrument_function)) static void __cyg_profile_func_exit  (void *this_fn,
                               void *call_site) {
	HERMIT_MIGPOINT();
}
#endif /* __KERNEL__ */

#endif /* MIGRATION_H */
