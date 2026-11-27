#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/bottom-edge-swipe.XXXXXX")

trap 'rm -rf "$BUILD_DIR"' EXIT
trap 'exit 1' HUP INT TERM

CXX="${CXX:-g++}"
"$CXX" -std=c++11 -Wall -Wextra -Werror -pedantic \
  -DNDEBUG \
  -I"$ROOT_DIR" \
  "$ROOT_DIR/tests/host/bottom_edge_swipe_gesture_test.cpp" \
  -o "$BUILD_DIR/bottom_edge_swipe_gesture_test"

"$BUILD_DIR/bottom_edge_swipe_gesture_test"
