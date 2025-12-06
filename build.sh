#!/bin/sh
set -e

# Detect Host
HOST_OS_RAW=$(uname -s)
HOST_OS=$(echo "$HOST_OS_RAW" | tr 'A-Z' 'a-z')
HOST_ARCH=$(uname -m)

# Defaults
TARGET="$HOST_OS"
ARCH="$HOST_ARCH"
COMMAND=""
EXTRA_ARGS=""

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
Usage: $0 [options] [command]

Options:
  target=<os>    : Target OS (default: $HOST_OS)
                   values: linux, freebsd, netbsd, qnx
  arch=<arch>    : Target Architecture (default: $HOST_ARCH)
                   values: x86_64, aarch64, i686, etc.

Commands:
  (no command)   : Build for specified target/arch
  clean          : Clean artifacts in build directory
  wipe           : Remove build directory completely
  wipeall        : Remove ALL build directories
  deb            : Build Debian package [SIGN=1 to sign]
  test           : Run tests
  asan           : Build with AddressSanitizer
  version=X.Y.Z  : Update version

Examples:
  $0 target=freebsd arch=aarch64   # Cross-compile
  $0 clean                         # Clean native build
  $0 target=qnx wipe               # Wipe QNX build dir
EOF
}

# Parse Arguments
for arg in "$@"; do
    case "$arg" in
        target=*|--target=*) TARGET=$(echo "${arg#*=}" | tr 'A-Z' 'a-z') ;;
        arch=*|--arch=*)     ARCH=$(echo "${arg#*=}" | tr 'A-Z' 'a-z') ;;
        clean)          COMMAND="clean" ;;
        wipe)           COMMAND="wipe" ;;
        wipeall)        COMMAND="wipeall" ;;
        deb)            COMMAND="deb" ;;
        test)           COMMAND="test" ;;
        asan)           COMMAND="asan" ;;
        version=*|--version=*) 
            COMMAND="version" 
            NEW_VERSION="${arg#*=}"
            ;;
        help|--help|-h) usage; exit 0 ;;
        *)
            echo "Unknown argument: $arg"
            usage
            exit 1
            ;;
    esac
done

# Normalization
# Map x86_64 to x86_64, amd64 to x86_64
if [ "$ARCH" = "amd64" ]; then ARCH="x86_64"; fi

# Determine Build Directory
if [ "$TARGET" = "$HOST_OS" ] && [ "$ARCH" = "$HOST_ARCH" ]; then
    # Native build
    BUILD_DIR="build_${HOST_OS_RAW}"
else
    # Cross build
    if [ "$TARGET" = "linux" ] && [ "$HOST_OS" = "linux" ] && [ "$ARCH" = "$HOST_ARCH" ]; then
         BUILD_DIR="build_Linux" # Explicit linux native
    else
         BUILD_DIR="build_${TARGET}_${ARCH}"
    fi
fi

# Execute Command
case "$COMMAND" in
    clean)
        need_cmd ninja "Please install ninja."
        if [ -f "$BUILD_DIR/build.ninja" ]; then
            echo "==> Cleaning $BUILD_DIR..."
            ninja -C "$BUILD_DIR" clean
        else
            echo "Build directory $BUILD_DIR does not exist or is not configured."
        fi
        exit 0
        ;;
    wipe)
        echo "==> Wiping $BUILD_DIR..."
        rm -rf "$BUILD_DIR"
        exit 0
        ;;
    wipeall)
        echo "==> Removing ALL build directories..."
        rm -rf build_* obj-* /tmp/posish_build_*
        echo "Done."
        exit 0
        ;;
    deb)
        echo "==> Building Debian package..."
        need_cmd dch "Please install 'devscripts'."
        need_cmd dpkg-buildpackage "Please install 'dpkg-dev'."
        VERSION=$(grep "version :" meson.build | cut -d "'" -f 2)
        rm -f debian/changelog.dch
        dch -v "${VERSION}-1" --distribution unstable "Build version ${VERSION}"
        if [ -n "$SIGN" ]; then
            dpkg-buildpackage
        else
            dpkg-buildpackage -us -uc
        fi
        mv ../posish_*.deb . 2>/dev/null || true
        mv ../posish_*.changes . 2>/dev/null || true
        mv ../posish_*.buildinfo . 2>/dev/null || true
        mv ../posish_*.dsc . 2>/dev/null || true
        mv ../posish_*.tar.xz . 2>/dev/null || true
        mv ../posish-dbgsym_*.ddeb . 2>/dev/null || true
        echo "Package build complete."
        exit 0
        ;;
    version)
        echo "==> Updating version to $NEW_VERSION..."
        sed -i "s/version : '[^']*'/version : '$NEW_VERSION'/" meson.build
        need_cmd dch "Please install 'devscripts'."
        dch -v "${NEW_VERSION}-1" --distribution unstable "Release $NEW_VERSION"
        echo "Version updated to $NEW_VERSION"
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
        simple_build "build_ASAN" -Db_sanitize=address -Dbuildtype=debug
        exit 0
        ;;
esac

# Build Logic
echo "==> Building for Target: $TARGET, Arch: $ARCH"
echo "    Build Directory: $BUILD_DIR"

need_cmd meson "Please install meson."
need_cmd ninja "Please install ninja."

# Check if Cross-Compilation
if [ "$TARGET" != "$HOST_OS" ] || [ "$ARCH" != "$HOST_ARCH" ]; then
    CROSS_FILE="${TARGET}-${ARCH}.txt"
    
    # Generate Cross File if not exists (or regenerate to be safe?)
    # Always regenerate logic for simplicity
    
    echo "    Generating cross-file: $CROSS_FILE"
    
    case "$TARGET" in
        linux)
             # Linux Cross Compilation (e.g. x86_64 -> aarch64)
             if [ "$ARCH" = "aarch64" ]; then
                 need_cmd aarch64-linux-gnu-gcc "Install gcc-aarch64-linux-gnu"
                 CC="aarch64-linux-gnu-gcc"
                 CPP="aarch64-linux-gnu-g++"
                 AR="aarch64-linux-gnu-ar"
                 STRIP="aarch64-linux-gnu-strip"
             elif [ "$ARCH" = "i686" ]; then
                 need_cmd i686-linux-gnu-gcc "Install gcc-i686-linux-gnu"
                 CC="i686-linux-gnu-gcc"
                 CPP="i686-linux-gnu-g++"
                 AR="i686-linux-gnu-ar"
                 STRIP="i686-linux-gnu-strip"
             else
                 die "Unsupported Linux cross-arch: $ARCH"
             fi
             
             cat > "$CROSS_FILE" <<EOF
[binaries]
c = '$CC'
cpp = '$CPP'
ar = '$AR'
strip = '$STRIP'
[host_machine]
system = 'linux'
cpu_family = '$ARCH'
cpu = '$ARCH'
endian = 'little'
EOF
             ;;
             
        freebsd)
             if [ "$ARCH" = "x86_64" ]; then
                 if command -v x86_64-unknown-freebsd14.0-gcc >/dev/null 2>&1; then
                    CC="x86_64-unknown-freebsd14.0-gcc"
                    AR="x86_64-unknown-freebsd14.0-ar"
                    STRIP="x86_64-unknown-freebsd14.0-strip"
                 else
                    need_cmd clang "Install clang"
                    CC="clang --target=x86_64-unknown-freebsd14 --sysroot=/usr/freebsd"
                    AR="llvm-ar"
                    STRIP="llvm-strip"
                 fi
             elif [ "$ARCH" = "aarch64" ]; then
                 # Example support for FreeBSD ARM64
                 need_cmd clang "Install clang"
                 CC="clang --target=aarch64-unknown-freebsd14 --sysroot=/usr/freebsd-aarch64"
                 AR="llvm-ar"
                 STRIP="llvm-strip"
             else
                 die "Unsupported FreeBSD arch: $ARCH"
             fi
             
             cat > "$CROSS_FILE" <<EOF
[binaries]
c = '$CC'
ar = '$AR'
strip = '$STRIP'
[host_machine]
system = 'freebsd'
cpu_family = '$ARCH'
cpu = '$ARCH'
endian = 'little'
EOF
             ;;

        netbsd)
             if [ "$ARCH" = "x86_64" ]; then
                 if command -v x86_64-unknown-netbsd10.0-gcc >/dev/null 2>&1; then
                    CC="x86_64-unknown-netbsd10.0-gcc"
                    AR="x86_64-unknown-netbsd10.0-ar"
                    STRIP="x86_64-unknown-netbsd10.0-strip"
                 else
                    need_cmd clang "Install clang"
                    CC="clang --target=x86_64-unknown-netbsd10 --sysroot=/usr/netbsd"
                    AR="llvm-ar"
                    STRIP="llvm-strip"
                 fi
             else
                  die "Unsupported NetBSD arch: $ARCH"
             fi
             
             cat > "$CROSS_FILE" <<EOF
[binaries]
c = '$CC'
ar = '$AR'
strip = '$STRIP'
[host_machine]
system = 'netbsd'
cpu_family = '$ARCH'
cpu = '$ARCH'
endian = 'little'
EOF
             ;;

        qnx)
             if [ -z "$QNX_HOST" ]; then
                if [ -f "$HOME/qnx800/qnxsdp-env.sh" ]; then . "$HOME/qnx800/qnxsdp-env.sh"
                elif [ -f "$HOME/qnx710/qnxsdp-env.sh" ]; then . "$HOME/qnx710/qnxsdp-env.sh"
                else die "QNX env not found."; fi
             fi
             
             if [ "$ARCH" = "x86_64" ]; then
                 QNX_ARCH="ntox86_64"
             elif [ "$ARCH" = "aarch64" ]; then
                 QNX_ARCH="ntoaarch64"
             else
                 die "Unsupported QNX arch: $ARCH"
             fi
             
             CC="$QNX_HOST/usr/bin/${QNX_ARCH}-gcc"
             CPP="$QNX_HOST/usr/bin/${QNX_ARCH}-g++"
             AR="$QNX_HOST/usr/bin/${QNX_ARCH}-ar"
             STRIP="$QNX_HOST/usr/bin/${QNX_ARCH}-strip"
             
             cat > "$CROSS_FILE" <<EOF
[binaries]
c = '$CC'
cpp = '$CPP'
ar = '$AR'
strip = '$STRIP'
[built-in options]
c_args = ['-D_QNX_SOURCE']
[properties]
sys_root = '$QNX_TARGET'
[host_machine]
system = 'qnx'
cpu_family = '$ARCH'
cpu = '$ARCH'
endian = 'little'
EOF
             ;;
             
        *)
             die "Unsupported target OS: $TARGET"
             ;;
    esac
    
    simple_build "$BUILD_DIR" --cross-file "$CROSS_FILE"
    
else
    # Native Build
    if ! setup_build "$BUILD_DIR" --buildtype=release; then
        echo "Fallback to /tmp..."
        USER_ID=$(id -u)
        BUILD_DIR="/tmp/posish_build_${HOST_OS}_${USER_ID}"
        setup_build "$BUILD_DIR" --buildtype=release
    fi
    ninja -C "$BUILD_DIR"
    
    # Copy binary to ./build_Native/posish for convenience if fallback was used
    # Or just copy to ./posish? No, legacy script copied to build_Linux/posish
    # Let's keep consistent: if we used fallback, maybe copy to local?
    # Old script: copied from BUILD_DIR to LOCAL_BUILD_DIR if they differed.
    
    LOCAL_BUILD_DIR="build_${HOST_OS_RAW}" # Restore legacy behavior for native
    if [ "$BUILD_DIR" != "$LOCAL_BUILD_DIR" ]; then
        if [ "$TARGET" = "$HOST_OS" ] && [ "$ARCH" = "$HOST_ARCH" ]; then
             mkdir -p "$LOCAL_BUILD_DIR"
             if [ -f "$BUILD_DIR/posish" ]; then
                 cp "$BUILD_DIR/posish" "$LOCAL_BUILD_DIR/"
                 echo "Binary copied to $LOCAL_BUILD_DIR/posish"
             fi
        fi
    fi
fi
