.set ALIGN, 1 << 0
.set MEMINFO, 1 << 1
.set VIDEO, 1 << 2
.set FLAGS, ALIGN | MEMINFO | VIDEO
.set MAGIC, 0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0
.long 1024
.long 768
.long 32

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.align 4
multiboot_info_address:
.long 0

.align 4096
pml4_table:
.skip 4096
pdpt_table:
.skip 4096
page_directory_0:
.skip 4096
page_directory_1:
.skip 4096
page_directory_2:
.skip 4096
page_directory_3:
.skip 4096

.section .text
.code32
.global _start
.type _start, @function
_start:
    cli
    cmp $0x2BADB002, %eax
    jne boot_failure
    mov %ebx, multiboot_info_address

    pushfl
    pop %eax
    mov %eax, %ecx
    xor $0x200000, %eax
    push %eax
    popfl
    pushfl
    pop %eax
    push %ecx
    popfl
    xor %ecx, %eax
    test $0x200000, %eax
    jz boot_failure

    mov $1, %eax
    cpuid
    test $0x40, %edx
    jz boot_failure

    mov $0x80000000, %eax
    cpuid
    cmp $0x80000001, %eax
    jb boot_failure
    mov $0x80000001, %eax
    cpuid
    test $0x20000000, %edx
    jz boot_failure

    mov $(pdpt_table + 3), %eax
    mov %eax, pml4_table
    mov $(page_directory_0 + 3), %eax
    mov %eax, pdpt_table
    mov $(page_directory_1 + 3), %eax
    mov %eax, pdpt_table + 8
    mov $(page_directory_2 + 3), %eax
    mov %eax, pdpt_table + 16
    mov $(page_directory_3 + 3), %eax
    mov %eax, pdpt_table + 24

    mov $page_directory_0, %edi
    mov $0x83, %eax
    mov $4, %ebp
1:
    mov $512, %ecx
2:
    mov %eax, (%edi)
    movl $0, 4(%edi)
    add $0x200000, %eax
    add $8, %edi
    loop 2b
    dec %ebp
    jnz 1b

    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4
    mov $pml4_table, %eax
    mov %eax, %cr3

    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr

    lgdt gdt64_pointer
    mov %cr0, %eax
    or $0x80000000, %eax
    mov %eax, %cr0
    ljmp $0x08, $long_mode_start

boot_failure:
    cli
2:
    hlt
    jmp 2b

.code64
long_mode_start:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    xor %ax, %ax
    mov %ax, %fs
    mov %ax, %gs
    lea stack_top(%rip), %rsp
    andq $-16, %rsp
    xor %rbp, %rbp
    mov multiboot_info_address(%rip), %edi
    call kernel_main
    lea kernel_returned_message(%rip), %rdi
    call kernel_panic

3:
    cli
    hlt
    jmp 3b

.size _start, . - _start

.section .rodata
.align 8
gdt64:
    .quad 0
    .quad 0x00AF9A000000FFFF
    .quad 0x00CF92000000FFFF
gdt64_end:
gdt64_pointer:
    .word gdt64_end - gdt64 - 1
    .long gdt64

kernel_returned_message:
    .asciz "kernel_main returned unexpectedly"

.section .note.GNU-stack,"",@progbits
