#include "hermit/memory-usage.h"
#include <hermit/spinlock.h>
#include <asm/uhyve.h>

typedef struct {
	uint64_t mem;
} __attribute__ ((packed)) uhyve_mem_usage_t;

static spinlock_t memory_usage_lock;
static uint64_t memory_usage;

/* Needs to be caled with memory_usage_lock hold */
static inline void update_uhyve(void) {
	uhyve_mem_usage_t arg = {memory_usage};
	uhyve_send(UHYVE_PORT_MEM_USAGE, (unsigned)virt_to_phys((size_t)&arg));
}

void _memory_usage_set(uint64_t val) {
	spinlock_lock(&memory_usage_lock);
	memory_usage = val;
	update_uhyve();
	spinlock_unlock(&memory_usage_lock);
}

void _memory_usage_add(uint64_t val) {
	spinlock_lock(&memory_usage_lock);
	memory_usage += val;
	update_uhyve();
	spinlock_unlock(&memory_usage_lock);
}

void _memory_usage_sub(uint64_t val) {
	spinlock_lock(&memory_usage_lock);
	memory_usage -= val;
	update_uhyve();
	spinlock_unlock(&memory_usage_lock);
}

