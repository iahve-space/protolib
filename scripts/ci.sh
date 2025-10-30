#!/usr/bin/env bash
set -euo pipefail
set -o errtrace
trap 'err "Failed at line $LINENO: $BASH_COMMAND"' ERR

log()  { printf "[ci] %s\n" "$*"; }
warn() { printf "[warn] %s\n" "$*"; }
err()  { printf "[err] %s\n" "$*" >&2; }

usage() {
  cat <<'EOF'
Usage:
  ci.sh [STAGES] [OPTIONS]

Stages (choose any; if none given, acts like --all):
  --all            Run build + test + coverage + tidy (coverage instrumentation is forced ON)
  --build          Run build stage
  --test           Run test stage (enables BUILD_TESTING in build step)
  --coverage       Run coverage stage
  --tidy           Run clang-tidy stage
  -h, --help       Show this help and exit

Options (forwarded to sub-scripts, including build.sh):
  --build-dir DIR
  --artifacts-dir DIR
  --generator GEN
  --cmake-build-type TYPE
  --extra-cmake-flags "FLAGS"
  --extra-build-flags "FLAGS"
  --project-semver X.Y.Z
  --software-version STR
  --coverage ON|OFF
  Note: If you run the --coverage stage, coverage instrumentation is forced ON even if "--coverage OFF" was given.
EOF
}

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DO_BUILD=0
DO_TEST=0
DO_COVERAGE=0
DO_TIDY=0
COVERAGE_EXPLICIT=""
ARGS_COMMON=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --all)
      DO_BUILD=1; DO_TEST=1; DO_COVERAGE=1; DO_TIDY=1
      ARGS_COMMON+=("--coverage" "ON")
      shift ;;
    --build) DO_BUILD=1; shift ;;
    --test)
      # Stage: test → must build with tests and coverage ON, then run tests
      DO_TEST=1
      DO_BUILD=1
      shift ;;
    --coverage)
      # Stage: coverage → build with tests+coverage, do NOT run tests; only run coverage.sh
      if [[ "${2:-}" =~ ^(ON|OFF)$ ]]; then
        ARGS_COMMON+=("--coverage" "$2")
        COVERAGE_EXPLICIT="$2"
        shift 2
      else
        shift
      fi
      DO_COVERAGE=1
      DO_BUILD=1
      ;;
    --tidy) DO_TIDY=1; shift ;;
    -h|--help) usage; exit 0 ;;
    --build-dir|--artifacts-dir|--generator|--cmake-build-type|\
    --extra-cmake-flags|--extra-build-flags|--project-semver|--software-version)
      [[ $# -ge 2 ]] || { err "Missing value for $1"; exit 2; }
      ARGS_COMMON+=("$1" "$2"); shift 2 ;;
    -*)
      err "Unknown option: $1"; usage; exit 2 ;;
    *)
      err "Unexpected positional argument: $1"; usage; exit 2 ;;
  esac
done


# defaults: if no stages specified
if [[ $DO_BUILD -eq 0 && $DO_TEST -eq 0 && $DO_COVERAGE -eq 0 && $DO_TIDY -eq 0 ]]; then
  DO_BUILD=1; DO_TEST=1; DO_COVERAGE=1
  ARGS_COMMON+=("--coverage" "ON")
fi

# ensure build configuration consistency
# if tests, coverage, or tidy requested, force build to include them
if [[ ${DO_TEST:-0} -eq 1 || ${DO_COVERAGE:-0} -eq 1 || ${DO_TIDY:-0} -eq 1 ]]; then
  DO_BUILD=1
fi

# print summary
log "== Parsed configuration =="
printf "%-20s %s\n" "DO_BUILD:"     "$DO_BUILD"
printf "%-20s %s\n" "DO_TEST:"      "$DO_TEST"
printf "%-20s %s\n" "DO_COVERAGE:"  "$DO_COVERAGE"
printf "%-20s %s\n" "DO_TIDY:"      "$DO_TIDY"
printf "%-20s %s\n" "Forwarded args:" "${ARGS_COMMON[*]:-(none)}"
echo "----------------------------------------"

run() { "$@" "${ARGS_COMMON[@]}"; }

if [[ $DO_BUILD -eq 1 ]]; then
  # Sanity check for build script
  if [[ ! -f "$HERE/build.sh" ]]; then
    err "build.sh is missing at: $HERE/build.sh"
    exit 127
  fi
  if [[ ! -x "$HERE/build.sh" ]]; then
    warn "build.sh is not executable → trying chmod +x"
    chmod +x "$HERE/build.sh" || true
  fi
  if [[ ! -x "$HERE/build.sh" ]]; then
    err "build.sh is not executable at: $HERE/build.sh"
    exit 126
  fi

  log "== Build stage =="
  # Resolve BUILD_TESTS flag
  BUILD_TESTS="OFF"
  # Enable tests in the build if any of test/coverage/tidy stages are requested
  if [[ $DO_TEST -eq 1 || $DO_COVERAGE -eq 1 ]]; then
    BUILD_TESTS="ON"
  fi

  # Start from currently collected common args
  BUILD_ARGS=( "${ARGS_COMMON[@]}" )

  # Normalize/resolve coverage value for the build step by STAGE RULES
  #  - If test OR coverage OR tidy stage requested → COVERAGE=ON (tests & analyzers need instrumentation)
  #  - Else if explicit COVERAGE_EXPLICIT provided → use it (ON/OFF)
  #  - Else COVERAGE=OFF
  RESOLVED_COV="OFF"
  if [[ $DO_TEST -eq 1 || $DO_COVERAGE -eq 1  ]]; then
    RESOLVED_COV="ON"
  elif [[ -n "$COVERAGE_EXPLICIT" ]]; then
    RESOLVED_COV="$COVERAGE_EXPLICIT"
  fi

  # Strip any existing "--coverage <val>" from BUILD_ARGS to avoid duplicates
  tmp_args=()
  i=0
  while [[ $i -lt ${#BUILD_ARGS[@]} ]]; do
    if [[ "${BUILD_ARGS[$i]}" == "--coverage" ]]; then
      # skip this pair
      i=$((i+2))
      continue
    fi
    tmp_args+=("${BUILD_ARGS[$i]}")
    i=$((i+1))
  done
  BUILD_ARGS=("${tmp_args[@]}")

  # Append normalized coverage and build-tests flags
  BUILD_ARGS+=( "--coverage" "$RESOLVED_COV" "--build-tests" "$BUILD_TESTS" )

  # Helpful debug: show exactly what will be forwarded to build.sh
  log "[build] forwarding args → ${BUILD_ARGS[*]}"
  if [[ ${#BUILD_ARGS[@]} -eq 0 ]]; then
    log "[build] forwarding args → (none)"
  fi
  "$HERE/build.sh" "${BUILD_ARGS[@]}" || exit $?
fi

if [[ $DO_TEST -eq 1 && -x "$HERE/test.sh" ]]; then
  run "$HERE/test.sh" || exit $?
fi

if [[ $DO_COVERAGE -eq 1 && -x "$HERE/coverage.sh" ]]; then
  run "$HERE/coverage.sh" || exit $?
fi

if [[ $DO_TIDY -eq 1 && -x "$HERE/clang_tidy.sh" ]]; then
  run "$HERE/clang_tidy.sh" || exit $?
fi

exit 0