#!/usr/bin/env bash
set -euo pipefail

# ──────────────────────────────────────────────────────────────────────────────
# Defaults (можно переопределить флагами)
BUILD_DIR="build_x86"
CMAKE_BUILD_TYPE="Release"
CMAKE_GENERATOR="Ninja"        # или "Unix Makefiles"

RUN_BUILD=false
RUN_TESTS=false
RUN_COVERAGE=false
MAKE_BADGE=false

EXTRA_CMAKE_FLAGS=""
EXTRA_BUILD_FLAGS=""

# Версии/метаданные (GitHub/GitLab/локально)
GIT_BRANCH="${CI_COMMIT_BRANCH:-${GITHUB_REF_NAME:-$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)}}"
GIT_SHA="${CI_COMMIT_SHA:-${GITHUB_SHA:-$(git rev-parse --short HEAD 2>/dev/null || echo 0000000)}}"
PROJECT_SEMVER="${PROJECT_SEMVER:-0.0.0}"
SOFTWARE_VERSION="${SOFTWARE_VERSION:-0.0.0-${GIT_BRANCH}.${GIT_SHA}}"

# Эти переменные вычислим по флагам после парсинга
BUILD_TESTING_CMAKE="OFF"
SOFTWARE_COVERAGE_CMAKE="OFF"

# ──────────────────────────────────────────────────────────────────────────────
usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Tasks:
  --build                 configure + build
  --test                  run ctest (и включает BUILD_TESTING=ON)
  --coverage              collect coverage (lcov/gcovr), компилирует с --coverage
  --badge                 generate coverage badge (при наличии --coverage)
  --all                   build + test + coverage + badge

Locations / flags:
  --build-dir DIR         build directory (default: ${BUILD_DIR})
  --cmake-type TYPE       CMAKE_BUILD_TYPE (default: ${CMAKE_BUILD_TYPE})
  --generator NAME        CMake generator (default: ${CMAKE_GENERATOR})
  --cmake-flags "..."     extra CMake flags
  --build-flags "..."     extra native build flags

Versioning (optional env):
  PROJECT_SEMVER=1.2.3  SOFTWARE_VERSION=1.2.3-rc.1

Examples:
  ./scripts/ci.sh --build --test --coverage --build-dir out/coverage --cmake-type Debug
  ./scripts/ci.sh --build --build-dir out/release --cmake-type Release
EOF
}

# ──────────────────────────────────────────────────────────────────────────────
# Parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build) RUN_BUILD=true ;;
    --test) RUN_TESTS=true ;;
    --coverage) RUN_COVERAGE=true ;;
    --badge) MAKE_BADGE=true ;;
    --all) RUN_BUILD=true; RUN_TESTS=true; RUN_COVERAGE=true; MAKE_BADGE=true ;;
    --build-dir) BUILD_DIR="$2"; shift ;;
    --cmake-type) CMAKE_BUILD_TYPE="$2"; shift ;;
    --generator) CMAKE_GENERATOR="$2"; shift ;;
    --cmake-flags) EXTRA_CMAKE_FLAGS="$2"; shift ;;
    --build-flags) EXTRA_BUILD_FLAGS="$2"; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown arg: $1"; usage; exit 1 ;;
  esac
  shift
done

# Привязка флагов к CMake-переменным
[[ "$RUN_TESTS" == true ]]    && BUILD_TESTING_CMAKE="ON"     || BUILD_TESTING_CMAKE="OFF"
[[ "$RUN_COVERAGE" == true ]] && SOFTWARE_COVERAGE_CMAKE="ON" || SOFTWARE_COVERAGE_CMAKE="OFF"

# ──────────────────────────────────────────────────────────────────────────────
# Helpers
log()  { printf "\033[1;34m[ci]\033[0m %s\n" "$*"; }
warn() { printf "\033[1;33m[warn]\033[0m %s\n" "$*" >&2; }
err()  { printf "\033[1;31m[err]\033[0m %s\n" "$*" >&2; exit 1; }

need() {
  if ! command -v "$1" >/dev/null 2>&1; then
    warn "'$1' not found."
    case "$1" in
      cmake)  echo "→ Install: cmake";;
      ninja)  echo "→ Install: ninja-build (или используйте генератор 'Unix Makefiles')";;
      ctest)  echo "→ Обычно ставится вместе с CMake";;
      conan)  echo "→ pip install --user conan";;
      lcov)   echo "→ Install: lcov";;
      gcovr)  echo "→ pip install --user gcovr";;
      curl)   echo "→ Install: curl";;
    esac
    return 1
  fi
  return 0
}

hr() { printf '%*s\n' "${COLUMNS:-80}" '' | tr ' ' '-'; }

# ──────────────────────────────────────────────────────────────────────────────
do_build() {
  log "== Configure & Build =="
  need cmake || err "cmake обязателен"
  [[ "$CMAKE_GENERATOR" == "Ninja" ]] && need ninja || true

  # Conan (опционально)
  if ls conanfile.* >/dev/null 2>&1; then
    if need conan; then
      local bt="$CMAKE_BUILD_TYPE"
      # Для покрытия лучше Debug
      if [[ "$SOFTWARE_COVERAGE_CMAKE" == "ON" ]]; then bt="Debug"; fi
      log "Conan install (build_type=${bt})"
      conan profile detect --force || true
      conan install . -s:h "build_type=${bt}" -of "$BUILD_DIR" -b missing
    else
      warn "conan не найден, пропускаю conan install"
    fi
  fi

  mkdir -p "$BUILD_DIR"

  # Флаги покрытия только когда --coverage
  local cov_cflags="" cov_ldflags=""
  if [[ "$SOFTWARE_COVERAGE_CMAKE" == "ON" ]]; then
    cov_cflags="--coverage -O0 -g"
    cov_ldflags="--coverage"
  fi

  log "CMake configure → $BUILD_DIR"
  cmake -S . -B "$BUILD_DIR" -G "$CMAKE_GENERATOR" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DPROJECT_SEMVER="${PROJECT_SEMVER}" \
    -DSOFTWARE_VERSION="${SOFTWARE_VERSION}" \
    -DBUILD_TESTING="${BUILD_TESTING_CMAKE}" \
    -DSOFTWARE_COVERAGE="${SOFTWARE_COVERAGE_CMAKE}" \
    -DCMAKE_C_FLAGS="${CMAKE_C_FLAGS:-} ${cov_cflags}" \
    -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS:-} ${cov_cflags}" \
    -DCMAKE_EXE_LINKER_FLAGS="${CMAKE_EXE_LINKER_FLAGS:-} ${cov_ldflags}" \
    -DCMAKE_SHARED_LINKER_FLAGS="${CMAKE_SHARED_LINKER_FLAGS:-} ${cov_ldflags}" \
    ${EXTRA_CMAKE_FLAGS}

  log "CMake build"
  cmake --build "$BUILD_DIR" -- -j"$(nproc)" ${EXTRA_BUILD_FLAGS}
}

# ──────────────────────────────────────────────────────────────────────────────
do_tests() {
  log "== Tests =="
  need ctest || err "ctest обязателен"
  # Если проект собран без BUILD_TESTING, будет "No tests were found" — это ок, но считаем это ошибкой конфигурации
  if [[ "$BUILD_TESTING_CMAKE" != "ON" ]]; then
    warn "BUILD_TESTING=OFF (не указан --test при сборке) — тестов может не оказаться"
  fi
  ctest --test-dir "$BUILD_DIR" --output-on-failure --no-tests=error -j"$(nproc)" --output-junit junit.xml
}

# ──────────────────────────────────────────────────────────────────────────────
detect_ignore_errors_arg() {
  # Безопасно собираем список поддерживаемых подпунктов для geninfo разных версий
  local parts=(source graph range unused inconsistent format)
  local supported=()
  if geninfo --help 2>&1 | grep -q -- '--ignore-errors'; then
    local H; H="$(geninfo --help 2>&1 || true)"
    for p in "${parts[@]}"; do
      if echo "$H" | grep -Eiq "(--ignore-errors[^[:alnum:]]+.*\b${p}\b)"; then
        supported+=("$p")
      fi
    done
  fi
  if [[ ${#supported[@]} -gt 0 ]]; then
    echo "--ignore-errors $(IFS=,; echo "${supported[*]}")"
  else
    echo ""
  fi
}

do_coverage() {
  log "== Coverage =="
  if [[ "$SOFTWARE_COVERAGE_CMAKE" != "ON" ]]; then
    warn "Покрытие не было включено при сборке (нужно собирать с --coverage). Пропускаю."
    return 0
  fi

  need lcov  || err "lcov обязателен"
  need gcovr || err "gcovr обязателен (pip install --user gcovr)"

  # Проверим, что есть gcno (компиляция с coverage) и gcda (исполнение тестов)
  if ! find "$BUILD_DIR" -name '*.gcno' -print -quit | grep -q .; then
    err "Не найдено *.gcno в $BUILD_DIR — сборка не была с флагами покрытия"
  fi
  if ! find "$BUILD_DIR" -name '*.gcda' -print -quit | grep -q .; then
    err "Не найдено *.gcda в $BUILD_DIR — тесты не были запущены после сборки с покрытием"
  fi

  pushd "$BUILD_DIR" >/dev/null

  local IGNORE_ARG; IGNORE_ARG="$(detect_ignore_errors_arg)"
  log "geninfo ignore-errors supported: ${IGNORE_ARG:-none}"

  lcov --capture --directory . --output-file coverage.info \
       ${IGNORE_ARG} \
       --rc geninfo_unexecuted_blocks=1

  lcov --remove coverage.info '/usr/*' --output-file coverage.info
  lcov --list coverage.info || true

  gcovr -r .. --xml-pretty --cobertura-pretty -o coverage.xml
  gcovr -r .. --html --html-details -o coverage.html
  gcovr -r .. --txt --print-summary | tee coverage.txt

  popd >/dev/null
}

# ──────────────────────────────────────────────────────────────────────────────
make_badge() {
  if [[ "$SOFTWARE_COVERAGE_CMAKE" != "ON" ]]; then
    warn "[badge] покрытие не включено — бейдж пропущен"
    return 0
  fi

  local COVERAGE_TXT="$BUILD_DIR/coverage.txt"
  local BADGE_DIR="badges"
  local BADGE_FILE="$BADGE_DIR/coverage.svg"
  mkdir -p "$BADGE_DIR"

  local PCT=""
  if [[ -f "$COVERAGE_TXT" ]]; then
    PCT=$(grep -Eo 'lines:\s*[0-9]+(\.[0-9]+)?%' "$COVERAGE_TXT" | tail -n1 | grep -Eo '[0-9]+(\.[0-9]+)?' || true)
    [[ -z "$PCT" ]] && PCT=$(grep -Eo '[0-9]+(\.[0-9]+)?%' "$COVERAGE_TXT" | tail -n1 | tr -d '%' || true)
  fi
  PCT="${PCT:-0}"

  local PCT_INT="${PCT%.*}"
  local color="red"
  if   [[ "$PCT_INT" -ge 90 ]]; then color="brightgreen"
  elif [[ "$PCT_INT" -ge 80 ]]; then color="green"
  elif [[ "$PCT_INT" -ge 70 ]]; then color="yellowgreen"
  elif [[ "$PCT_INT" -ge 60 ]]; then color="yellow"
  elif [[ "$PCT_INT" -ge 40 ]]; then color="orange"
  else color="red"; fi

  if need curl; then
    local url="https://img.shields.io/badge/coverage-${PCT}%25-${color}.svg"
    log "[badge] ${PCT}% → $url"
    if curl -fsSL "$url" -o "$BADGE_FILE"; then
      log "[badge] saved at $BADGE_FILE"
      return 0
    fi
    warn "[badge] curl failed, делаю локальный svg"
  else
    warn "[badge] curl не найден, делаю локальный svg"
  fi

  # Локальный минимальный SVG (fallback)
  cat > "$BADGE_FILE" <<SVG
<svg xmlns="http://www.w3.org/2000/svg" width="120" height="20" role="img" aria-label="coverage: ${PCT}%">
  <linearGradient id="s" x2="0" y2="100%"><stop offset="0" stop-color="#bbb" stop-opacity=".1"/><stop offset="1" stop-opacity=".1"/></linearGradient>
  <mask id="m"><rect width="120" height="20" rx="3" fill="#fff"/></mask>
  <g mask="url(#m)"><rect width="60" height="20" fill="#555"/><rect x="60" width="60" height="20" fill="#4c1"/><rect width="120" height="20" fill="url(#s)"/></g>
  <g fill="#fff" text-anchor="middle" font-family="DejaVu Sans,Verdana,sans-serif" font-size="11">
    <text x="30" y="15">coverage</text><text x="90" y="15">${PCT}%</text>
  </g>
</svg>
SVG
  # Подкраска по уровню
  case "$color" in
    brightgreen) sed -i 's/fill="#4c1"/fill="#4c1"/' "$BADGE_FILE" ;;
    green)       sed -i 's/fill="#4c1"/fill="#97CA00"/' "$BADGE_FILE" ;;
    yellowgreen) sed -i 's/fill="#4c1"/fill="#a4a61d"/' "$BADGE_FILE" ;;
    yellow)      sed -i 's/fill="#4c1"/fill="#dfb317"/' "$BADGE_FILE" ;;
    orange)      sed -i 's/fill="#4c1"/fill="#fe7d37"/' "$BADGE_FILE" ;;
    red)         sed -i 's/fill="#4c1"/fill="#e05d44"/' "$BADGE_FILE" ;;
  esac
  log "[badge] fallback saved at $BADGE_FILE"
}

# ──────────────────────────────────────────────────────────────────────────────
log "build-dir=${BUILD_DIR}"
log "cmake-type=${CMAKE_BUILD_TYPE} generator=${CMAKE_GENERATOR}"
log "PROJECT_SEMVER=${PROJECT_SEMVER} SOFTWARE_VERSION=${SOFTWARE_VERSION}"
log "BUILD_TESTING=${BUILD_TESTING_CMAKE} SOFTWARE_COVERAGE=${SOFTWARE_COVERAGE_CMAKE}"
hr

if ! $RUN_BUILD && ! $RUN_TESTS && ! $RUN_COVERAGE && ! $MAKE_BADGE; then
  usage; exit 1
fi

$RUN_BUILD     && do_build
$RUN_TESTS     && do_tests
$RUN_COVERAGE  && do_coverage
$MAKE_BADGE    && make_badge

log "✅ done"