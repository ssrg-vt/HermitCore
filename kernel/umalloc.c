#include <hermit/umalloc.h>
#include <hermit/stddef.h>

/* Umalloc allows the kernel to allocate memory that will be migrated (as
 * opposed to kmalloc'd memory which is not).
 * For now we rely on malloc from the C library, which is a terrible hack
 */

extern void *malloc(size_t bytes);
extern void free(void *ptr);

void *umalloc(size_t bytes) {
	return malloc(bytes);
}

void ufree(void *ptr) {
	return free(ptr);
}
