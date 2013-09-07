#include "operations.h"

#include <stdlib.h>
#include <string.h>

void *malloc_wrapper(void *pool, int size) { return malloc(size); }
void *realloc_wrapper(void *pool, void *p, int size) { return realloc(p, size); }
void *strdup_wrapper(void *pool, void *p) { return strdup(p); }
void free_wrapper(void *pool, void *p) { free(p); }

alloc_operations_t malloc_ops = {
    .alloc = &malloc_wrapper,
    .realloc = &realloc_wrapper,
    .strdup = &strdup_wrapper,
    .free = &free_wrapper,
};


