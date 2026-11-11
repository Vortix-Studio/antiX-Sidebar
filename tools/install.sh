#!/bin/bash
# Full installation: binary + CSS + desktop entry + icons
PREFIX="${PREFIX:-/usr/local}"

if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root (sudo)"
    exit 1
fi

cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cmake --install build --prefix "$PREFIX"

# Refresh the desktop database
if command -v update-desktop-database &> /dev/null; then
    sudo update-desktop-database /usr/share/applications
fi

echo "✓ antiX Sidebar installed in $PREFIX"
echo "  Launch it from the menu or run: antix-sidebar"
