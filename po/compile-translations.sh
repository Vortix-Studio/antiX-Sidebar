#!/usr/bin/env bash
set -euo pipefail

# Nome del dominio gettext: deve corrispondere a quello usato nell'app.
DOMAIN="antix-sidebar"

# Cartella di output predefinita; puoi passarne un'altra come primo argomento.
OUTPUT_DIR="${1:-build/locale}"

LANGUAGES=(
    de es fr it ja nl pl pt ru uk
)

if ! command -v msgfmt >/dev/null 2>&1; then
    echo "Errore: msgfmt non trovato. Installa GNU gettext." >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

for lang in "${LANGUAGES[@]}"; do
    po_file="${lang}.po"
    mo_dir="${OUTPUT_DIR}/${lang}/LC_MESSAGES"
    mo_file="${mo_dir}/${DOMAIN}.mo"

    if [[ ! -f "$po_file" ]]; then
        echo "Errore: file non trovato: $po_file" >&2
        exit 1
    fi

    mkdir -p "$mo_dir"
    msgfmt --check --output-file="$mo_file" "$po_file"
    echo "Compilato: $po_file -> $mo_file"
done

echo "Traduzioni compilate in: $OUTPUT_DIR"
