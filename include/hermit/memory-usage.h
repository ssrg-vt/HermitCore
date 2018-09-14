#ifndef MEMORY_USAGE_H
#define MEMORY_USAGE_H

#include <hermit/stddef.h>

/* Set to 1 to enable */
#define MEMORY_USAGE_TRACKING 0

#if MEMORY_USAGE_TRACKING == 1
void _memory_usage_set(uint64_t val);
void _memory_usage_add(uint64_t val);
void _memory_usage_sub(uint64_t val);

#define memory_usage_set(x) _memory_usage_set(x)
#define memory_usage_add(x) _memory_usage_add(x)
#define memory_usage_sub(x) _memory_usage_sub(x)
#else
#define memory_usage_set(x) (void)0
#define memory_usage_add(x) (void)0
#define memory_usage_sub(x) (void)0
#endif

#endif /* MEMORY_USAGE_H */
