#ifndef STACK_SLOTS
#define STACK_SLOTS

#include <hermit/stddef.h>

int stack_slots_init(void);
uint64_t get_stack_slot(int id);
int addr_in_stack_slot(uint64_t addr);

#endif /* STACK_SLOTS */
