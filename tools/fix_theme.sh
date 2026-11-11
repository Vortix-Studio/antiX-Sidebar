#!/bin/bash
# Clean a CSS file for GTK3: remove BOM, CRLF, and non-ASCII characters

set -eu

INPUT="${1:-}"
OUTPUT="${2:-$INPUT}"

if [ -z "$INPUT" ]; then
    echo "Usage: $0 <input.css> [output.css]"
    exit 1
fi

if [ ! -f "$INPUT" ]; then
    echo "Error: file '$INPUT' not found"
    exit 1
fi

sed '1s/^\xef\xbb\xbf//' "$INPUT" \
 | tr -d '\r' \
 | sed 's/\xe2\x80\x94/-/g; s/\xe2\x80\x93/-/g; s/\xe2\x80\x99/'\''/g; s/\xe2\x80\x9c/"/g; s/\xe2\x80\x9d/"/g; s/\xc2\xa0/ /g' \
 | LC_ALL=C tr -cd '\011\012\015\040-\176' \
 > "$OUTPUT"

echo "Sanitized CSS written to: $OUTPUT"
