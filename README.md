# Read AI-Policy.md before this

# KelpOS

KelpOS is an x86_64 operating-system project written from scratch. Its goals are
to be lightweight and approachable, and to use its own kernel rather than the
Linux, Windows NT, or Darwin kernels.

> **Development disclosure:** KelpOS is built with AI assistance and identifies
> this workflow as **Vibecoded OSDev (V-OSDev)**. See [AI-Policy.md](AI-Policy.md)
> for the project's review, attribution, and responsibility policy.

## Current Status

The project currently contains a GRUB Multiboot v1 boot path and a minimal
x86_64 kernel. Its 32-bit bootstrap checks CPU support, creates identity-mapped
page tables for the first 4 GiB, and enters long mode before calling the kernel.
GRUB offers a graphical Desktop entry and a text-mode TTY Shell entry. The
kernel reports each startup stage to COM1 serial output. This remains an early
development system, not a feature-complete or crash-proof operating system.

The kernel initializes a fixed 64 KiB aligned heap and a bounded-polling COM1
serial diagnostic driver when the port is detected. The TTY shell accepts
polled PS/2 keyboard input and includes `help`, `about`, `clear`, `echo`, and
`mem` commands. The graphical entry uses GRUB's requested 32-bit RGB framebuffer
to draw KelpDE's desktop and an interactive terminal (`T` opens it, `Esc`
returns to the desktop; `help`, `about`, `clear`, `echo`, and `desktop` work in
the terminal). In the GRUB menu, choose **KelpOS TTY Shell** for text mode or
**KelpOS Desktop** for the graphical path.

This is an early single-core prototype: keyboard input is polled, the desktop
has no mouse or window manager, and IDT/CPU exception setup is deferred to the
final hardening pass. The shell and framebuffer desktop compile into the kernel,
but their runtime handoff still needs verification in QEMU. The heap remains a
fixed bootstrap allocator rather than a physical memory manager.

## Build And Run

The root `Makefile` is the only Makefile in this project and is the single entry
point for compiling the current OS sources and creating the bootable ISO. The
`create` target also builds the host-side CISWN prototype, verifies the boot
image, and runs KPM, CISWN, boot-layout, memory, and graphics tests. Driver checks compile
privileged I/O code but do not execute it on the host. On Arch Linux, install
the build and boot tools with:

```sh
sudo pacman -S base-devel grub xorriso qemu-full qemu-ui-gtk python
```

Build and verify the current OS:

```sh
make create
```

`make` runs the same target. The output is `build/kelpos.iso`. Build commands
are printed in full. Run it in a GTK window with verbose guest output in the
terminal using `make run` (requires QEMU's GTK UI package). QEMU diagnostics are
saved to `build/qemu-debug.log`; override the window or memory with
`QEMU_DISPLAY=sdl` or `QEMU_MEMORY=512M`. Remove generated build files with
`make clean`. The
`./bootstrap` and `./clearcache` scripts are convenience wrappers around those
root Makefile targets. Build output shows each source file being compiled; as
new subsystems are introduced, their sources and checks must be added to this
same build before a phase is considered complete.

The opt-in KelpOS cross-toolchain builds pinned `x86_64-elf` Binutils and GCC
for Linux or macOS. Install host prerequisites and run `make toolchain`; the
root build automatically prefers it after installation. See
[toolchain/README.md](toolchain/README.md) for platform packages, verification,
and the freestanding toolchain scope. The first GCC build can take a while and
requires several gigabytes of disk space.

KPM is a host-side Python tool and is not linked into the freestanding kernel.
Run only its focused tests with `make kpm-test`; see [kpm/README.md](kpm/README.md)
for building and installing a sample package into a sandbox.

CISWN is a host-buildable C service supervisor prototype, also not yet part of
the kernel or boot ISO. Run its focused checks with `make ciswn-test`; see
[ciswn/README.md](ciswn/README.md) for its config format and current limits.

Run the bootstrap heap tests with `make memory-test`, software framebuffer/font
tests with `make graphics-test`, Multiboot layout checks with `make boot-test`,
and driver object compile checks with `make drivers-test`.

## KelpOS Toolchain

The cross-toolchain is built once for the current host and then produces
`x86_64-elf` KelpOS binaries. Install the precompiled host build dependencies
first.

Linux, Debian/Ubuntu:

```sh
sudo apt install build-essential bison flex texinfo gawk curl xz-utils \
	gpg libgmp-dev libmpfr-dev libmpc-dev libisl-dev zlib1g-dev
```

Linux, Arch/Artix:

```sh
sudo pacman -S base-devel bison flex texinfo gawk curl xz gmp mpfr libmpc isl zlib gpg
```

macOS with Homebrew:

```sh
xcode-select --install
brew install gcc gmp mpfr libmpc isl zlib gawk bison texinfo xz gpg
```

Build it with `make toolchain`. This compiles pinned GNU source releases and
installs host-native compiler tools into `toolchain/local/`; the generated
executables are not shared between Linux and macOS. `make create` automatically
uses the cross compiler after it is installed. See
[toolchain/README.md](toolchain/README.md) for signature verification, build
controls, and toolchain limits. The first build can take a while and needs
several gigabytes of free disk space.

## Layout

- `boot/`: x86 boot entry and transition to long mode
- `base/`: small freestanding utility functions
- `drivers/`: VGA text terminal, PS/2 keyboard input, COM1 serial output, and x86 port I/O
- `graphics/`: framebuffer drawing, 5x7 font, and KelpDE desktop/terminal
- `grub/`: GRUB Multiboot boot menu configuration
- `kernel/`: kernel entry point, TTY command shell, and linker script
- `toolchain/`: Linux/macOS bootstrap for pinned `x86_64-elf` GCC and Binutils
- `ciswn/`: C init/service supervisor prototype and tests
- `kpm/`: Python package manager prototype and tests
- `memory/`: fixed bootstrap heap; physical/virtual memory management remains future work

## Project Direction

The intended audience is older hardware, with a lightweight desktop and a
custom C library named `klibc`. Work proceeds one component at a time. A
milestone needs a defined interface, a build path, and a repeatable check before
the next layer is built on top of it.

1. **KPM host prototype (first slice complete):** local package creation and
	sandbox operations have a tested Python implementation. KelpOS integration
	remains future work.
2. **CISWN (current component):** implement and test the service model and
	supervisor in C. The current program is host-buildable and is not yet a
	bootable KelpOS init process.
3. **Kernel foundation:** stabilize boot, kernel interfaces, panic reporting,
   and memory management with `make create` and QEMU checks. The current heap is
   only an early allocator; IDT and exception setup are deferred to the final
   hardening pass.
4. **Core runtime:** develop `klibc` and the minimum process, filesystem, and
	device interfaces needed by user-space programs. This enables future init
	and package-manager integration.
5. **CISWN integration:** port the tested service model to KelpOS after its
	process, filesystem, and signal/service interfaces exist.
6. **KPM integration:** adapt packages and installation to the KelpOS filesystem
	and runtime after those interfaces exist.
7. **SessionDM display manager:** add login/session startup after the kernel
	supports the required input, graphics, and process interfaces.
8. **KelpDE desktop:** build the desktop on top of the display manager and
	stable graphics/input APIs.
9. **Bootloader hardening:** retain Multiboot validation and QEMU boot checks
	while improving the GRUB handoff and future firmware support.

## Next ToDos

- [ ] **Build the KelpOS toolchain:** define and document the compiler, linker,
	assembler, sysroot, and reproducible build process for KelpOS programs.
- [ ] **Build a real shell and desktop environment:** start with an interactive
	shell, then build the desktop on top of the toolchain, userspace, graphics,
	and input foundations.

The roadmap is being worked through in dependency order. CISWN's host-side C
prototype and the initial kernel heap are implemented, but are not yet integrated
as a bootable userspace system. CISWN integration,
SessionDM and KelpDE are plans, not implemented features.
IDT and CPU exception setup remain deferred to the final kernel-hardening pass.
Keep the scope of each change small, review generated or AI-assisted code, and
do not claim a milestone as working until its checks pass. See
[AI-Policy.md](AI-Policy.md).

The project was previously named SquareOS and TridentOS.