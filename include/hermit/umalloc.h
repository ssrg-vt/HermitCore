#ifndef UMALLOC_H
#define UMALLOC_H

#include <hermit/stddef.h>

void *umalloc(size_t bytes);
void ufree(void *ptr);

#endif /* UMALLOC_H */
