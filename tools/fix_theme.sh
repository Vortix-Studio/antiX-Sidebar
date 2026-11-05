#!/bin/bash
# Pulisce un file CSS per GTK3: rimuove BOM, CRLF, caratteri non-ASCII

set -eu

INPUT="${1:-}"
OUTPUT="${2:-$INPUT}"

if [ -z "$INPUT" ]; then
    echo "Uso: $0 <input.css> [output.css]"
    exit 1
fi

if [ ! -f "$INPUT" ]; then
    echo "Errore: file '$INPUT' non trovato"
    exit 1
fi

# Rimuove BOM UTF-8 (EF BB BF), converte CRLF -> LF, filtra ASCII puro
sed '1s/^\xef\xbb\xbf//' \
 | tr -d '\r' \
 | sed 's/\xe2\x80\x94/-/g; s/\xe2\x80\x93/-/g; s/\xe2\x80\x99/'\''/g; s/\xe2\x80\x9c/"/g; s/\xe2\x80\x9d/"/g; s/\xc2\xa0/ /g' \
 | LC_ALL=C tr -cd '\011\012\015\040-\176' \
 > "$OUTPUT"

echo "CSS sanificato scritto in: $OUTPUT"
