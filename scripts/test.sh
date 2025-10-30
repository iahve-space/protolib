#!/usr/bin/env bash
set -euo pipefail

log()  { printf "[ci] %s\n" "$*"; }
warn() { printf "[warn] %s\n" "$*"; }
err()  { printf "[err] %s\n" "$*" >&2; }

need() { command -v "$1" >/dev/null 2>&1 || { err "Missing tool: $1"; return 1; }; }

usage() {
  cat <<'EOF'
Usage:
  test.sh [OPTIONS]

Options:
  --build-dir DIR          Path to CMake build directory (default: build)
  --artifacts-dir DIR      Path to artifacts directory (default: artifacts)
  --extra-build-flags "…"  Extra flags to pass to ctest (optional)
  -h, --help               Show this help and exit

Behavior:
  • Searches for tests in the given build directory using CTest.
  • If no tests found, recursively searches all subdirectories for CTestTestfile.cmake.
  • Generates JUnit XML reports (→ artifacts/junit*.xml).
  • Exits non-zero if any test suite fails.
EOF
}

# defaults
BUILD_DIR="build"
ARTIFACTS_DIR="artifacts"
EXTRA_BUILD_FLAGS=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --build-dir)      BUILD_DIR="$2"; shift 2 ;;
    --artifacts-dir)  ARTIFACTS_DIR="$2"; shift 2 ;;
    --extra-build-flags) EXTRA_BUILD_FLAGS="$2"; shift 2 ;;
    -*)
      warn "Unknown option: $1"; shift 2 ;;
    *)
      err "Unexpected positional argument: $1"; usage; exit 2 ;;
  esac
done

mkdir -p "$ARTIFACTS_DIR"
# Use absolute path for JUnit outputs since ctest resolves paths relative to --test-dir
ARTIFACTS_DIR_ABS="$(cd "$ARTIFACTS_DIR" && pwd)"

# summary
log "== Parsed test configuration =="
printf "%-20s %s\n" "BUILD_DIR:"       "$BUILD_DIR"
printf "%-20s %s\n" "ARTIFACTS_DIR:"   "$ARTIFACTS_DIR"
printf "%-20s %s\n" "ARTIFACTS_DIR_ABS:" "$ARTIFACTS_DIR_ABS"
printf "%-20s %s\n" "EXTRA_BUILD_FLAGS:" "${EXTRA_BUILD_FLAGS:-<none>}"
echo "----------------------------------------"

need ctest || { err "Install ctest (from cmake package)"; exit 1; }

# helper to count tests
count_tests() {
  local dir="$1"
  local total
  total=$(ctest -N --test-dir "$dir" 2>/dev/null | awk '/Total Tests:/ {print $3; exit}')
  echo "${total:-0}"
}

TOP_TOTAL=$(count_tests "$BUILD_DIR")
if [[ "$TOP_TOTAL" =~ ^[0-9]+$ ]] && [[ "$TOP_TOTAL" -gt 0 ]]; then
  log "Found $TOP_TOTAL tests in $BUILD_DIR → running"
  ctest --test-dir "$BUILD_DIR" \
    --output-on-failure --no-tests=error \
    -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)" \
    --output-junit "$ARTIFACTS_DIR_ABS/junit.xml" ${EXTRA_BUILD_FLAGS}
  log "Tests completed ✓"
  exit 0
fi

warn "No tests found at $BUILD_DIR. Searching subdirectories for CTestTestfile.cmake…"
mapfile -t CTEST_DIRS < <(find "$BUILD_DIR" -type f -name CTestTestfile.cmake -exec dirname {} \; | sort -u)

if [[ ${#CTEST_DIRS[@]} -eq 0 ]]; then
  err "No CTestTestfile.cmake files found under $BUILD_DIR."
  exit 1
fi

RAN_ANY=0
FAILED_ANY=0
IDX=0
for d in "${CTEST_DIRS[@]}"; do
  TOTAL=$(count_tests "$d")
  if [[ "$TOTAL" =~ ^[0-9]+$ ]] && [[ "$TOTAL" -gt 0 ]]; then
    ((IDX++)) || true
    local_junit="$ARTIFACTS_DIR_ABS/junit-${IDX}.xml"
    log "Running $TOTAL tests in $d"
    if ! ctest --test-dir "$d" \
      --output-on-failure --no-tests=error \
      -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)" \
      --output-junit "$local_junit" ${EXTRA_BUILD_FLAGS}; then
      warn "Tests failed in $d"
      FAILED_ANY=1
    fi
    RAN_ANY=1
  else
    warn "No tests in $d (Total=$TOTAL) — skip"
  fi
done

if [[ "$RAN_ANY" -eq 0 ]]; then
  err "CTest files found, but no runnable tests."
  exit 2
fi

if [[ "$FAILED_ANY" -ne 0 ]]; then
  err "Some test suites failed. See reports in $ARTIFACTS_DIR"
  exit 1
fi

log "All tests passed ✓"