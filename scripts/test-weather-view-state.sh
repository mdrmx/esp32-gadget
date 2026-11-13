#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/weather-view-state.XXXXXX")

# Always clean up, but preserve a failing status when the script is interrupted.
trap 'rm -rf "$BUILD_DIR"' EXIT
trap 'exit 1' HUP INT TERM

CXX="${CXX:-g++}"
"$CXX" -std=c++11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT_DIR" \
  "$ROOT_DIR/tests/host/weather_view_state_test.cpp" \
  -o "$BUILD_DIR/weather_view_state_test"

"$BUILD_DIR/weather_view_state_test"
