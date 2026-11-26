#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/weather-scroll-state.XXXXXX")

trap 'rm -rf "$BUILD_DIR"' EXIT
trap 'exit 1' HUP INT TERM

CXX="${CXX:-g++}"
"$CXX" -std=c++11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT_DIR" \
  -DNDEBUG \
  "$ROOT_DIR/tests/host/weather_scroll_state_test.cpp" \
  -o "$BUILD_DIR/weather_scroll_state_test"

"$BUILD_DIR/weather_scroll_state_test"
