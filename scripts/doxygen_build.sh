#!/usr/bin/env bash
set -euo pipefail
# doxygen_build.sh — сборка HTML-документации
# Usage: doxygen_build.sh --doxyfile docs/Doxyfile --out public
# Результат — в каталоге --out

DOXYFILE=""
OUT_DIR="public"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --doxyfile) DOXYFILE="$2"; shift 2;;
    --out) OUT_DIR="$2"; shift 2;;
    *) echo "Unknown arg: $1" >&2; exit 2;;
  esac
done
[[ -n "$DOXYFILE" ]] || { echo "--doxyfile is required" >&2; exit 2; }
command -v doxygen >/dev/null || { echo "doxygen not found" >&2; exit 1; }

mkdir -p "$OUT_DIR"
doxygen -v

# Пытаемся направить вывод в OUT_DIR: если в Doxyfile уже есть OUTPUT_DIRECTORY — подменим на лету.
if grep -qE '^\\s*OUTPUT_DIRECTORY\\s*=' "$DOXYFILE"; then
  tmp=$(mktemp)
  sed -E "s#^(\\s*OUTPUT_DIRECTORY\\s*=).*$#\\1 $OUT_DIR#" "$DOXYFILE" > "$tmp"
  doxygen "$tmp"
  rm -f "$tmp"
else
  # Иначе надеемся на RELATIVE_PATHS/GENERATE_HTML_DIR и т.д.
  doxygen "$DOXYFILE"
fi

[[ -d "$OUT_DIR" ]] || { echo "Doxygen did not produce $OUT_DIR" >&2; exit 3; }