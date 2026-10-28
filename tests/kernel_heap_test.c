#include "../memory/heap.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <string.h>

int main(void)
{
    kernel_heap_initialize();
    const size_t initial_available = kernel_heap_available();
    assert(initial_available > 60000);
    assert(kernel_heap_allocate(0) == NULL);

    void *first = kernel_heap_allocate(37);
    void *second = kernel_heap_allocate(128);
    assert(first != NULL && second != NULL);
    assert((uintptr_t)first % alignof(max_align_t) == 0);
    assert((uintptr_t)second % alignof(max_align_t) == 0);
    memset(first, 0xA5, 37);
    memset(second, 0x5A, 128);
    for (size_t index = 0; index < 37; index++) {
        assert(((unsigned char *)first)[index] == 0xA5);
    }
    assert(kernel_heap_available() < initial_available);

    assert(!kernel_heap_release((unsigned char *)first + 1));
    assert(kernel_heap_release(first));
    assert(!kernel_heap_release(first));
    assert(kernel_heap_release(second));
    assert(kernel_heap_available() == initial_available);

    void *blocks[32];
    for (size_t index = 0; index < 32; index++) {
        blocks[index] = kernel_heap_allocate(64 + index);
        assert(blocks[index] != NULL);
        memset(blocks[index], (int)index, 64 + index);
    }
    for (size_t index = 0; index < 32; index += 2) {
        assert(kernel_heap_release(blocks[index]));
    }
    for (size_t index = 1; index < 32; index += 2) {
        assert(kernel_heap_release(blocks[index]));
    }
    assert(kernel_heap_available() == initial_available);

    void *full = kernel_heap_allocate(initial_available);
    assert(full != NULL);
    assert(kernel_heap_allocate(1) == NULL);
    assert(kernel_heap_release(full));
    assert(kernel_heap_available() == initial_available);

    puts("kernel heap tests passed");
    return 0;
}