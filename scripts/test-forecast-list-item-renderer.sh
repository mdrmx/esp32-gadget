#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/forecast-list-item-renderer.XXXXXX")

trap 'rm -rf "$BUILD_DIR"' EXIT
trap 'exit 1' HUP INT TERM

CXX="${CXX:-g++}"
"$CXX" -std=c++11 -Wall -Wextra -Werror -pedantic \
  -DNDEBUG \
  -I"$ROOT_DIR/tests/host/stubs" \
  -I"$ROOT_DIR" \
  "$ROOT_DIR/tests/host/forecast_list_item_renderer_test.cpp" \
  -o "$BUILD_DIR/forecast_list_item_renderer_test"

"$BUILD_DIR/forecast_list_item_renderer_test"
