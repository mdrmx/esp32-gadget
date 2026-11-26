#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT_DIR"

fail() {
  printf '%s\n' "Network transport boundary check failed: $1" >&2
  exit 1
}

HTTP_CLIENT="src/services/HttpRequestClient.h"
SKETCH="weatherDashboard.ino"
LOCATION="src/services/LocationService.h"

grep -Fq '#include <NetworkClientSecure.h>' "$HTTP_CLIENT" ||
  fail "HttpRequestClient must include NetworkClientSecure.h"
grep -Fq 'NetworkClientSecure secureClient_;' "$HTTP_CLIENT" ||
  fail "HttpRequestClient must own NetworkClientSecure"
if grep -Fq 'WiFiClientSecure' "$HTTP_CLIENT"; then
  fail "HttpRequestClient must not depend on the WiFiClientSecure alias"
fi

grep -Fq '!connectivityManager.isConnected()' "$SKETCH" ||
  fail "Weather icon preparation must gate on ConnectivityManager state"
if grep -Fq 'WiFi.status()' "$SKETCH"; then
  fail "the Weather composition root must not use WiFi.status() for generic connectivity"
fi

if grep -Eq '#include <WiFi.h>|WiFi\.(status|scanNetworks|BSSID|RSSI|channel|scanDelete)\(' "$LOCATION"; then
  fail "LocationService must not call ESP32 Wi-Fi APIs directly"
fi

printf '%s\n' "Network transport boundary checks passed."
