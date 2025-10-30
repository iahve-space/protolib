#!/usr/bin/env bash
set -euo pipefail

log()  { printf "[ci] %s\n" "$*"; }
warn() { printf "[warn] %s\n" "$*"; }
err()  { printf "[err] %s\n" "$*" >&2; }

need() { command -v "$1" >/dev/null 2>&1 || { err "Missing tool: $1"; return 1; }; }

usage() {
  cat <<'EOF'
Usage:
  build.sh [OPTIONS]

Options:
  --build-dir DIR
  --artifacts-dir DIR
  --generator GEN
  --cmake-build-type TYPE
  --extra-cmake-flags "FLAGS"
  --extra-build-flags "FLAGS"
  --project-semver X.Y.Z
  --software-version STR
  --coverage ON|OFF
  --build-tests ON|OFF
  -h, --help
EOF
}

# defaults
BUILD_DIR="build"
ARTIFACTS_DIR="artifacts"
GENERATOR=""
CMAKE_BUILD_TYPE="Release"
EXTRA_CMAKE_FLAGS=""
EXTRA_BUILD_FLAGS=""
PROJECT_SEMVER="0.0.0"
SOFTWARE_VERSION="0.0.0"
COVERAGE="OFF"
BUILD_TESTS="OFF"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --build-dir)           BUILD_DIR="$2"; shift 2 ;;
    --artifacts-dir)       ARTIFACTS_DIR="$2"; shift 2 ;;
    --generator)           GENERATOR="$2"; shift 2 ;;
    --cmake-build-type)    CMAKE_BUILD_TYPE="$2"; shift 2 ;;
    --extra-cmake-flags)   EXTRA_CMAKE_FLAGS="$2"; shift 2 ;;
    --extra-build-flags)   EXTRA_BUILD_FLAGS="$2"; shift 2 ;;
    --project-semver)      PROJECT_SEMVER="$2"; shift 2 ;;
    --software-version)    SOFTWARE_VERSION="$2"; shift 2 ;;
    --coverage)            COVERAGE="$2"; shift 2 ;;
    --build-tests)         BUILD_TESTS="$2"; shift 2 ;;
    -*)
      err "Unknown option: $1"; usage; exit 2 ;;
    *)
      err "Unexpected positional argument: $1"; usage; exit 2 ;;
  esac
done

mkdir -p "$BUILD_DIR" "$ARTIFACTS_DIR"

# summary
log "== Parsed build configuration =="
printf "%-20s %s\n" "BUILD_DIR:"        "$BUILD_DIR"
printf "%-20s %s\n" "ARTIFACTS_DIR:"    "$ARTIFACTS_DIR"
printf "%-20s %s\n" "GENERATOR:"        "${GENERATOR:-auto}"
printf "%-20s %s\n" "CMAKE_BUILD_TYPE:" "$CMAKE_BUILD_TYPE"
printf "%-20s %s\n" "PROJECT_SEMVER:"   "$PROJECT_SEMVER"
printf "%-20s %s\n" "SOFTWARE_VERSION:" "$SOFTWARE_VERSION"
printf "%-20s %s\n" "COVERAGE:"         "$COVERAGE"
printf "%-20s %s\n" "BUILD_TESTS:"      "$BUILD_TESTS"
printf "%-20s %s\n" "EXTRA_CMAKE_FLAGS:" "${EXTRA_CMAKE_FLAGS:-<none>}"
printf "%-20s %s\n" "EXTRA_BUILD_FLAGS:" "${EXTRA_BUILD_FLAGS:-<none>}"
echo "----------------------------------------"

need cmake || { err "Install cmake (apt/brew)"; exit 1; }

if [[ -z "$GENERATOR" ]]; then
  if command -v ninja >/dev/null 2>&1; then GENERATOR="Ninja"; else GENERATOR="Unix Makefiles"; fi
fi

cov_cflags="" ; cov_ldflags=""
if [[ "$COVERAGE" == "ON" ]]; then
  cov_cflags="--coverage -O0 -g"
  cov_ldflags="--coverage"
fi

log "== Build =="
log "CMake configure → $BUILD_DIR (GENERATOR=$GENERATOR, BUILD_TYPE=$CMAKE_BUILD_TYPE, TESTING=$BUILD_TESTS, COVERAGE=$COVERAGE)"

cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
  -DPROJECT_SEMVER="${PROJECT_SEMVER}" \
  -DSOFTWARE_VERSION="${SOFTWARE_VERSION}" \
  -DBUILD_TESTING="${BUILD_TESTS}" \
  -DCMAKE_C_FLAGS="${CMAKE_C_FLAGS:-} ${cov_cflags}" \
  -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS:-} ${cov_cflags}" \
  -DCMAKE_EXE_LINKER_FLAGS="${CMAKE_EXE_LINKER_FLAGS:-} ${cov_ldflags}" \
  -DCMAKE_SHARED_LINKER_FLAGS="${CMAKE_SHARED_LINKER_FLAGS:-} ${cov_ldflags}" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  ${EXTRA_CMAKE_FLAGS}

jobs="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"


log "CMake build (jobs=$jobs)"
cmake --build "$BUILD_DIR" -- -j"$jobs" ${EXTRA_BUILD_FLAGS}
BUILD_STATUS=$?
if [[ $BUILD_STATUS -ne 0 ]]; then
  err "Build failed with code $BUILD_STATUS"
  exit $BUILD_STATUS
fi

if [[ -f "$BUILD_DIR/compile_commands.json" ]]; then
  cp -f "$BUILD_DIR/compile_commands.json" "$ARTIFACTS_DIR/compile_commands.json" || true
fi

log "Build done ✓"