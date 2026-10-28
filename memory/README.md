# Memory Management

`heap.c` provides the kernel's fixed 64 KiB aligned bootstrap heap, with
first-fit allocation, validated release, and adjacent-block coalescing. The
kernel initializes it during startup. Run its host tests with `make memory-test`.

This heap is not backed by a firmware memory map and does not allocate physical
pages. The initial boot code creates temporary identity-mapped page tables in
`boot/` so the kernel can enter x86_64 long mode. Physical/virtual memory
management and page allocation remain future work.