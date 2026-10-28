#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET="${TARGET:-x86_64-elf}"
BINUTILS_VERSION="2.44"
GCC_VERSION="14.2.0"
HOST_CC="${HOST_CC:-${CC:-cc}}"
HOST_CXX="${HOST_CXX:-${CXX:-c++}}"
PREFIX="${PREFIX:-$SCRIPT_DIR/local}"
WORK_DIR="${WORK_DIR:-$SCRIPT_DIR/build}"
DOWNLOAD_DIR="$WORK_DIR/downloads"
SOURCE_DIR="$WORK_DIR/sources"
BUILD_DIR="$WORK_DIR/build"

die() {
    printf 'toolchain: error: %s\n' "$*" >&2
    exit 1
}

case "$(uname -s)" in
    Linux) ;;
    Darwin)
        command -v brew >/dev/null 2>&1 || die "install Homebrew first; see toolchain/README.md"
        brew_prefix="$(brew --prefix)"
        PATH="$brew_prefix/opt/bison/bin:$brew_prefix/opt/texinfo/bin:$brew_prefix/opt/gawk/libexec/gnubin:$brew_prefix/bin:$PATH"
        export PATH
        ;;
    *) die "supported build hosts are Linux and macOS (Darwin); found $(uname -s)" ;;
esac

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
    cat <<'EOF'
Usage: toolchain/bootstrap.sh [--check]

Build the pinned x86_64-elf Binutils and GCC cross-toolchain for the current
Linux or macOS host. Output defaults to toolchain/local; set PREFIX, WORK_DIR,
JOBS, HOST_CC, or HOST_CXX to customize the build.

--check  Check basic host tools and print platform dependency guidance without
         downloading or building sources.

Set VERIFY_SIGNATURES=required and GNUPGHOME to require verification of the
GNU detached release signatures before extracting source archives.
EOF
    exit 0
fi

CHECK_ONLY=false
if [[ ${1:-} == --check ]]; then
    CHECK_ONLY=true
elif [[ $# -ne 0 ]]; then
    die "unknown argument: $1 (use --help)"
fi

for tool in curl tar xz make bison flex gawk makeinfo; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        die "missing required host tool: $tool"
    fi
done
command -v "$HOST_CC" >/dev/null 2>&1 || die "missing host C compiler: $HOST_CC"
command -v "$HOST_CXX" >/dev/null 2>&1 || die "missing host C++ compiler: $HOST_CXX"

if $CHECK_ONLY; then
    printf 'toolchain: host check passed for %s/%s; target=%s\n' \
        "$(uname -s)" "$(uname -m)" "$TARGET"
    printf 'toolchain: read toolchain/README.md for host package dependencies\n'
    exit 0
fi

if ! command -v gpg >/dev/null 2>&1; then
    if [[ ${VERIFY_SIGNATURES:-auto} == required ]]; then
        die "VERIFY_SIGNATURES=required, but gpg is not installed"
    fi
    printf 'toolchain: warning: gpg unavailable; downloaded source signatures cannot be checked\n' >&2
elif [[ -z "${GNUPGHOME:-}" ]]; then
    if [[ ${VERIFY_SIGNATURES:-auto} == required ]]; then
        die "set GNUPGHOME to a keyring containing trusted GNU release keys"
    fi
    printf 'toolchain: warning: set GNUPGHOME with trusted GNU release keys to verify detached signatures\n' >&2
fi

if [[ -n "${JOBS:-}" ]]; then
    BUILD_JOBS="$JOBS"
elif [[ $(uname -s) == Darwin ]]; then
    BUILD_JOBS="$(sysctl -n hw.ncpu)"
else
    BUILD_JOBS="$(getconf _NPROCESSORS_ONLN)"
fi
if (( BUILD_JOBS > 8 )); then
    BUILD_JOBS=8
fi

mkdir -p "$DOWNLOAD_DIR" "$SOURCE_DIR" "$BUILD_DIR" "$PREFIX"
mkdir -p "$PREFIX/$TARGET/sysroot"

verify_signature() {
    local archive="$1"
    local signature="$2"

    if [[ -z "${GNUPGHOME:-}" || ! -d ${GNUPGHOME:-/nonexistent} ]]; then
        [[ ${VERIFY_SIGNATURES:-auto} != required ]] || die "GNU signature verification is required but GNUPGHOME is not configured"
        return
    fi
    command -v gpg >/dev/null 2>&1 || die "GNU signature verification requires gpg"
    gpg --batch --verify "$signature" "$archive" \
        || die "signature verification failed for $(basename "$archive"); verify that GNUPGHOME contains trusted GNU release keys"
}

fetch_release() {
    local name="$1"
    local base_url="$2"
    local archive="$DOWNLOAD_DIR/$name"
    local signature="$archive.sig"

    if [[ ! -s $archive ]]; then
        printf 'toolchain: downloading %s\n' "$name"
        curl --fail --location --retry 3 --output "$archive.part" "$base_url/$name"
        mv "$archive.part" "$archive"
    fi
    if [[ ! -s $signature ]]; then
        curl --fail --location --retry 3 --output "$signature.part" "$base_url/$name.sig"
        mv "$signature.part" "$signature"
    fi
    verify_signature "$archive" "$signature"
}

extract_release() {
    local archive="$1"
    local expected_directory="$2"
    local destination="$SOURCE_DIR/$expected_directory"

    if [[ ! -d $destination ]]; then
        tar -xf "$archive" -C "$SOURCE_DIR"
    fi
    [[ -d $destination ]] || die "archive did not create expected source directory: $expected_directory"
    printf '%s\n' "$destination"
}

BINUTILS_ARCHIVE="binutils-$BINUTILS_VERSION.tar.xz"
GCC_ARCHIVE="gcc-$GCC_VERSION.tar.xz"
fetch_release "$BINUTILS_ARCHIVE" "https://ftp.gnu.org/gnu/binutils"
fetch_release "$GCC_ARCHIVE" "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION"

BINUTILS_SOURCE="$(extract_release "$DOWNLOAD_DIR/$BINUTILS_ARCHIVE" "binutils-$BINUTILS_VERSION")"
GCC_SOURCE="$(extract_release "$DOWNLOAD_DIR/$GCC_ARCHIVE" "gcc-$GCC_VERSION")"

case "$(uname -s)" in
    Darwin)
        if ! command -v brew >/dev/null 2>&1; then
            die "install Homebrew first; see toolchain/README.md"
        fi
        brew_prefix="$(brew --prefix)"
        HOST_CPPFLAGS="-I$brew_prefix/include ${CPPFLAGS:-}"
        HOST_LDFLAGS="-L$brew_prefix/lib ${LDFLAGS:-}"
        ;;
    Linux)
        HOST_CPPFLAGS="${CPPFLAGS:-}"
        HOST_LDFLAGS="${LDFLAGS:-}"
        ;;
esac

printf 'toolchain: building %s for %s on %s/%s\n' \
    "$TARGET" "$(uname -s)" "$(uname -m)" "$BUILD_JOBS"

mkdir -p "$BUILD_DIR/binutils"
(
    cd "$BUILD_DIR/binutils"
    CC="$HOST_CC" CXX="$HOST_CXX" CPPFLAGS="$HOST_CPPFLAGS" LDFLAGS="$HOST_LDFLAGS" \
        "$BINUTILS_SOURCE/configure" \
        --target="$TARGET" \
        --prefix="$PREFIX" \
        --with-sysroot="$PREFIX/$TARGET/sysroot" \
        --disable-nls \
        --disable-werror \
        --disable-gdb \
        --disable-gprofng \
        --disable-sim \
        --disable-libdecnumber \
        --disable-readline \
        --disable-gold
    make -j"$BUILD_JOBS"
    make install
)

mkdir -p "$BUILD_DIR/gcc"
(
    cd "$BUILD_DIR/gcc"
    CC="$HOST_CC" CXX="$HOST_CXX" CPPFLAGS="$HOST_CPPFLAGS" LDFLAGS="$HOST_LDFLAGS" \
        "$GCC_SOURCE/configure" \
        --target="$TARGET" \
        --prefix="$PREFIX" \
        --with-sysroot="$PREFIX/$TARGET/sysroot" \
        --with-native-system-header-dir=/include \
        --disable-nls \
        --disable-werror \
        --disable-multilib \
        --disable-shared \
        --disable-threads \
        --disable-libatomic \
        --disable-libgomp \
        --disable-libquadmath \
        --disable-libssp \
        --disable-libstdcxx \
        --enable-languages=c,c++ \
        --without-headers
    make -j"$BUILD_JOBS" all-gcc
    make install-gcc
    make -j"$BUILD_JOBS" all-target-libgcc
    make install-target-libgcc
)

cat > "$PREFIX/env.sh" <<EOF
export KELPOS_TOOLCHAIN_PREFIX="$PREFIX"
export PATH="$PREFIX/bin:\$PATH"
EOF

"$PREFIX/bin/$TARGET-gcc" --version | head -n 1
"$PREFIX/bin/$TARGET-as" --version | head -n 1
"$PREFIX/bin/$TARGET-ld" --version | head -n 1
printf 'toolchain: installed in %s\n' "$PREFIX"
printf 'toolchain: in this shell run: source "%s/env.sh"\n' "$PREFIX"
printf 'toolchain: KelpOS targets can also select it with make CROSS_COMPILE=%s/bin/%s-\n' "$PREFIX" "$TARGET"
