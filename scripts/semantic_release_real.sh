#!/usr/bin/env bash
set -euo pipefail
# semantic_release_real.sh — настоящий релиз через semantic-release.
# Требует настроенные токены (например, GITLAB_TOKEN).

command -v npm >/dev/null || { echo "npm not found" >&2; exit 1; }
if [[ -f package-lock.json ]]; then
  npm ci --no-audit --no-fund --prefer-offline
else
  npm i  --no-audit --no-fund --prefer-offline
fi

npx semantic-release --no-ci