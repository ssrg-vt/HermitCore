#include <hermit/stack_slots.h>
#include <hermit/config.h>
#include <hermit/logging.h>
#include <hermit/vma.h>
#include <hermit/string.h>
#include <hermit/stddef.h>
#include <hermit/migration.h>
#include <asm/page.h>
#include <hermit/memory.h>

/* Stack slots: when we resume a thread from migration, its stack needs to be
 * located (in the virtual address space) in the same location as where it was
 * previously allocated on the machine source of the migration. Thus, we 
 * implement a set of stack slots, and each task id is mapped to a specific
 * slot. Task ids are moved during migrations and used to get the same
 * slot after migration.
 * Stack slots are indexed by task id so there is no need to protect this code
 * against concurrent access
 */

static void *stack_slots = NULL;
static uint64_t stack_slots_size = 0;

extern const void kernel_start;

/* allocate max_task_id slots */
int stack_slots_init(void) {
	stack_slots_size =  STACK_SLOTS_NUM * DEFAULT_STACK_SIZE;
	stack_slots = (void *)STACK_SLOTS_START;

	MIGLOG("Stack slots: trying to reserve area 0x%x -> 0x%x (%u slots, %u "
			"pages, 0x%x bytes)\n", stack_slots, stack_slots + stack_slots_size,
			STACK_SLOTS_NUM, stack_slots_size/PAGE_SIZE, stack_slots_size);

	return 0;
}

/* Return 1 if addr falls into the stack slot allocated area */
int addr_in_stack_slot(uint64_t addr) {
	return (addr >= (uint64_t)stack_slots) && (addr < (uint64_t)stack_slots +
			stack_slots_size);
}

/* On demand stack slot mapping: return the base address of a slot on success,
 * 0 on error */
uint64_t get_stack_slot(int task_id) {
	size_t slot;

	if(task_id >= STACK_SLOTS_NUM) {
		MIGERR("Stack slot limit reached\n");
		return 0;
	}

	slot = ((uint64_t)stack_slots + DEFAULT_STACK_SIZE*task_id);
	uint64_t pages_needed = DEFAULT_STACK_SIZE >> PAGE_BITS;

	if (BUILTIN_EXPECT(!slot, 0))
		return -1;

	size_t phy = get_pages(pages_needed);
	if(!phy) {
		MIGERR("Cannot get pages for stack slots allocation\n");
		return -1;
	}

	if(page_map(slot + PAGE_SIZE, phy, pages_needed, PG_RW|PG_GLOBAL|PG_NX)) {
		MIGERR("Cannot map pages for stack slots\n");
		return -1;
	}

	MIGLOG("stack slots created stack from 0x%x to 0x%x\n", slot, slot +
			pages_needed * PAGE_SIZE);

	return slot + PAGE_SIZE;
}
