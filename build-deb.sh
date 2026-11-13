#!/usr/bin/env bash
set -euo pipefail

PACKAGE="antix-sidebar"
VERSION="1.0.1"
ARCHITECTURE="$(dpkg --print-architecture)"
MAINTAINER="Vortix Studio <vortix.studio@ik.me>"

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
PACKAGE_ROOT="$ROOT_DIR/package-root"
OUTPUT_DIR="$ROOT_DIR/dist"
DEBIAN_DIR="$PACKAGE_ROOT/DEBIAN"
DEB_FILE="$OUTPUT_DIR/${PACKAGE}_${VERSION}_${ARCHITECTURE}.deb"

# Check that the required tools are installed.
for tool in cmake dpkg dpkg-deb; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Error: required command not found: $tool" >&2
        exit 1
    fi
done

# Remove files from previous builds and packages.
rm -rf "$BUILD_DIR" "$PACKAGE_ROOT"
mkdir -p "$DEBIAN_DIR" "$OUTPUT_DIR"
rm -f "$DEB_FILE"

# Configure and build the application.
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" --parallel

# Install the application files into the package directory structure.
DESTDIR="$PACKAGE_ROOT" cmake --install "$BUILD_DIR"

# Create the control file containing the Debian package metadata.
cat > "$DEBIAN_DIR/control" <<EOF
Package: $PACKAGE
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCHITECTURE
Maintainer: $MAINTAINER
Depends: libc6, libgtk-3-0 (>= 3.24), libcairo2 (>= 1.16), libcurl4, libxml2, libx11-6
Recommends: playerctl, icewm (>= 1.7)
Description: sidebar application for antiX
 Application with a calendar, weather, news, and audio controls.
EOF

# Build the Debian package.
dpkg-deb --root-owner-group --build "$PACKAGE_ROOT" "$DEB_FILE"

echo
echo "Package created: $DEB_FILE"
echo
echo "Package contents:"
dpkg-deb --contents "$DEB_FILE"
