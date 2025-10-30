#!/usr/bin/env bash
set -euo pipefail
# conan_publish.sh — собрать и загрузить пакет библиотеки в remote
#
# Теперь без переменных окружения: все настраивается аргументами.
#
# Usage:
#   scripts/conan_publish.sh \
#     --remote-name <name> \
#     --remote-url <url> \
#     --full-version <vX.Y.Z[-pre]> [--pkg-name <name>] [--build-type <Release|Debug>]
#
# Примеры:
#   scripts/conan_publish.sh --remote-name my --remote-url https://conan/api \
#     --full-version v1.2.3-rc.1
#
# Пояснения:
#  - --full-version  — «полная» версия (например, v1.0.0-dev.1). Будет передана в Conan как версия пакета
#                      и в CMake как SOFTWARE_VERSION. Из нее же будет выведен PROJECT_SEMVER (1.0.0) и
#                      передан в CMake через Conan conf.
#  - Имя пакета (PKG_NAME) берется из conanfile.py (conan inspect), если явно не указано через --pkg-name.
#  - В CMake пробрасываются макроопределения через Conan 2 conf:
#       -c tools.cmake.cmakedefines.SOFTWARE_VERSION=...
#       -c tools.cmake.cmakedefines.PROJECT_SEMVER=...
#

usage() {
  cat <<EOF
Usage: \$0 --remote-name <name> --remote-url <url> --full-version <vX.Y.Z[-pre]> [options]

Required:
  --remote-name NAME       Имя remote в Conan
  --remote-url URL         URL remote в Conan
  --full-version VERSION   Полная версия (например, v1.0.0-dev.1)

Optional:
  --pkg-name NAME          Имя пакета (если не указан, берется из conanfile)
  --build-type TYPE        CMake build type (Release по умолчанию)
  -h, --help               Показать справку
EOF
}

# ----------------------
# Parse args
# ----------------------
REMOTE_NAME=""
REMOTE_URL=""
PKG_NAME=""
FULL_VERSION=""
BUILD_TYPE="Release"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --remote-name)
      REMOTE_NAME=${2:?}; shift 2 ;;
    --remote-url)
      REMOTE_URL=${2:?}; shift 2 ;;
    --pkg-name)
      PKG_NAME=${2:?}; shift 2 ;;
    --full-version)
      FULL_VERSION=${2:?}; shift 2 ;;
    --build-type)
      BUILD_TYPE=${2:?}; shift 2 ;;
    -h|--help)
      usage; exit 0 ;;
    *)
      echo "[err] Unknown arg: $1" >&2; usage; exit 2 ;;
  esac
done

# Validate required
[[ -n "$REMOTE_NAME" ]] || { echo "[err] --remote-name is required" >&2; usage; exit 2; }
[[ -n "$REMOTE_URL"  ]] || { echo "[err] --remote-url is required"  >&2; usage; exit 2; }
[[ -n "$FULL_VERSION" ]] || { echo "[err] --full-version is required" >&2; usage; exit 2; }

# ----------------------
# Derive PROJECT_SEMVER from FULL_VERSION (strip leading 'v' and prerelease)
# ----------------------
derive_semver() {
  local s="$1"
  s="${s#v}"  # cut leading v
  if [[ $s =~ ^([0-9]+)\.([0-9]+)\.([0-9]+) ]]; then
    printf "%s.%s.%s" "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" "${BASH_REMATCH[3]}"
  else
    return 1
  fi
}

PROJECT_SEMVER="$(derive_semver "$FULL_VERSION")" || {
  echo "[err] --full-version must start with semantic version, got: $FULL_VERSION" >&2
  exit 2
}

# ----------------------
# Conan presence & remote setup
# ----------------------
command -v conan >/dev/null || { echo "conan not found" >&2; exit 1; }

if ! conan profile list | grep -qx "default"; then
  conan profile detect --force
fi

if conan remote list | grep -qE "^${REMOTE_NAME}\\s*:"; then
  conan remote update "${REMOTE_NAME}" --url "${REMOTE_URL}" || true
else
  conan remote add "${REMOTE_NAME}" "${REMOTE_URL}" --force
fi

# Detect PKG_NAME from conanfile.py if not provided
if [[ -z "${PKG_NAME}" ]]; then
  PKG_NAME=$(conan inspect . -a name 2>/dev/null | sed -n 's/^name: //p' | head -n1 || true)
  if [[ -z "$PKG_NAME" ]]; then
    PKG_NAME=$(conan inspect . --format=json 2>/dev/null | awk -F '"' '/"name"[[:space:]]*:/ {print $4; exit}' || true)
  fi
fi

[[ -n "$PKG_NAME" ]] || { echo "[conan_publish] Unable to detect package name. Use --pkg-name." >&2; exit 1; }

# ----------------------
# Print config
# ----------------------
echo "[conan_publish] == Config =="
echo "REMOTE_NAME:      $REMOTE_NAME"
echo "REMOTE_URL:       $REMOTE_URL"
echo "PKG_NAME:         $PKG_NAME"
echo "FULL_VERSION:     $FULL_VERSION"
echo "PROJECT_SEMVER:   $PROJECT_SEMVER"
echo "BUILD_TYPE:       $BUILD_TYPE"
echo "----------------------------------------"

# ----------------------
# Create & Upload
# ----------------------
# Передаем обе версии в CMake через Conan 2 conf, чтобы рецепт мог генерировать
# корректные config-файлы и экспортировать их дальше.
conan create . \
  --version "${FULL_VERSION}" \
  -s:h build_type="${BUILD_TYPE}"

conan upload "${PKG_NAME}/${FULL_VERSION}" -r "${REMOTE_NAME}" --confirm --check

echo "[conan_publish] Done ✓"