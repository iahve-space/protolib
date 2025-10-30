#!/usr/bin/env bash
set -euo pipefail

OUT_FILE="${1:-}"
if [[ "${OUT_FILE:-}" == "--out" ]]; then
  OUT_FILE="${2:-version.env}"
else
  # поддержка: scripts/compute_version.sh --out version.env
  if [[ "${1:-}" == "--out" && -n "${2:-}" ]]; then
    OUT_FILE="${2}"
  else
    OUT_FILE="artifacts/version.env"
  fi
fi

echo "[compute_version] node/npm/husky and npm install (lenient peers)"

# 1) Инфо об окружении
if command -v node >/dev/null 2>&1; then node -v; else echo "[warn] node not found"; fi
if command -v npm  >/dev/null 2>&1; then npm -v;  else echo "[warn] npm  not found"; fi

# 2) Мягкая установка зависимостей
export HUSKY=0
export NPM_CONFIG_FUND=false
export NPM_CONFIG_AUDIT=false
export NPM_CONFIG_LEGACY_PEER_DEPS=true

# Локальный кеш npm (чтобы избежать прав и ускорить)
mkdir -p .npm
export npm_config_cache="$PWD/.npm"

if [[ -f package-lock.json ]]; then
  npm ci --no-audit --prefer-offline || npm ci --legacy-peer-deps --no-audit --prefer-offline
else
  npm i  --no-audit --no-fund --prefer-offline || npm i --legacy-peer-deps --no-audit --no-fund --prefer-offline
fi

# 3) semantic-release (dry-run)
echo "[compute_version] semantic-release dry-run"
# В GitLab/GitHub можно работать без @github/@gitlab плагинов, если конфиг «semantic-version-only»
npx semantic-release --dry-run --no-ci --no-color 2>&1 | tee sr.log || true

# 4) Извлекаем версию
# Примеры строк:
#  - "The next release version is 1.2.3"
#  - "next release version is 1.2.3-alpha.1"
VERSION="$(grep -iPo 'next release version is\s+\K[0-9]+(\.[0-9]+){2}([-.][0-9A-Za-z.]+)?' sr.log | head -n1 || true)"

if [[ -n "$VERSION" ]]; then
  echo "SOFTWARE_VERSION=$VERSION" > "$OUT_FILE"
  echo "[compute_version] SOFTWARE_VERSION=$VERSION → $OUT_FILE"
else
  # Фоллбек, если нет релиза — используем дефолт из CI переменных (если есть)
  DEF="${DEFAULT_APP_VERSION:-0.0.0-${CI_COMMIT_REF_SLUG:-local}.${CI_COMMIT_SHORT_SHA:-dev}}"
  echo "SOFTWARE_VERSION=$DEF" > "$OUT_FILE"
  echo "[compute_version] No next release; fallback SOFTWARE_VERSION=$DEF → $OUT_FILE"
fi