#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/text-renderer.XXXXXX")

trap 'rm -rf "$BUILD_DIR"' EXIT
trap 'exit 1' HUP INT TERM

CXX="${CXX:-g++}"
"$CXX" -std=c++11 -Wall -Wextra -Werror -pedantic \
  -DNDEBUG \
  -I"$ROOT_DIR/tests/host/stubs" \
  -I"$ROOT_DIR" \
  "$ROOT_DIR/tests/host/text_renderer_test.cpp" \
  -o "$BUILD_DIR/text_renderer_test"

"$BUILD_DIR/text_renderer_test"
