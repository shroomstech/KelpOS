HOSTCC ?= cc
CROSS_COMPILE ?= $(if $(wildcard $(CURDIR)/toolchain/local/bin/x86_64-elf-gcc),$(CURDIR)/toolchain/local/bin/x86_64-elf-,)
CC := $(if $(CROSS_COMPILE),$(CROSS_COMPILE)gcc,gcc)
AS := $(if $(CROSS_COMPILE),$(CROSS_COMPILE)as,as)
LD := $(if $(CROSS_COMPILE),$(CROSS_COMPILE)ld,ld)
GRUB_MKRESCUE ?= grub-mkrescue
PYTHON ?= python3
QEMU_DISPLAY ?= gtk
QEMU_MEMORY ?= 256M
QEMU_LOG ?= $(BUILD_DIR)/qemu-debug.log
QEMU_EXTRA_ARGS ?=

BUILD_DIR := build
ISO_ROOT := $(BUILD_DIR)/grub-root
OBJECTS := $(BUILD_DIR)/boot.o \
	$(BUILD_DIR)/kernel.o \
	$(BUILD_DIR)/shell.o \
	$(BUILD_DIR)/memory_heap.o \
	$(BUILD_DIR)/graphics_framebuffer.o \
	$(BUILD_DIR)/graphics_font.o \
	$(BUILD_DIR)/graphics_desktop.o \
	$(BUILD_DIR)/panic.o \
	$(BUILD_DIR)/vga_terminal.o \
	$(BUILD_DIR)/serial.o \
	$(BUILD_DIR)/keyboard.o \
	$(BUILD_DIR)/x86_io.o \
	$(BUILD_DIR)/string.o
KERNEL := $(BUILD_DIR)/kelpos.elf
ISO := $(BUILD_DIR)/kelpos.iso
CISWN := $(BUILD_DIR)/ciswn

CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding \
	-fno-stack-protector -fno-pic -fno-pie -m64 -mcmodel=small -mno-red-zone
LDFLAGS := -m elf_x86_64 -nostdlib -z max-page-size=0x1000 -T kernel/linker.ld

.PHONY: all create iso verify toolchain kpm-test ciswn-test kernel-test memory-test graphics-test boot-test drivers-test run clean check-tools

all: create

create: verify

iso: $(ISO)

toolchain:
	bash toolchain/bootstrap.sh

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/boot.o: boot/boot.s | $(BUILD_DIR)
	$(AS) --64 $< -o $@

$(BUILD_DIR)/kernel.o: kernel/kernel.c boot/multiboot.h kernel/shell.h memory/heap.h drivers/vga_terminal.h drivers/serial.h graphics/framebuffer.h graphics/desktop.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell.o: kernel/shell.c kernel/shell.h drivers/keyboard.h drivers/vga_terminal.h memory/heap.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/memory_heap.o: memory/heap.c memory/heap.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/graphics_framebuffer.o: graphics/framebuffer.c graphics/framebuffer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/graphics_font.o: graphics/font.c graphics/font.h graphics/framebuffer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/graphics_desktop.o: graphics/desktop.c graphics/desktop.h graphics/font.h drivers/keyboard.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/panic.o: kernel/panic.c kernel/panic.h drivers/vga_terminal.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vga_terminal.o: drivers/vga_terminal.c drivers/vga_terminal.h base/string.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/serial.o: drivers/serial.c drivers/serial.h drivers/x86_io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: drivers/keyboard.c drivers/keyboard.h drivers/x86_io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/x86_io.o: drivers/x86_io.c drivers/x86_io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/string.o: base/string.c base/string.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJECTS) kernel/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

$(CISWN): ciswn/ciswn.c | $(BUILD_DIR)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -pedantic $< -lm -o $@

$(ISO): $(KERNEL) grub/grub.cfg
	mkdir -p $(ISO_ROOT)/boot/grub
	cp $(KERNEL) $(ISO_ROOT)/boot/kelpos.elf
	cp grub/grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ $(ISO_ROOT)

check-tools:
	@command -v $(CC) >/dev/null
	@command -v $(AS) >/dev/null
	@command -v $(LD) >/dev/null
	@command -v $(HOSTCC) >/dev/null
	@command -v $(GRUB_MKRESCUE) >/dev/null
	@command -v grub-file >/dev/null
	@command -v $(PYTHON) >/dev/null

verify: $(ISO) $(CISWN) check-tools kpm-test ciswn-test memory-test graphics-test boot-test drivers-test
	grub-file --is-x86-multiboot $(KERNEL)
	test -s $(ISO)
	@printf 'Build complete: %s\n' $(ISO)

kpm-test:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) -m unittest discover -s kpm/tests -v

ciswn-test: $(CISWN)
	CISWN_BIN=$(abspath $(CISWN)) PYTHONDONTWRITEBYTECODE=1 $(PYTHON) -m unittest discover -s ciswn/tests -v

memory-test: memory/heap.c memory/heap.h tests/kernel_heap_test.c | $(BUILD_DIR)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -pedantic memory/heap.c tests/kernel_heap_test.c -o $(BUILD_DIR)/kernel-heap-test
	$(BUILD_DIR)/kernel-heap-test

kernel-test: memory-test

graphics-test: graphics/framebuffer.c graphics/framebuffer.h graphics/font.c graphics/font.h tests/graphics_framebuffer_test.c | $(BUILD_DIR)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -pedantic graphics/framebuffer.c graphics/font.c tests/graphics_framebuffer_test.c -o $(BUILD_DIR)/graphics-framebuffer-test
	$(BUILD_DIR)/graphics-framebuffer-test

boot-test: boot/multiboot.h tests/multiboot_info_test.c | $(BUILD_DIR)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -pedantic tests/multiboot_info_test.c -o $(BUILD_DIR)/multiboot-info-test
	$(BUILD_DIR)/multiboot-info-test

drivers-test: $(BUILD_DIR)/serial.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/x86_io.o
	@printf 'Driver objects compiled (privileged port I/O was not executed).\n'

run: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO) -boot order=d,menu=on -m $(QEMU_MEMORY) -display $(QEMU_DISPLAY) -serial stdio -monitor none -D $(QEMU_LOG) -d guest_errors,cpu_reset $(QEMU_EXTRA_ARGS)

clean:
	rm -rf $(BUILD_DIR)