#!/bin/bash
# Installazione completa: binario + CSS + desktop entry + icone
PREFIX="${PREFIX:-/usr/local}"

if [[ $EUID -ne 0 ]]; then
    echo "Questo script deve essere eseguito come root (sudo)"
    exit 1
fi

cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$PREFIX -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cmake --install build --prefix "$PREFIX"

# Ricarica il database delle icone
if command -v update-desktop-database &> /dev/null; then
    sudo update-desktop-database /usr/share/applications
fi

echo "✓ antiX Sidebar installato in $PREFIX"
echo "  Avvia da menu o con: antix-sidebar"
