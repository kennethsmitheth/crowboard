#!/usr/bin/env bash
# Format C/C++ sources with clang-format.
#
#   tools/format.sh          rewrite files in place
#   tools/format.sh --check  report differences without editing; exits non-zero if any
#
# Covers src/**/*.{h,cpp} and test/**/*.{h,cpp}. Looks for clang-format on PATH, then in the
# PlatformIO Python environment, then in Homebrew's prefix.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 2

find_clang_format() {
  local candidate
  for candidate in \
    "$(command -v clang-format 2>/dev/null)" \
    "$HOME/.platformio/penv/bin/clang-format" \
    "/opt/homebrew/bin/clang-format" \
    "/usr/local/bin/clang-format"; do
    if [ -n "$candidate" ] && [ -x "$candidate" ]; then
      echo "$candidate"
      return 0
    fi
  done
  return 1
}

CF="$(find_clang_format)" || {
  echo "error: clang-format not found. Install it with 'brew install clang-format' or" >&2
  echo "       '~/.platformio/penv/bin/python -m pip install clang-format'." >&2
  exit 2
}

# Same file set in both modes: src/**/*.{h,cpp} and test/**/*.{h,cpp}.
FILES_EXPR=(\( -path 'src/*' \( -name '*.h' -o -name '*.cpp' \) \) -o \( -path 'test/*' \( -name '*.h' -o -name '*.cpp' \) \))

case "${1:-}" in
  --check)
    find src test -type f \( "${FILES_EXPR[@]}" \) -exec "$CF" --dry-run -Werror {} +
    ;;
  "")
    find src test -type f \( "${FILES_EXPR[@]}" \) -exec "$CF" -i {} +
    ;;
  *)
    echo "usage: tools/format.sh [--check]" >&2
    exit 2
    ;;
esac
