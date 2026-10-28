#include "heap.h"

#include <stdint.h>
#include <stdalign.h>

#define KERNEL_HEAP_SIZE (64 * 1024)

struct heap_block {
    size_t size;
    struct heap_block *next;
    bool is_free;
};

static alignas(max_align_t) unsigned char heap_storage[KERNEL_HEAP_SIZE];
static struct heap_block *heap_first_block;
static bool heap_initialized;

static size_t heap_alignment(void)
{
    return alignof(max_align_t);
}

static size_t heap_header_size(void)
{
    const size_t alignment = heap_alignment();
    return (sizeof(struct heap_block) + alignment - 1) & ~(alignment - 1);
}

static bool align_size(size_t size, size_t *aligned_size)
{
    const size_t alignment = heap_alignment();
    if (size > SIZE_MAX - (alignment - 1)) {
        return false;
    }
    *aligned_size = (size + alignment - 1) & ~(alignment - 1);
    return true;
}

void kernel_heap_initialize(void)
{
    heap_first_block = (struct heap_block *)heap_storage;
    heap_first_block->size = KERNEL_HEAP_SIZE - heap_header_size();
    heap_first_block->next = NULL;
    heap_first_block->is_free = true;
    heap_initialized = true;
}

void *kernel_heap_allocate(size_t size)
{
    if (!heap_initialized || size == 0) {
        return NULL;
    }

    size_t aligned_size;
    if (!align_size(size, &aligned_size)) {
        return NULL;
    }

    const size_t header_size = heap_header_size();
    for (struct heap_block *block = heap_first_block; block != NULL; block = block->next) {
        if (!block->is_free || block->size < aligned_size) {
            continue;
        }

        if (block->size - aligned_size >= header_size + heap_alignment()) {
            struct heap_block *remainder = (struct heap_block *)
                ((unsigned char *)block + header_size + aligned_size);
            remainder->size = block->size - aligned_size - header_size;
            remainder->next = block->next;
            remainder->is_free = true;
            block->next = remainder;
            block->size = aligned_size;
        }
        block->is_free = false;
        return (unsigned char *)block + header_size;
    }
    return NULL;
}

bool kernel_heap_release(void *pointer)
{
    if (!heap_initialized || pointer == NULL) {
        return false;
    }

    struct heap_block *target = NULL;
    for (struct heap_block *block = heap_first_block; block != NULL; block = block->next) {
        if ((unsigned char *)block + heap_header_size() == (unsigned char *)pointer) {
            target = block;
            break;
        }
    }
    if (target == NULL || target->is_free) {
        return false;
    }

    target->is_free = true;
    for (struct heap_block *block = heap_first_block; block != NULL && block->next != NULL;) {
        if (block->is_free && block->next->is_free) {
            block->size += heap_header_size() + block->next->size;
            block->next = block->next->next;
        } else {
            block = block->next;
        }
    }
    return true;
}

size_t kernel_heap_available(void)
{
    if (!heap_initialized) {
        return 0;
    }

    size_t available = 0;
    for (struct heap_block *block = heap_first_block; block != NULL; block = block->next) {
        if (block->is_free) {
            available += block->size;
        }
    }
    return available;
}