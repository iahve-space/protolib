#!/usr/bin/env bash
set -euo pipefail
# docs_archive.sh — упаковать сайт документации в zip-артефакт
# Usage: docs_archive.sh --in public --zip artifacts/documentation.zip

SRC_DIR="public"
ZIP_PATH="artifacts/documentation.zip"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --in)  SRC_DIR="$2"; shift 2;;
    --zip) ZIP_PATH="$2"; shift 2;;
    *) echo "Unknown arg: $1" >&2; exit 2;;
  esac
done

[[ -d "$SRC_DIR" ]] || { echo "docs folder '$SRC_DIR' not found" >&2; exit 1; }
mkdir -p "$(dirname "$ZIP_PATH")"
zip -r "$ZIP_PATH" "$SRC_DIR" >/dev/null
files=$(find "$SRC_DIR" -type f | wc -l | tr -d ' ')
echo "Archived $files files to $ZIP_PATH"