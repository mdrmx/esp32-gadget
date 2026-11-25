#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/wifi-geolocation-scanner.XXXXXX")

trap 'rm -rf "$BUILD_DIR"' EXIT
trap 'exit 1' HUP INT TERM

CXX="${CXX:-g++}"
"$CXX" -std=c++11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT_DIR" \
  "$ROOT_DIR/tests/host/wifi_geolocation_scanner_test.cpp" \
  -o "$BUILD_DIR/wifi_geolocation_scanner_test"
"$BUILD_DIR/wifi_geolocation_scanner_test"
