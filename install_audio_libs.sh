#!/bin/bash
# Script to build and install libfvad and libspecbleach from source

set -e

THIRD_PARTY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/third_party"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"

echo "Installing audio processing libraries..."
echo "Install prefix: ${INSTALL_PREFIX}"
echo ""

# Check and install prerequisites
echo "=== Checking prerequisites ==="
MISSING_DEPS=()

if ! command -v pkg-config &> /dev/null; then
    MISSING_DEPS+=("pkg-config")
fi

if ! command -v meson &> /dev/null; then
    MISSING_DEPS+=("meson")
fi

if ! command -v autoreconf &> /dev/null; then
    MISSING_DEPS+=("autotools (autoconf automake libtool)")
fi

# Check for fftw3f (required by libspecbleach)
if ! pkg-config --exists fftw3f 2>/dev/null; then
    MISSING_DEPS+=("fftw (for fftw3f)")
fi

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo "Missing dependencies:"
    for dep in "${MISSING_DEPS[@]}"; do
        echo "  - $dep"
    done
    echo ""
    echo "Install with:"
    echo "  brew install pkg-config meson autoconf automake libtool fftw"
    echo ""
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi
echo ""

# Build libfvad
echo "=== Building libfvad ==="
if [ ! -d "${THIRD_PARTY_DIR}/libfvad" ]; then
    echo "Cloning libfvad..."
    git clone --depth 1 https://github.com/dpirch/libfvad.git "${THIRD_PARTY_DIR}/libfvad"
fi

cd "${THIRD_PARTY_DIR}/libfvad"
if [ ! -f "configure" ]; then
    echo "Running autoreconf..."
    autoreconf -i
fi

echo "Configuring libfvad..."
./configure --prefix="${INSTALL_PREFIX}"

echo "Building libfvad..."
make -j$(sysctl -n hw.ncpu)

echo "Installing libfvad (may require sudo)..."
sudo make install

echo ""

# Build libspecbleach
echo "=== Building libspecbleach ==="
if [ ! -d "${THIRD_PARTY_DIR}/libspecbleach" ]; then
    echo "Cloning libspecbleach..."
    git clone --depth 1 https://github.com/lucianodato/libspecbleach.git "${THIRD_PARTY_DIR}/libspecbleach"
fi

cd "${THIRD_PARTY_DIR}/libspecbleach"

# Check if meson is installed
if ! command -v meson &> /dev/null; then
    echo "Error: meson is required to build libspecbleach"
    echo "Install it with: brew install meson"
    exit 1
fi

echo "Building libspecbleach..."
meson setup build --buildtype=release --prefix="${INSTALL_PREFIX}" --libdir=lib

echo "Compiling libspecbleach..."
meson compile -C build

echo "Installing libspecbleach (may require sudo)..."
sudo meson install -C build

echo ""
echo "=== Installation complete ==="
echo "Libraries installed to: ${INSTALL_PREFIX}"
echo ""
echo "You may need to update PKG_CONFIG_PATH:"
echo "export PKG_CONFIG_PATH=\"${INSTALL_PREFIX}/lib/pkgconfig:\$PKG_CONFIG_PATH\""
echo ""
echo "Or run:"
echo "sudo ldconfig  # On Linux"
echo "# On macOS, the libraries should be found automatically"

