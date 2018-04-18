/*
 * Copyright (c) 2010, Stefan Lankes, RWTH Aachen University
 * All rights reserved.
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
 */

/**
 * @author Stefan Lankes
 * @file include/hermit/tasks.h
 * @brief Task related
 *
 * Create and leave tasks or fork them.
 */

#ifndef __TASKS_H__
#define __TASKS_H__

#include <hermit/stddef.h>
#include <hermit/tasks_types.h>
#include <asm/tasks.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief System call to terminate a user level process */
void NORETURN sys_exit(int);


/** @brief Task switcher
 *
 * Timer-interrupted use of this function for task switching
 *
 * @return
 * - 0 no context switch
 * - !0 address of the old stack pointer
 */
size_t** scheduler(void);


/** @brief Initialize the multitasking subsystem
 *
 * This procedure sets the current task to the
 * current "task" (there are no tasks, yet) and that was it.
 *
 * @return
 * - 0 on success
 * - -ENOMEM (-12) on failure
 */
int multitasking_init(void);


/** @brief Clone current task with a specific entry point
 *
 * @todo Don't acquire table_lock for the whole task creation.
 *
 * @param id Pointer to a tid_t struct were the id shall be set
 * @param ep Pointer to the function the task shall start with
 * @param arg Arguments list
 * @param prio Desired priority of the new task
 * @param core_id Start the new task on the core with this id
 *
 * @return
 * - 0 on success
 * - -ENOMEM (-12) or -EINVAL (-22) on failure
 */
int clone_task(tid_t* id, entry_point_t ep, void* arg, uint8_t prio);


/** @brief Create a task with a specific entry point
 *
 * @todo Don't acquire table_lock for the whole task creation.
 *
 * @param id Pointer to a tid_t struct were the id shall be set
 * @param ep Pointer to the function the task shall start with
 * @param arg Arguments list
 * @param prio Desired priority of the new task
 * @param core_id Start the new task on the core with this id
 *
 * @return
 * - 0 on success
 * - -ENOMEM (-12) or -EINVAL (-22) on failure
 */
int create_task(tid_t* id, entry_point_t ep, void* arg, uint8_t prio, uint32_t core_id);


/** @brief create a kernel-level task on the current core.
 *
 * @param id The value behind this pointer will be set to the new task's id
 * @param ep Pointer to the entry function for the new task
 * @param args Arguments the task shall start with
 * @param prio Desired priority of the new kernel task
 *
 * @return
 * - 0 on success
 * - -EINVAL (-22) on failure
 */
int create_kernel_task(tid_t* id, entry_point_t ep, void* args, uint8_t prio);


/** @brief create a kernel-level task.
 *
 * @param id The value behind this pointer will be set to the new task's id
 * @param ep Pointer to the entry function for the new task
 * @param args Arguments the task shall start with
 * @param prio Desired priority of the new kernel task
 * @param core_id Start the new task on the core with this id
 *
 * @return
 * - 0 on success
 * - -EINVAL (-22) on failure
 */
int create_kernel_task_on_core(tid_t* id, entry_point_t ep, void* args, uint8_t prio, uint32_t core_id);

/** @brief Cleanup function for the task termination
 *
 * On termination, the task call this function to cleanup its address space.
 */
void finish_task_switch(void);


/** @brief determine the highest priority of all tasks, which are ready
 *
 * @return
 * - return highest priority
 * - if no task is ready, the function returns an invalid value (> MAX_PRIO)
 */
uint32_t get_highest_priority(void);


/** @brief Call to rescheduling
 *
 * This is a purely assembled procedure for rescheduling
 */
void reschedule(void);


/** @brief Wake up a blocked task
 *
 * The task's status will be changed to TASK_READY
 *
 * @return
 * - 0 on success
 * - -EINVAL (-22) on failure
 */
int wakeup_task(tid_t);

/** @brief Wake up a core_id
 *
 * Wakeup core to be sure that
 * the core isn't in halt state
 *
 * @param core_id Specifies the core
 */
void wakeup_core(uint32_t core_id);

/** @brief Block current task
 *
 * The current task's status will be changed to TASK_BLOCKED
 *
 * @return
 * - 0 on success
 * - -EINVAL (-22) on failure
 */
int block_current_task(void);

/** @brief Block task until a new arrived
 *
 */
void wait_for_task(void);

/** @brief Get readyqueue of the current core
 *
 * @return
 *  - address of the readyqueue
 */
void* get_readyqueue(void);

/** @brief Get a process control block
 *
 * @param id	ID of the task to retrieve
 * @param task	Location to store pointer to task
 * @return
 *  - 0 on success
 *  - -ENOMEM (-12) if @p task is NULL
 *  - -ENOENT ( -2) if @p id not in task table
 *  - -EINVAL (-22) if there's no valid task with @p id
 */
int get_task(tid_t id, task_t** task);


/** @brief Block current task until timer expires
 *
 * @param deadline Clock tick, when the timer expires
 * @return
 *  - 0 on success
 *  - -EINVAL (-22) on failure
 */
int set_timer(uint64_t deadline);


/** @brief check is a timer is expired
 *
 */
void check_timers(void);


/** @brief Abort current task */
void NORETURN do_abort(void);


/** @brief This function shall be called by leaving kernel-level tasks */
void NORETURN leave_kernel_task(void);


/** @brief if a task exists with higher priority, HermitCore switch to it.
 */
void check_scheduling(void);

/** @brief This function shutdowns the (ip) network
 */
int network_shutdown(void);


#ifdef DYNAMIC_TICKS
/** @brief check, if the tick counter has to be updated
 */
void check_ticks(void);
#endif


/** @brief shutdown the whole system
 */
void shutdown_system(void);

extern volatile uint32_t go_down;
static inline void check_workqueues_in_irqhandler(int irq)
{
#ifdef DYNAMIC_TICKS
	// Increment ticks
	check_ticks();
#endif

	check_timers();

	if (go_down)
		shutdown_system();
	if (irq < 0)
		check_scheduling();
}

static inline void check_workqueues(void)
{
	// call with invalid interrupt number
	check_workqueues_in_irqhandler(-1);
}

/** @brief check if a proxy is available
 */
int is_proxy(void);

/** @brief initialized the stacks of the idle tasks
 */
int set_boot_stack(tid_t id, size_t stack, size_t ist_addr);

#ifdef __cplusplus
}
#endif

#endif
