#!/usr/bin/env bash
#
# @file scripts/clang_tidy.sh
# @brief Project-focused wrapper around clang-tidy / run-clang-tidy.
#
# This script runs **clang-tidy** against only your project sources and hides
# diagnostics from third‑party folders (e.g. `build/`, `_deps/`, `third_party/`, `external/`)
# while still showing notes that relate to your files. It also prints a compact
# summary with total error/warning counts suitable for CI.
#
# @par Key features
# - Works directly from `compile_commands.json`.
# - Filters out noise from vendored/third‑party code and generated build trees.
# - Optionally analyzes standalone headers (`.h/.hpp`) in addition to files from the compilation DB.
# - macOS smart defaults: auto‑detects SDK and sets `-isysroot`.
# - Emits a one‑line machine‑readable trailer for CI parsing.
#
# @section usage Usage
# From the project root (where your `CMakeLists.txt` lives):
# @code{.bash}
# # 1) Configure CMake to produce compile_commands.json
# cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
#
# # 2) Build (so that compile_commands.json contains all your TUs)
# cmake --build build -j
#
# # 3) Run clang-tidy via this script
# scripts/clang_tidy.sh
# @endcode
#
# If your build directory is different:
# @code{.bash}
# BUILD_DIR=out/RelWithDebInfo scripts/clang_tidy.sh
# @endcode
#
# Analyze headers as standalone translation units (in addition to sources from the DB):
# @code{.bash}
# TIDY_HEADERS=1 scripts/clang_tidy.sh
# @endcode
#
# Fail CI when at least one error is reported:
# @code{.bash}
# TIDY_FAIL_ON_ERROR=1 scripts/clang_tidy.sh
# @endcode
#
# @section env Environment Variables
# The following variables can be provided to adjust behavior (all have sensible defaults):
#
# | Variable             | Default                                   | Purpose |
# |----------------------|-------------------------------------------|---------|
# | `BUILD_DIR`          | `build`                                    | Path to CMake build directory that contains `compile_commands.json`. |
# | `DB_FILE`            | `$BUILD_DIR/compile_commands.json`         | Full path to the compilation database. |
# | `TIDY_HEADERS`       | `0`                                        | If `1`, also run clang-tidy on all project headers (`.h/.hpp`) as standalone files. |
# | `RUN_TIDY_BIN`       | `run-clang-tidy`                           | Wrapper binary to prefer if available. |
# | `CLANG_TIDY_BIN`     | `clang-tidy`                               | Fallback binary if `run-clang-tidy` is not found. |
# | `TIDY_FAIL_ON_ERROR` | `0`                                        | If `1`, script exits with non‑zero code when any **errors** are found. |
# | `JOBS`               | auto                                       | Parallel jobs for run-clang-tidy if supported. |
#
# @note
# - The script automatically **excludes** paths matching `(.*/|^)(build|_deps|third_party|external)(/|$)`.
#   Edit `EXCLUDE_REGEX` inside the script if you need a different policy.
# - Diagnostics from GoogleTest or other vendored code are suppressed unless they originate
#   from your files (notes tied to your file lines are still kept).
# - On macOS, the script auto‑injects `-extra-arg=-isysroot -extra-arg=$(xcrun --show-sdk-path)` if possible.
#
# @section out Output
# The script prints a list of files to be analyzed and then the filtered diagnostics.
# At the end you will see a compact summary:
# @code
# [ci] clang-tidy summary: 2 error(s), 17 warning(s)
# @endcode
# and a machine‑readable footer on the last line:
# @code
# __TIDY_SUMMARY__ ERR=2 WARN=17
# @endcode
# which you can parse in CI if needed.
#
# @section ci CI Integration
# **GitLab CI** example:
# @code{.yaml}
# clang-tidy:
#   stage: lint
#   script:
#     - cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
#     - cmake --build build -j
#     - TIDY_FAIL_ON_ERROR=1 scripts/clang_tidy.sh
#   artifacts:
#     when: always
#     paths: [ clang-tidy.log ]
# @endcode
#
# @section tips Tips & Troubleshooting
# - If some of your sources are missing from the analysis, ensure they belong to a CMake target
#   and reconfigure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`. The script prints a helpful list
#   under “NOT in compile_commands.json”.
# - To include headers without enabling `TIDY_HEADERS=1`, make sure they are transitively included
#   from at least one TU present in `compile_commands.json`.
# - To analyze only a subset of files, temporarily restrict your CMake targets or edit
#   the `FILES` list/filters near the bottom of this script.


set -euo pipefail

# ------------------------------------------
# Minimal CLI parsing for --build-dir, --db-file, --jobs/-j
# ------------------------------------------
CLI_BUILD_DIR=""
CLI_DB_FILE=""
CLI_JOBS=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      CLI_BUILD_DIR="$2"
      shift 2
      ;;
    --db-file)
      CLI_DB_FILE="$2"
      shift 2
      ;;
    --jobs)
      CLI_JOBS="$2"
      shift 2
      ;;
    -j)
      CLI_JOBS="$2"
      shift 2
      ;;
    *)
      break
      ;;
  esac
done

#
#
# Config (можно переопределять через env)
#
BUILD_DIR="${BUILD_DIR:-build}"
[[ -n "$CLI_BUILD_DIR" ]] && BUILD_DIR="$CLI_BUILD_DIR"
DB_FILE_DEFAULT="$BUILD_DIR/compile_commands.json"
DB_FILE="${DB_FILE:-$DB_FILE_DEFAULT}"
[[ -n "$CLI_DB_FILE" ]] && DB_FILE="$CLI_DB_FILE"
TIDY_HEADERS="${TIDY_HEADERS:-0}"

# Корень проекта
PROJECT_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

# Исключения путей
EXCLUDE_REGEX='(.*/|^)(build|_deps|third_party|external)(/|$)'

# Разрешаем диагностику во ВСЕХ ваших заголовках, но продолжаем
# скрывать предупреждения из внешних папок.
escape_regex() {
  printf '%s' "$1" | sed -e 's/[.[\()*^$+?{|\\]/\\&/g'
}
PROJECT_ROOT_RX="$(escape_regex "$PROJECT_ROOT")"
HEADER_FILTER="^${PROJECT_ROOT_RX}/(?!.*(/|^)(build|_deps|third_party|external)(/|$)).*"

# ------------------------------------------
# Бинарники: стараемся найти даже если PATH "урезан"
# ------------------------------------------
_find_bin() {
  local name="$1"; shift
  # 1) prefer PATH
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return 0
  fi
  # 2) try common locations
  local cand
  for cand in "$@"; do
    if [[ -x "$cand" ]]; then
      printf '%s\n' "$cand"
      return 0
    fi
  done
  return 1
}

# Кандидаты для поиска
: "${RUN_TIDY_BIN:=run-clang-tidy}"
: "${CLANG_TIDY_BIN:=clang-tidy}"

RUN_TIDY_BIN="$(_find_bin "${RUN_TIDY_BIN}" \
  /usr/bin/run-clang-tidy /usr/local/bin/run-clang-tidy \
  /opt/homebrew/opt/llvm/bin/run-clang-tidy /opt/local/libexec/llvm-*/bin/run-clang-tidy 2>/dev/null || true)"

CLANG_TIDY_BIN="$(_find_bin "${CLANG_TIDY_BIN}" \
  /usr/bin/clang-tidy /usr/local/bin/clang-tidy \
  /opt/homebrew/opt/llvm/bin/clang-tidy /opt/local/libexec/llvm-*/bin/clang-tidy 2>/dev/null || true)"

# macOS isysroot (если нужно)
EXTRA_ARGS=()
if [[ "$(uname -s)" == "Darwin" ]]; then
  if command -v xcrun >/dev/null 2>&1; then
    SDK_PATH="$(xcrun --show-sdk-path 2>/dev/null || true)"
    if [[ -n "${SDK_PATH:-}" ]]; then
      EXTRA_ARGS+=("-extra-arg=-isysroot" "-extra-arg=${SDK_PATH}")
    fi
  fi
fi

# ------------------------------------------
# Предусловия
# ------------------------------------------
if [[ ! -f "$DB_FILE" ]]; then
  echo "[err] Missing $DB_FILE. Configure cmake with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
  echo "Hint: EXTRA_CMAKE_FLAGS='-DCMAKE_EXPORT_COMPILE_COMMANDS=ON' cmake -S . -B $BUILD_DIR ..."
  exit 1
fi

if [[ -z "${RUN_TIDY_BIN}" && -z "${CLANG_TIDY_BIN}" ]]; then
  echo "[err] Neither run-clang-tidy nor clang-tidy found."
  echo "→ install: LLVM/clang-tools (e.g. apt install clang-tidy / brew install llvm)"
  echo "→ also check PATH; common locations: /usr/bin, /usr/local/bin, /opt/homebrew/opt/llvm/bin"
  exit 1
fi

# ------------------------------------------
# Список исходников из compile_commands.json (нормализуем пути)
mapfile -t SRC_FILES < <(jq -r \
  '.[] | if (.file|startswith("/")) then .file else (.directory + "/" + .file) end' \
  "$DB_FILE" \
  | grep -Ei '\.(c|cc|cpp|cxx)$' \
  | grep -Evi "$EXCLUDE_REGEX" \
  | sort -u)

# Диагностика: какие .c/.cc/.cpp/.cxx есть в проекте, но не попали в compile_commands.json
mapfile -t FS_CPP < <(find "$PROJECT_ROOT" \
        \( -path "$PROJECT_ROOT/build" -o -path "$PROJECT_ROOT/_deps" -o -path "$PROJECT_ROOT/third_party" -o -path "$PROJECT_ROOT/external" \) -prune -o \
        -type f \( -iname '*.c' -o -iname '*.cc' -o -iname '*.cpp' -o -iname '*.cxx' \) -print \
      | sort -u)

MISSING_IN_DB=()
if [[ ${#FS_CPP[@]} -gt 0 ]]; then
  TMP_DB=$(mktemp)
  TMP_FS=$(mktemp)
  printf '%s\n' "${SRC_FILES[@]}" > "$TMP_DB"
  printf '%s\n' "${FS_CPP[@]}"   > "$TMP_FS"
  while IFS= read -r f; do
    if ! grep -Fxq -- "$f" "$TMP_DB"; then
      MISSING_IN_DB+=("$f")
    fi
  done < "$TMP_FS"
  rm -f "$TMP_DB" "$TMP_FS"
fi

# ------------------------------------------
# (Опц.) Добавляем «голые» заголовки для прямого анализа
# ------------------------------------------
HEADER_FILES=()
if [[ "$TIDY_HEADERS" == "1" ]]; then
  while IFS= read -r f; do
    HEADER_FILES+=("$f")
  done < <(find "$PROJECT_ROOT" \
          \( -path "$PROJECT_ROOT/build" -o -path "$PROJECT_ROOT/_deps" -o -path "$PROJECT_ROOT/third_party" -o -path "$PROJECT_ROOT/external" -o -path '*/_deps/*' \) -prune -o \
          -type f \( -iname '*.h' -o -iname '*.hpp' \) -print | sort -u)
fi

FILES=("${SRC_FILES[@]}")
if [[ "${#HEADER_FILES[@]}" -gt 0 ]]; then
  FILES+=("${HEADER_FILES[@]}")
fi

if [[ "${#FILES[@]}" -eq 0 ]]; then
  echo "[ci] Clang-Tidy: no files matched filters. Nothing to do."
  exit 0
fi

#
# Конфигурационный вывод
#
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
[[ -n "$CLI_JOBS" ]] && JOBS="$CLI_JOBS"

echo "[ci] == Clang-Tidy =="
echo "[ci] Compilation DB : $DB_FILE"
echo "[ci] Project root   : $PROJECT_ROOT"
echo "[ci] Header filter  : $HEADER_FILTER"
echo "[ci] Exclude filter : $EXCLUDE_REGEX"
echo "[ci] Extra args     : ${EXTRA_ARGS[*]:-(none)}"
echo "[ci] run-clang-tidy : ${RUN_TIDY_BIN:-<not found>}"
echo "[ci] clang-tidy     : ${CLANG_TIDY_BIN:-<not found>}"
echo "[ci] Jobs (-j)      : ${JOBS}"
echo "[ci] Files to analyze (${#SRC_FILES[@]}):"
for f in "${SRC_FILES[@]}"; do
  echo "  - $f"
done

# If TIDY_HEADERS=1, also show which headers will be analyzed as standalone TUs
if [[ ${#HEADER_FILES[@]} -gt 0 ]]; then
  echo "[ci] Headers (standalone) to analyze (${#HEADER_FILES[@]}):"
  for f in "${HEADER_FILES[@]}"; do
    echo "  - $f"
  done
fi

# ------------------------------------------
# Фильтрация вывода и подсчёт ошибок/варнов
# ------------------------------------------
post_filter_awkscript='BEGIN{
  RS="";
  ORS="\n";
  err=0; warn=0; pb=0;
}
{
  show=0;
  n=split($0,L,"\n");
  for(i=1;i<=n;i++){
    if(L[i] ~ root && L[i] ~ /: (warning|error):/){ show=1 }
  }
  if(show){
    for(i=1;i<=n;i++){
      if(L[i] ~ /(^|\/)(build|_deps|third_party|external)\//) continue;
      if(L[i] ~ /googletest/) continue;
      if(L[i] ~ /: note:/ && L[i] !~ root) continue;

      if(L[i] ~ /^[[:space:]]*$/){
        if(pb==0){ print ""; pb=1 }
        continue
      } else {
        pb=0
      }

      print L[i];

      if(L[i] ~ /: error:/)   err++;
      else if(L[i] ~ /: warning:/) warn++;
    }
  }
}
END{
  printf("[ci] clang-tidy summary: %d error(s), %d warning(s)\n", err, warn);
  printf("__TIDY_SUMMARY__ ERR=%d WARN=%d\n", err, warn);
}'

QUIET_FLAG=("-quiet")

# Проверка поддержки флагов run-clang-tidy
supports_rct_flag() {
  local flag="$1"
  "$RUN_TIDY_BIN" -help 2>&1 | grep -q -- "${flag}"
}

# ------------------------------------------
# Запуск
# ------------------------------------------
TMP_TIDY_OUT="$(mktemp)"

if [[ -n "${RUN_TIDY_BIN}" ]]; then
  RT_ARGS=(
    -p "$BUILD_DIR"
    -header-filter="${HEADER_FILTER}"
  )
  if supports_rct_flag "-exclude-header-filter"; then
    RT_ARGS+=( -exclude-header-filter="${EXCLUDE_REGEX}" )
  fi
  if supports_rct_flag "-j"; then
    RT_ARGS+=( -j "${JOBS}" )
  fi
  # Ensure run-clang-tidy uses the same clang-tidy we detected
  if [[ -n "${CLANG_TIDY_BIN}" ]]; then
    RT_ARGS+=( -clang-tidy-binary "${CLANG_TIDY_BIN}" )
  fi
  RT_ARGS+=( "${EXTRA_ARGS[@]}" )
  RT_ARGS+=( "${QUIET_FLAG[@]}" )

  "$RUN_TIDY_BIN" "${RT_ARGS[@]}" "${FILES[@]}" \
    | awk -v root="$PROJECT_ROOT" "$post_filter_awkscript" | tee "$TMP_TIDY_OUT"
else
  FAIL=0
  for f in "${FILES[@]}"; do
    "$CLANG_TIDY_BIN" \
      -p "$BUILD_DIR" \
      -header-filter="$HEADER_FILTER" \
      "${QUIET_FLAG[@]}" \
      -extra-arg=-Wno-unknown-warning-option \
      -extra-arg=-std=c++17 \
      "${EXTRA_ARGS[@]}" \
      "$f" || FAIL=1
  done | awk -v root="$PROJECT_ROOT" "$post_filter_awkscript" | tee "$TMP_TIDY_OUT"
  # В ветке с поштучным clang-tidy уже вернули код возврата в FAIL. Т.к. фильтрация пайпом
  # скрывает реальный exit code, возвращаем FAIL отдельно:
  if [[ $FAIL -ne 0 ]]; then
    echo "[ci] clang-tidy exited with non-zero for some files (see above)."
  fi
fi

# ------------------------------------------
# Parse summary from awk output
# ------------------------------------------
ERR=0; WARN=0
if [[ -s "$TMP_TIDY_OUT" ]]; then
  if grep -q "__TIDY_SUMMARY__" "$TMP_TIDY_OUT"; then
    ERR="$(grep -Eo '__TIDY_SUMMARY__ ERR=[0-9]+' "$TMP_TIDY_OUT" | sed 's/.*ERR=//')"
    WARN="$(grep -Eo '__TIDY_SUMMARY__ .*WARN=[0-9]+' "$TMP_TIDY_OUT" | sed 's/.*WARN=//')"
  fi
fi
echo "[ci] == Summary =="
echo "[ci] Errors:   ${ERR}"
echo "[ci] Warnings: ${WARN}"

# ------------------------------------------
# Fail on error if requested
# ------------------------------------------
if [[ "${TIDY_NOT_FAIL_ON_ERROR:-0}" != "1" && ${ERR:-0} -gt 0 ]]; then
  echo "[ci] clang-tidy failed: ${ERR} error(s) detected."
  rm -f "$TMP_TIDY_OUT" 2>/dev/null || true
  exit 1
fi

rm -f "$TMP_TIDY_OUT" 2>/dev/null || true