#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/wifi-profile-store.XXXXXX")

trap 'rm -rf "$BUILD_DIR"' EXIT
trap 'exit 1' HUP INT TERM

CXX="${CXX:-g++}"
"$CXX" -std=c++11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT_DIR" \
  "$ROOT_DIR/tests/host/wifi_profile_store_test.cpp" \
  -o "$BUILD_DIR/wifi_profile_store_test"
"$BUILD_DIR/wifi_profile_store_test"

"$CXX" -std=c++11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT_DIR/tests/host/stubs" \
  -I"$ROOT_DIR" \
  "$ROOT_DIR/tests/host/preferences_wifi_profile_storage_test.cpp" \
  -o "$BUILD_DIR/preferences_wifi_profile_storage_test"
"$BUILD_DIR/preferences_wifi_profile_storage_test"
