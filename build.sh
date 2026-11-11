#!/usr/bin/env bash
# build.sh – compila e installa la antiX Sidebar
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
INSTALL_PREFIX="${1:-/usr/local}"

echo "╔══════════════════════════════════════╗"
echo "║   antiX Sidebar – Build Script       ║"
echo "╚══════════════════════════════════════╝"
echo ""
echo "▸ Directory progetto : $PROJECT_DIR"
echo "▸ Directory build    : $BUILD_DIR"
echo "▸ Prefisso install   : $INSTALL_PREFIX"
echo ""

# ── dipendenze ────────────────────────────────────────────────────────────────
echo "── Controllo dipendenze ──"
MISSING=()
for pkg in cmake pkg-config libgtk-3-dev libcairo2-dev libx11-dev curl; do
    if ! dpkg -s "$pkg" &>/dev/null 2>&1; then
        MISSING+=("$pkg")
    fi
done

# WebKit2GTK: accetta sia 4.1 che 4.0
WEBKIT_PKG=""
for v in libwebkit2gtk-4.1-dev libwebkit2gtk-4.0-dev; do
    if dpkg -s "$v" &>/dev/null 2>&1; then
        WEBKIT_PKG="$v"
        break
    fi
done
if [ -z "$WEBKIT_PKG" ]; then
    MISSING+=(libwebkit2gtk-4.1-dev)
fi

if [ ${#MISSING[@]} -gt 0 ]; then
    echo "⚠  Pacchetti mancanti: ${MISSING[*]}"
    echo "   Installa con:"
    echo "   sudo apt install ${MISSING[*]}"
    exit 1
fi
echo "✔  Tutte le dipendenze sono presenti."
echo ""

# ── cmake configure ───────────────────────────────────────────────────────────
echo "── Configurazione CMake ──"
mkdir -p "$BUILD_DIR"
cmake -B "$BUILD_DIR" -S "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"

echo ""

# ── build ─────────────────────────────────────────────────────────────────────
echo "── Compilazione ──"
cmake --build "$BUILD_DIR" --parallel "$(nproc)"
echo ""

# ── install (opzionale) ──────────────────────────────────────────────────────
if [ "${INSTALL:-0}" = "1" ]; then
    echo "── Installazione in $INSTALL_PREFIX ──"
    cmake --install "$BUILD_DIR"
    echo "✔  Installazione completata."
else
    echo "── Esecuzione diretta (senza install) ──"
    echo "   Esegui: $BUILD_DIR/antix-sidebar"
    echo "   (I temi CSS sono integrati direttamente nel binario!)"
    echo ""
    echo "   Oppure imposta INSTALL=1 per installare:"
    echo "   INSTALL=1 sudo -E ./build.sh $INSTALL_PREFIX"
fi

echo ""
echo "✔  Build completata con successo!"
