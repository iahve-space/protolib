#!/usr/bin/env bash
set -euo pipefail

log()  { printf "[ci] %s\n" "$*"; }
warn() { printf "[warn] %s\n" "$*"; }
err()  { printf "[err] %s\n" "$*" >&2; }

usage() {
  cat <<'EOF'
Usage:
  coverage.sh [OPTIONS]

Options:
  --build-dir DIR          Path to the CMake build directory (default: build)
  --artifacts-dir DIR      Where to place reports (default: artifacts)
  --root-dir DIR           Project root for relative paths (default: git root or .)
  --exclude REGEX          Extra exclude regex for gcovr (can be repeated)
  -h, --help               Show this help and exit

Behavior:
  • If lcov is available, captures *.gcda/*.gcno from the build dir into artifacts/coverage.info.
  • Always runs gcovr to produce:
      - artifacts/coverage.xml   (Cobertura)
      - artifacts/coverage.html  (HTML summary + details)
      - artifacts/coverage.txt   (text summary)
      - artifacts/coverage.svg   (badge), if процент извлечён
  • Excludes common 3rd-party and build-generated paths.

Notes / Tips:
  • Нужна сборка с покрытием:
      - GCC/Clang (GNU):  -O0 -g --coverage
      - Clang/LLVM:       -O0 -g -fprofile-instr-generate -fcoverage-mapping
    Наш build.sh при флаге COVERAGE=ON уже добавляет нужные флаги под GCC/Clang (GNU).
  • Если используешь Clang/LLVM и видишь странные цифры, можно явно указать:
      gcovr ... --gcov-executable "llvm-cov gcov"
  • Этот скрипт автоматически найдёт `llvm-cov gcov` (если установлен) или обычный `gcov`.
EOF
}

# defaults
BUILD_DIR="build"
ARTIFACTS_DIR="artifacts"
ROOT_DIR=""
EXCLUDES=()

# parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --build-dir)        BUILD_DIR="$2"; shift 2 ;;
    --artifacts-dir)    ARTIFACTS_DIR="$2"; shift 2 ;;
    --root-dir)         ROOT_DIR="$2"; shift 2 ;;
    --exclude)          EXCLUDES+=("$2"); shift 2 ;;
    -*)
      warn "Unknown option: $1"; shift 2 ;;
    *)
      err "Unexpected positional argument: $1"; usage; exit 2 ;;
  esac
done

# detect root if not provided
if [[ -z "$ROOT_DIR" ]]; then
  if git_root="$(git rev-parse --show-toplevel 2>/dev/null || true)"; then
    ROOT_DIR="${git_root:-.}"
  else
    ROOT_DIR="."
  fi
fi

# normalize ROOT_DIR to absolute path
ROOT_DIR="$(cd "$ROOT_DIR" && pwd -P)"

mkdir -p "$ARTIFACTS_DIR"

# detect compiler used by the build (from CMakeCache.txt)
COMPILER_ID=""
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  COMPILER_ID=$(sed -n 's/^CMAKE_CXX_COMPILER_ID:STRING=\(.*\)$/\1/p' "$BUILD_DIR/CMakeCache.txt" | head -n1)
fi
if [[ -z "$COMPILER_ID" && -f "$BUILD_DIR/CMakeFiles/TargetDirectories.txt" ]]; then
  : # placeholder for future heuristics
fi
if [[ -n "$COMPILER_ID" ]]; then
  log "Detected compiler: $COMPILER_ID"
fi

# choose gcov executable based on compiler (prefer matching tool)
GCOV_BIN=""
case "$COMPILER_ID" in
  GNU)
    if command -v gcov >/dev/null 2>&1; then
      GCOV_BIN="gcov"
    elif command -v llvm-cov >/dev/null 2>&1 && llvm-cov gcov --version >/dev/null 2>&1; then
      GCOV_BIN="llvm-cov gcov"
    fi
    ;;
  Clang|AppleClang)
    if command -v llvm-cov >/dev/null 2>&1 && llvm-cov gcov --version >/dev/null 2>&1; then
      GCOV_BIN="llvm-cov gcov"
    elif command -v gcov >/dev/null 2>&1; then
      GCOV_BIN="gcov"
    fi
    ;;
  *)
    # fallback: try llvm-cov gcov then gcov
    if command -v llvm-cov >/dev/null 2>&1 && llvm-cov gcov --version >/dev/null 2>&1; then
      GCOV_BIN="llvm-cov gcov"
    elif command -v gcov >/dev/null 2>&1; then
      GCOV_BIN="gcov"
    fi
    ;;
esac

if [[ -n "$GCOV_BIN" ]]; then
  log "gcov executable: $GCOV_BIN"
else
  warn "No gcov tool found (gcov/llvm-cov gcov). Reports may be empty."
fi

# macOS tip: prefer llvm-cov gcov for AppleClang if available
if [[ "$GCOV_BIN" == "gcov" && "$(uname -s 2>/dev/null)" == "Darwin" ]]; then
  if xcrun -f llvm-cov >/dev/null 2>&1; then
    warn "Using system gcov. Consider: brew install llvm && use 'llvm-cov gcov' for cleaner reports."
  fi
fi

# summary
log "== Parsed coverage configuration =="
printf "%-20s %s\n" "BUILD_DIR:"       "$BUILD_DIR"
printf "%-20s %s\n" "ARTIFACTS_DIR:"   "$ARTIFACTS_DIR"
printf "%-20s %s\n" "ROOT_DIR:"        "$ROOT_DIR"
if [[ ${#EXCLUDES[@]} -gt 0 ]]; then
  printf "%-20s %s\n" "EXCLUDES:"      "${EXCLUDES[*]}"
else
  printf "%-20s %s\n" "EXCLUDES:"      "<none>"
fi
echo "----------------------------------------"

# tools check
command -v gcovr >/dev/null 2>&1 || { err "gcovr is required"; echo "→ install: pip install gcovr"; exit 1; }
lcov_ok=0
if command -v lcov >/dev/null 2>&1; then
  lcov_ver="$(lcov --version 2>/dev/null || true)"
  log "lcov: $lcov_ver"
  lcov_ok=1
fi

# ---- lcov (optional) ----
if [[ "$lcov_ok" -eq 1 ]]; then
  log "lcov capture…"
  (
    cd "$BUILD_DIR"
    # базовый capture с игнорированием известных несоответствий строк (mismatch) и нормализацией "неисполняемых" блоков
    if ! lcov --capture --directory . \
        --output-file coverage.info \
        --ignore-errors mismatch \
        --rc geninfo_unexecuted_blocks=1; then
      warn "lcov capture failed, fallback to gcovr only"
      lcov_ok=0
    else
      # очистим шум системных и сторонних путей
      lcov --remove coverage.info '/usr/*' '*/_deps/*' '*/CMakeFiles/*' --output-file coverage.info || true
      lcov --list coverage.info || true
      cp -f coverage.info "../$ARTIFACTS_DIR/coverage.info" || true
    fi
  )
fi

# ---- gcovr ----
log "gcovr reports…"

# exclude individual files by regex (relative to --root)
GCOVR_EXCLUDE_ARGS=(
  --exclude '.*_deps/.*'
  --exclude '.*/CMakeFiles/.*'
  --exclude '.*/CompilerId[A-Za-z].*'
  --exclude '.*/CMakeFiles/[0-9.]+/CompilerId.*/.*'
  --exclude '.*/tests?/.*'
)
# exclude whole directories that are definitely external vendored code
# NOTE: do NOT exclude CMakeFiles here — gcovr scans object files under it.
GCOVR_EXCLUDE_DIR_ARGS=(
  --exclude-directories '.*/_deps/.*'
)
# пользовательские exclude’ы
for rx in "${EXCLUDES[@]}"; do
  GCOVR_EXCLUDE_ARGS+=( --exclude "$rx" )
done

# Base args: root + object-directory are important to avoid gcovr WD errors
GCOVR_BASE_ARGS=(
  --root "$ROOT_DIR"
  --object-directory "$BUILD_DIR"
  --filter "${ROOT_DIR%/}/.*"
)

# Append gcov executable if detected
if [[ -n "$GCOV_BIN" ]]; then
  GCOVR_BASE_ARGS+=( --gcov-executable "$GCOV_BIN" )
fi

# Be lenient with common GCOV quirks (e.g., missing CompilerId sources)
GCOVR_IGNORE_ARGS=(
  --gcov-ignore-errors no_working_dir_found
  --gcov-ignore-errors source_not_found
  --gcov-ignore-parse-errors all
)

# Cobertura/XML
gcovr "${GCOVR_BASE_ARGS[@]}" "${GCOVR_EXCLUDE_ARGS[@]}" "${GCOVR_EXCLUDE_DIR_ARGS[@]}" "${GCOVR_IGNORE_ARGS[@]}" \
  --xml-pretty --cobertura-pretty -o "$ARTIFACTS_DIR/coverage.xml" || true

# HTML + details
gcovr "${GCOVR_BASE_ARGS[@]}" "${GCOVR_EXCLUDE_ARGS[@]}" "${GCOVR_EXCLUDE_DIR_ARGS[@]}" "${GCOVR_IGNORE_ARGS[@]}" \
  --html --html-details -o "$ARTIFACTS_DIR/coverage.html" || true

# Text summary
gcovr "${GCOVR_BASE_ARGS[@]}" "${GCOVR_EXCLUDE_ARGS[@]}" "${GCOVR_EXCLUDE_DIR_ARGS[@]}" "${GCOVR_IGNORE_ARGS[@]}" \
  --txt --print-summary | tee "$ARTIFACTS_DIR/coverage.txt" || true

# Diagnose zero-coverage situations (typical when nothing was executed)
GCDA_COUNT="$(find "$BUILD_DIR" -type f -name '*.gcda' | wc -l | tr -d '[:space:]')"
if [[ "${GCDA_COUNT:-0}" -eq 0 ]]; then
  warn "No .gcda files found in $BUILD_DIR — built with COVERAGE=ON but nothing was executed. Reports will show 0%."
  warn "Tip: run tests in a previous job (with the same build dir) and pass the build/ as an artifact, then run coverage collection only."
fi

# Badge (извлекаем %)
PCT=""
if json_out="$(gcovr "${GCOVR_BASE_ARGS[@]}" "${GCOVR_EXCLUDE_ARGS[@]}" "${GCOVR_EXCLUDE_DIR_ARGS[@]}" "${GCOVR_IGNORE_ARGS[@]}" --json -o - 2>/dev/null || true)"; [[ -n "$json_out" ]]; then
  if _num="$(printf '%s' "$json_out" | sed -n 's/.*"line_coverage":[[:space:]]*\([0-9.][0-9.]*\).*/\1/p' | head -1)"; [[ -n "$_num" ]]; then
    PCT="$(awk -v n="$_num" 'BEGIN{printf("%.1f", n*100)}')"
  fi
fi
if [[ -z "$PCT" && -f "$ARTIFACTS_DIR/coverage.txt" ]]; then
  _line_pct="$(grep -Eo 'lines:[[:space:]]*[0-9.]+%' "$ARTIFACTS_DIR/coverage.txt" | head -1 | sed -E 's/.*lines:[[:space:]]*([0-9.]+)%.*/\1/')"
  [[ -n "$_line_pct" ]] && PCT="$_line_pct"
fi

if [[ -n "$PCT" ]]; then
  if [[ "$PCT" == "0.0" ]]; then
    warn "Computed coverage is 0.0% — likely because tests were not executed in this job."
  fi
  log "[badge] coverage ${PCT}%"
  mkdir -p badges
  printf '%s\n' "<svg xmlns='http://www.w3.org/2000/svg' width='140' height='20'><linearGradient id='b' x2='0' y2='100%'><stop offset='0' stop-color='#bbb' stop-opacity='.1'/><stop offset='1' stop-opacity='.1'/></linearGradient><mask id='a'><rect width='140' height='20' rx='3' fill='#fff'/></mask><g mask='url(#a)'><rect width='70' height='20' fill='#555'/><rect x='70' width='70' height='20' fill='#fe7d37'/><rect width='140' height='20' fill='url(#b)'/></g><g fill='#fff' text-anchor='middle' font-family='DejaVu Sans,Verdana,Geneva,sans-serif' font-size='11'><text x='35' y='15'>coverage</text><text x='105' y='15'>${PCT}%</text></g></svg>" > badges/coverage.svg
  cp -f badges/coverage.svg "$ARTIFACTS_DIR/coverage.svg" || true
fi

log "Coverage done ✓  (reports → $ARTIFACTS_DIR)"