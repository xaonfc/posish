#!/bin/sh
set -e

# Use OS-specific build directory to support shared folders (NFS/VirtualBox)
OS_NAME=$(uname -s)
BUILD_DIR="build_${OS_NAME}"

# Check dependencies
if ! command -v meson >/dev/null 2>&1; then
    echo "Error: meson is not installed in your system."
    echo "Please install it via your package manager"
    exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
    echo "Error: ninja is not installed in your system."
    echo "Please install it via your package manager"
    exit 1
fi

# Handle arguments
case "$1" in
    clean)
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
    deb)
        echo "==> Building Debian package..."
        
        # Check dependencies
        if ! command -v dch >/dev/null 2>&1; then
            echo "Error: 'dch' not found. Please install 'devscripts'."
            exit 1
        fi
        if ! command -v dpkg-buildpackage >/dev/null 2>&1; then
            echo "Error: 'dpkg-buildpackage' not found. Please install 'dpkg-dev'."
            exit 1
        fi

        # Extract version from meson.build
        VERSION=$(grep "version :" meson.build | cut -d "'" -f 2)
        echo "Detected version: $VERSION"

        # Update changelog
        rm -f debian/changelog.dch
        dch -v "${VERSION}-1" --distribution unstable "Build version ${VERSION}"

        # Build package
        dpkg-buildpackage -us -uc

        # Move artifacts
        mv ../posish_*.deb . 2>/dev/null || true
        mv ../posish_*.changes . 2>/dev/null || true
        mv ../posish_*.buildinfo . 2>/dev/null || true
        mv ../posish_*.dsc . 2>/dev/null || true
        mv ../posish_*.tar.xz . 2>/dev/null || true
        mv ../posish-dbgsym_*.ddeb . 2>/dev/null || true

        echo ""
        echo "Package build complete."
        ls -1 posish_*.deb
        exit 0
        ;;
    test)
        echo "==> Running tests..."
        if ! command -v pytest >/dev/null 2>&1; then
            echo "Error: pytest not found. Please install python3-pytest."
            exit 1
        fi
        pytest tests/
        exit 0
        ;;
    asan)
        echo "==> Building with AddressSanitizer..."
        BUILD_DIR="build_ASAN"
        if [ ! -d "$BUILD_DIR" ]; then
            meson setup "$BUILD_DIR" . -Db_sanitize=address -Dbuildtype=debug
        fi
        ninja -C "$BUILD_DIR"
        echo "Binary location: $BUILD_DIR/posish"
        exit 0
        ;;
    target=*|--target=*)
        # Cross-compilation for different operating systems
        TARGET=$(echo "$1" | cut -d= -f2)
        case "$TARGET" in
            qnx)
                echo "==> Cross-compiling for QNX..."
                
                # Check QNX SDP environment
                if [ -z "$QNX_HOST" ]; then
                    if [ -f "$HOME/qnx800/qnxsdp-env.sh" ]; then
                        echo "Sourcing QNX SDP environment..."
                        . "$HOME/qnx800/qnxsdp-env.sh"
                    elif [ -f "$HOME/qnx710/qnxsdp-env.sh" ]; then
                        . "$HOME/qnx710/qnxsdp-env.sh"
                    else
                        echo "Error: QNX SDP environment not found."
                        echo "Please source qnxsdp-env.sh first or install QNX SDP."
                        exit 1
                    fi
                fi
                
                # Verify QNX environment variables
                if [ -z "$QNX_HOST" ] || [ -z "$QNX_TARGET" ]; then
                    echo "Error: QNX_HOST or QNX_TARGET not set."
                    echo "Please source qnxsdp-env.sh from your QNX SDP installation."
                    exit 1
                fi
                
                # Check for cross-compiler
                QNX_GCC="$QNX_HOST/usr/bin/ntox86_64-gcc"
                if [ ! -x "$QNX_GCC" ]; then
                    echo "Error: QNX cross-compiler not found at $QNX_GCC"
                    exit 1
                fi
                
                # Generate cross-file dynamically
                echo "Generating qnx-x86_64.txt cross-file..."
                cat > qnx-x86_64.txt << EOF
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
                
                BUILD_DIR="build_qnx"
                if [ ! -d "$BUILD_DIR" ]; then
                    meson setup "$BUILD_DIR" . --cross-file qnx-x86_64.txt
                fi
                ninja -C "$BUILD_DIR"
                echo ""
                echo "QNX build complete."
                echo "Binary location: $BUILD_DIR/posish"
                exit 0
                ;;
            *)
                echo "Error: Unknown target OS '$TARGET'"
                echo "Available targets: qnx"
                exit 1
                ;;
        esac
        ;;
    arch=*|--arch=*)
        # Cross-compilation for different architectures (Linux)
        ARCH=$(echo "$1" | cut -d= -f2)
        case "$ARCH" in
            aarch64|arm64)
                echo "==> Cross-compiling for AArch64 (ARM64) Linux..."
                
                # Check for cross-compiler
                if ! command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
                    echo "Error: aarch64-linux-gnu-gcc not found."
                    echo "Install with: sudo apt install gcc-aarch64-linux-gnu"
                    exit 1
                fi
                
                # Create cross-file if it doesn't exist
                if [ ! -f "aarch64-linux.txt" ]; then
                    echo "Creating aarch64-linux.txt cross-file..."
                    cat > aarch64-linux.txt << 'EOF'
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
                
                BUILD_DIR="build_aarch64"
                if [ ! -d "$BUILD_DIR" ]; then
                    meson setup "$BUILD_DIR" . --cross-file aarch64-linux.txt
                fi
                ninja -C "$BUILD_DIR"
                echo ""
                echo "AArch64 Linux build complete."
                echo "Binary location: $BUILD_DIR/posish"
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
        echo "Usage: $0 [command]"
        echo ""
        echo "Commands:"
        echo "  (no args)      : Build the project for current platform"
        echo "  clean          : Clean build artifacts (ninja clean)"
        echo "  wipe           : Remove build directory completely"
        echo "  deb            : Build Debian package (.deb)"
        echo "  test           : Run test suite (pytest)"
        echo "  asan           : Build with AddressSanitizer"
        echo ""
        echo "Cross-compilation:"
        echo "  target=<os>    : Cross-compile for a different OS"
        echo "    target=qnx   : QNX Neutrino (requires QNX SDP)"
        echo ""
        echo "  arch=<arch>    : Cross-compile for a different architecture (Linux)"
        echo "    arch=aarch64 : ARM64 Linux (requires gcc-aarch64-linux-gnu)"
        exit 0
        ;;
esac

# Function to attempt build setup
setup_build() {
    TARGET_DIR=$1
    echo "==> Setting up build directory in $TARGET_DIR..."
    
    # Clean if exists but broken (missing build.ninja)
    if [ -d "$TARGET_DIR" ] && [ ! -f "$TARGET_DIR/build.ninja" ]; then
         rm -rf "$TARGET_DIR"
    fi

    if meson setup "$TARGET_DIR" . --buildtype=release; then
        return 0
    else
        return 1
    fi
}

# Try standard build dir first
if ! setup_build "$BUILD_DIR"; then
    echo "Warning: Failed to setup build in $BUILD_DIR."
    echo "Attempting fallback to /tmp (useful for NFS/Shared Folders)..."
    
    # Use a unique temp directory based on user ID to avoid permission issues
    USER_ID=$(id -u)
    BUILD_DIR="/tmp/posish_build_${OS_NAME}_${USER_ID}"
    
    if ! setup_build "$BUILD_DIR"; then
        echo ""
        echo "Error: Meson setup failed even in /tmp."
        echo "Please check your Meson installation and permissions."
        exit 1
    fi
fi

# Build
echo "==> Building..."
ninja -C "$BUILD_DIR"

# Copy binary to OS-specific build directory if we used a fallback
LOCAL_BUILD_DIR="build_${OS_NAME}"

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

echo ""
echo "Build complete."
echo "Binary location: $LOCAL_BUILD_DIR/posish"
