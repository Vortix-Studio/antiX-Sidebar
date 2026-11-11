#!/bin/bash
# Complete uninstallation
PREFIX="${PREFIX:-/usr/local}"

if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root (sudo)"
    exit 1
fi

rm -f "$PREFIX/bin/antix-sidebar"
rm -rf "$PREFIX/share/antix-sidebar"
rm -f "/usr/share/applications/antix-sidebar.desktop"
rm -f "/usr/share/icons/hicolor/scalable/apps/antix-sidebar.svg"

if command -v update-desktop-database &> /dev/null; then
    sudo update-desktop-database /usr/share/applications
fi

echo "✓ antiX Sidebar uninstalled"
