#!/bin/sh
set -e

OS_NAME=$(uname -s)
LOCAL_BUILD_DIR="build_${OS_NAME}"
BUILD_DIR=$LOCAL_BUILD_DIR

die() {
    echo "Error: $*" >&2
    exit 1
}

need_cmd() {
    cmd=$1
    shift
    if ! command -v "$cmd" >/dev/null 2>&1; then
        if [ "$#" -gt 0 ]; then
            die "$cmd not found. $*"
        else
            die "$cmd not found in PATH."
        fi
    fi
}

# For simple one-off builds (no /tmp fallback)
simple_build() {
    dir=$1
    shift
    if [ ! -d "$dir" ]; then
        meson setup "$dir" . "$@"
    fi
    ninja -C "$dir"
    echo
    echo "Binary location: $dir/posish"
}

# For default build with /tmp fallback
setup_build() {
    dir=$1
    shift
    # Clean if exists but broken (missing build.ninja)
    if [ -d "$dir" ] && [ ! -f "$dir/build.ninja" ]; then
        rm -rf "$dir"
    fi
    meson setup "$dir" . "$@"
}

usage() {
    cat <<EOF
Usage: $0 [command]

Commands:
  (no args)      : Build the project for current platform
  clean          : Clean build artifacts (ninja clean + temp dirs)
  wipe           : Remove build directory completely
  wipeall        : Remove ALL build directories (Linux, FreeBSD, QNX, etc.)
  deb            : Build Debian package (.deb) [SIGN=1 to sign]
  test           : Run test suite (pytest)
  asan           : Build with AddressSanitizer

Version:
  version=X.Y.Z  : Update version in meson.build and debian/changelog

Cross-compilation:
  target=<os>    : Build for a specific OS
    target=linux   : Linux (native build)
    target=freebsd : FreeBSD (cross-compile)
    target=qnx     : QNX Neutrino (requires QNX SDP)

  arch=<arch>    : Cross-compile for a different architecture (Linux)
    arch=aarch64 : ARM64 Linux (requires gcc-aarch64-linux-gnu)
EOF
}

case "$1" in
    clean)
        need_cmd ninja "Please install it via your package manager."

        # Clean local build dir
        if [ -f "$BUILD_DIR/build.ninja" ]; then
            echo "==> Cleaning artifacts..."
            ninja -C "$BUILD_DIR" clean
        elif [ -d "$BUILD_DIR" ]; then
            echo "==> Removing binary from $BUILD_DIR..."
            rm -f "$BUILD_DIR/posish"
        else
            echo "Build directory does not exist."
        fi

        # Clean fallback dir if it exists
        USER_ID=$(id -u)
        TMP_BUILD="/tmp/posish_build_${OS_NAME}_${USER_ID}"
        if [ -d "$TMP_BUILD" ]; then
            echo "==> Cleaning fallback build directory ($TMP_BUILD)..."
            if [ -f "$TMP_BUILD/build.ninja" ]; then
                ninja -C "$TMP_BUILD" clean
            else
                rm -rf "$TMP_BUILD"
            fi
        fi

        # Clean Debian build artifacts
        if [ -d "obj-x86_64-linux-gnu" ]; then
            echo "==> Cleaning Debian build artifacts..."
            rm -rf obj-*
        fi
        exit 0
        ;;
    wipe)
        echo "==> Removing build directory..."
        rm -rf "$BUILD_DIR"
        rm -rf obj-*
        exit 0
        ;;
    wipeall)
        echo "==> Removing ALL build directories..."
        rm -rf build_Linux build_FreeBSD build_qnx build_freebsd build_ASAN build_aarch64
        rm -rf obj-*
        # Clean /tmp fallback dirs
        rm -rf /tmp/posish_build_*
        echo "All build directories removed."
        exit 0
        ;;
    deb)
        echo "==> Building Debian package..."

        need_cmd dch "Please install 'devscripts'."
        need_cmd dpkg-buildpackage "Please install 'dpkg-dev'."

        VERSION=$(grep "version :" meson.build | cut -d "'" -f 2)
        echo "Detected version: $VERSION"

        rm -f debian/changelog.dch
        dch -v "${VERSION}-1" --distribution unstable "Build version ${VERSION}"

        if [ -n "$SIGN" ]; then
            dpkg-buildpackage  # Signs with GPG key
        else
            dpkg-buildpackage -us -uc  # Unsigned
        fi

        mv ../posish_*.deb       . 2>/dev/null || true
        mv ../posish_*.changes   . 2>/dev/null || true
        mv ../posish_*.buildinfo . 2>/dev/null || true
        mv ../posish_*.dsc       . 2>/dev/null || true
        mv ../posish_*.tar.xz    . 2>/dev/null || true
        mv ../posish-dbgsym_*.ddeb . 2>/dev/null || true

        echo
        echo "Package build complete."
        ls -1 posish_*.deb
        exit 0
        ;;
    version=*|--version=*)
        NEW_VERSION=${1#*=}
        echo "==> Updating version to $NEW_VERSION..."

        # Update meson.build
        sed -i "s/version : '[^']*'/version : '$NEW_VERSION'/" meson.build
        echo "Updated meson.build"

        # Update debian/changelog
        need_cmd dch "Please install 'devscripts'."
        dch -v "${NEW_VERSION}-1" --distribution unstable "Release $NEW_VERSION"
        echo "Updated debian/changelog"

        echo
        echo "Version updated to $NEW_VERSION"
        echo "Don't forget to: git add meson.build debian/changelog && git commit"
        exit 0
        ;;
    test)
        echo "==> Running tests..."
        need_cmd pytest "Please install python3-pytest."
        pytest tests/
        exit 0
        ;;
    asan)
        echo "==> Building with AddressSanitizer..."
        need_cmd meson "Please install it via your package manager."
        need_cmd ninja "Please install it via your package manager."
        simple_build "build_ASAN" -Db_sanitize=address -Dbuildtype=debug
        exit 0
        ;;
    target=*|--target=*)
        TARGET=${1#*=}
        need_cmd meson "Please install it via your package manager."
        need_cmd ninja "Please install it via your package manager."

        case "$TARGET" in
            qnx)
                echo "==> Cross-compiling for QNX..."

                if [ -z "$QNX_HOST" ]; then
                    if [ -f "$HOME/qnx800/qnxsdp-env.sh" ]; then
                        echo "Sourcing QNX SDP environment..."
                        . "$HOME/qnx800/qnxsdp-env.sh"
                    elif [ -f "$HOME/qnx710/qnxsdp-env.sh" ]; then
                        . "$HOME/qnx710/qnxsdp-env.sh"
                    else
                        die "QNX SDP environment not found. Please source qnxsdp-env.sh first."
                    fi
                fi

                if [ -z "$QNX_HOST" ] || [ -z "$QNX_TARGET" ]; then
                    die "QNX_HOST or QNX_TARGET not set. Please source qnxsdp-env.sh."
                fi

                QNX_GCC="$QNX_HOST/usr/bin/ntox86_64-gcc"
                [ -x "$QNX_GCC" ] || die "QNX cross-compiler not found at $QNX_GCC"

                echo "Generating qnx-x86_64.txt cross-file..."
                cat > qnx-x86_64.txt <<EOF
[binaries]
c = '$QNX_HOST/usr/bin/ntox86_64-gcc'
cpp = '$QNX_HOST/usr/bin/ntox86_64-g++'
ar = '$QNX_HOST/usr/bin/ntox86_64-ar'
strip = '$QNX_HOST/usr/bin/ntox86_64-strip'

[built-in options]
c_args = ['-D_QNX_SOURCE']

[properties]
sys_root = '$QNX_TARGET'

[host_machine]
system = 'qnx'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
EOF
                simple_build "build_qnx" --cross-file qnx-x86_64.txt
                echo "QNX build complete."
                exit 0
                ;;
            freebsd)
                echo "==> Cross-compiling for FreeBSD..."

                if command -v x86_64-unknown-freebsd14.0-gcc >/dev/null 2>&1; then
                    FREEBSD_CC="x86_64-unknown-freebsd14.0-gcc"
                    FREEBSD_AR="x86_64-unknown-freebsd14.0-ar"
                    FREEBSD_STRIP="x86_64-unknown-freebsd14.0-strip"
                else
                    need_cmd clang "Install clang or a FreeBSD cross-compiler."
                    FREEBSD_CC="clang --target=x86_64-unknown-freebsd14 --sysroot=/usr/freebsd"
                    FREEBSD_AR="llvm-ar"
                    FREEBSD_STRIP="llvm-strip"
                fi

                echo "Creating freebsd-x86_64.txt cross-file..."
                cat > freebsd-x86_64.txt <<EOF
[binaries]
c = '$FREEBSD_CC'
ar = '$FREEBSD_AR'
strip = '$FREEBSD_STRIP'

[host_machine]
system = 'freebsd'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
EOF
                simple_build "build_freebsd" --cross-file freebsd-x86_64.txt
                echo "FreeBSD build complete."
                exit 0
                ;;
            linux)
                echo "==> Building for Linux (native)..."
                simple_build "build_Linux" --buildtype=release
                echo "Linux build complete."
                exit 0
                ;;
            *)
                echo "Error: Unknown target OS '$TARGET'"
                echo "Available targets: linux, freebsd, qnx"
                exit 1
                ;;
        esac
        ;;
    arch=*|--arch=*)
        ARCH=${1#*=}
        need_cmd meson "Please install it via your package manager."
        need_cmd ninja "Please install it via your package manager."

        case "$ARCH" in
            aarch64|arm64)
                echo "==> Cross-compiling for AArch64 (ARM64) Linux..."

                need_cmd aarch64-linux-gnu-gcc \
                    "Install with: sudo apt install gcc-aarch64-linux-gnu"

                if [ ! -f "aarch64-linux.txt" ]; then
                    echo "Creating aarch64-linux.txt cross-file..."
                    cat > aarch64-linux.txt <<'EOF'
[binaries]
c = 'aarch64-linux-gnu-gcc'
cpp = 'aarch64-linux-gnu-g++'
ar = 'aarch64-linux-gnu-ar'
strip = 'aarch64-linux-gnu-strip'

[host_machine]
system = 'linux'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
EOF
                fi

                simple_build "build_aarch64" --cross-file aarch64-linux.txt
                echo "AArch64 Linux build complete."
                exit 0
                ;;
            *)
                echo "Error: Unknown architecture '$ARCH'"
                echo "Available architectures: aarch64"
                exit 1
                ;;
        esac
        ;;
    help|--help|-h)
        usage
        exit 0
        ;;
esac

# Default: native build with /tmp fallback
need_cmd meson "Please install it via your package manager."
need_cmd ninja "Please install it via your package manager."

if ! setup_build "$BUILD_DIR" --buildtype=release; then
    echo "Warning: Failed to setup build in $BUILD_DIR."
    echo "Attempting fallback to /tmp (useful for NFS/Shared Folders)..."

    USER_ID=$(id -u)
    BUILD_DIR="/tmp/posish_build_${OS_NAME}_${USER_ID}"

    if ! setup_build "$BUILD_DIR" --buildtype=release; then
        echo
        die "Meson setup failed even in /tmp. Please check your Meson installation and permissions."
    fi
fi

echo "==> Building..."
ninja -C "$BUILD_DIR"

if [ "$BUILD_DIR" != "$LOCAL_BUILD_DIR" ]; then
    mkdir -p "$LOCAL_BUILD_DIR"
    if [ -f "$BUILD_DIR/posish" ]; then
        cp "$BUILD_DIR/posish" "$LOCAL_BUILD_DIR/"
    fi
fi

# Cleanup legacy build dir if it exists
if [ -d "build" ]; then
    rm -rf "build"
fi

echo
echo "Build complete."
echo "Binary location: $LOCAL_BUILD_DIR/posish"
