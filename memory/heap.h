#ifndef KELPOS_MEMORY_HEAP_H
#define KELPOS_MEMORY_HEAP_H

#include <stdbool.h>
#include <stddef.h>

void kernel_heap_initialize(void);
void *kernel_heap_allocate(size_t size);
bool kernel_heap_release(void *pointer);
size_t kernel_heap_available(void);

#endif