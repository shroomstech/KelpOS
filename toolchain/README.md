# KelpOS Cross-Toolchain

This directory bootstraps a native-hosted cross-toolchain that emits code for
KelpOS's bare-metal target, `x86_64-elf`. The same script supports Linux and
macOS hosts; it builds host-appropriate executable files rather than trying to
share Linux ELF binaries with macOS or macOS Mach-O binaries with Linux. The
result is a precompiled toolchain for that host, built locally from pinned GNU
source releases.

The bootstrap builds pinned GNU Binutils **2.44** and GCC **14.2.0** from their
official GNU release archives. GCC's GMP, MPFR, MPC, ISL, and zlib dependencies
are precompiled host packages installed by the platform package manager. The
target compiler, assembler, linker, and `libgcc` are built locally and installed
under `toolchain/local/`. Generated files and downloaded sources are not
committed.

## Host Dependencies

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

The compiler used to build the cross-toolchain must support the host system.
Override it with `HOST_CC` and `HOST_CXX` if the defaults are not suitable. On
macOS, Homebrew libraries are discovered through `brew --prefix`.

## Build

Check basic host tools without downloading or compiling anything:

```sh
./toolchain/bootstrap.sh --check
```

Build the cross-toolchain:

```sh
make toolchain
```

The build is intentionally opt-in because compiling GCC can take a while and
needs several gigabytes of free disk space. It uses up to 8 parallel jobs by
default; set `JOBS` to change this. Set `PREFIX` or `WORK_DIR` to move the
installation or generated sources/build tree.

The script downloads only the pinned GNU releases and their detached `.sig`
files. By default, it warns and continues when no trusted GNU signing keyring
is configured. To require signature verification, install/import the GNU release
keys into a trusted GPG home and run:

```sh
VERIFY_SIGNATURES=required GNUPGHOME=/path/to/trusted-gnu-keyring make toolchain
```

After installation, `make create` automatically uses `toolchain/local/bin/`
when its `x86_64-elf-gcc` exists. To use it explicitly in the current shell:

```sh
source toolchain/local/env.sh
make clean
make create
```

## What It Provides

- `x86_64-elf-gcc` and `x86_64-elf-g++` for KelpOS-targeted C/C++ compilation
- `x86_64-elf-as` and `x86_64-elf-ld` for target assembly and linking
- target `libgcc` for compiler runtime helpers
- a target sysroot location reserved for KelpOS headers and libraries

This is a **freestanding compiler toolchain**, not a complete hosted C/C++
environment. It deliberately does not install a host libc into KelpOS or provide
target `libstdc++`; KelpOS must implement its own headers/runtime before hosted
C++ applications can be supported. The root build still uses the host compiler
for runnable CISWN and test executables, and uses the cross compiler for the
kernel.

The produced binaries are specific to the host OS and architecture. Re-run the
bootstrap separately on Linux and macOS to produce a native toolchain for each.