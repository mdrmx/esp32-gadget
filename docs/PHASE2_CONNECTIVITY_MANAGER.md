# Phase 2 — Connectivity Manager and Wi-Fi backend

**Status:** In progress — first implementation slice on `phase2/01-connectivity-manager-wifi-backend`  
**Base:** `main` at `ed4991e249a42d68ec59cecc6d3f0931d06af19a`  
**Scope:** establish the shared connectivity boundary and route the existing Wi-Fi association lifecycle through it without redesigning the Weather startup experience.

## 1. Design boundary

The platform owns network-interface lifecycle and connection state. Applications and domain services consume connectivity state; they must not select, start or stop a transport.

The first slice adds three small modules:

- `ConnectivityBackend.h` defines a transport-neutral interface and `NetworkType` (`None`, `WiFi`, `Cellular`).
- `ConnectivityManager.h` owns connection state, start/update/disconnect operations and a bounded connection timeout.
- `WiFiBackend.h` implements the interface with the ESP32 Arduino Wi-Fi station APIs.

The manager does not include Arduino or Wi-Fi headers. A future cellular backend can implement the same contract without changing the manager or Weather's use of the manager.

## 2. Connection state

`ConnectivityManager::connect(nowMs)` starts one backend attempt and returns immediately. It rejects a second attempt while one is already connecting. The caller must continue to call `update(nowMs)`.

The state transitions are:

```text
Disconnected --connect accepted--> Connecting
Connecting --backend connected----> Connected
Connecting --20 s elapsed---------> TimedOut
Connecting --backend start failed-> Failed
Connected --link lost-------------> Disconnected
Connected --disconnect------------> Disconnected
TimedOut / Failed --connect-------> Connecting (new attempt)
```

The default connection timeout is 20 seconds, matching the previous sketch-level connection wait. Elapsed-time arithmetic is unsigned and is tested across `millis()` rollover. A timeout disconnects the backend. The manager does not retain profile credentials; profile selection and credential lifetime remain outside the manager.

The backend owns platform-specific profile configuration. For this first slice, `WiFiBackend::configure()` borrows the SSID/password strings. The current sketch supplies static values from the ignored `secrets.h`, whose lifetime is process-wide. When NVS-backed profiles are introduced, the profile store must keep the selected strings alive for as long as the backend may use them or the backend must take explicit ownership.

## 3. Current Weather integration

`weatherDashboard.ino` now creates `WiFiBackend` and `ConnectivityManager` at the composition root.

- The existing `connectWiFi()` helper delegates association start and polling to the manager.
- Time synchronisation receives `connectivityManager.isConnected()` instead of reading `WiFi.status()` in the sketch.
- Location startup receives the manager's connection state.
- `loop()` calls `connectivityManager.update(millis())` so link loss is reflected in shared state during normal operation.

**Important transitional limitation:** the existing `connectWiFi()` call sites are synchronous. The helper currently pumps `ConnectivityManager::update()` and retains the existing 500 ms polling delay so Weather startup and refresh sequencing are not changed in this PR. Therefore, the manager API is non-blocking, but the complete Weather startup/refresh path is not yet non-blocking and can still wait up to 20 seconds during association. This slice must not be reported as having removed the UI freeze on a missing access point.

The next integration slice should move connection progress into the regular application update path, make initial location/weather loading wait on explicit connection state, and remove this compatibility loop without changing the existing success, stale-data or fallback presentation.

## 4. Deliberately deferred

This first slice does not yet implement:

- Preferences / NVS storage or multiple saved Wi-Fi profiles.
- Network scanning, selection, password-entry UI, preferred-network ordering or forget.
- Migration from compile-time credentials.
- Automatic retry/backoff policy or background reconnect while idle.
- Moving all Weather connection-dependent work to an asynchronous application state machine.
- Moving Wi-Fi scanning/BSSID collection out of `LocationService`; its Google Wi-Fi positioning path still uses the ESP32 Wi-Fi scan API.
- Replacing the Wi-Fi-specific `HttpRequestClient` transport or changing the existing TLS compatibility policy.
- Cellular modem support.

These are separate reviewable follow-up slices. Do not introduce FreeRTOS tasks, concurrent HTTP requests or a modem-specific network path as part of this boundary extraction.

## 5. Host tests and remaining validation

Run:

```sh
sh scripts/test-connectivity-manager.sh
sh scripts/test-wifi-backend.sh
```

The two focused strict C++11 host test suites pass in the implementation workspace. They cover accepted and duplicate starts, backend failure, connected/disconnected transitions, timeout, rollover, reconnect, network-type reporting, Wi-Fi profile configuration, open networks and RSSI reporting.

Still required before merge:

- Arduino IDE Verify using the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN and ESP32 core 3.3.12.
- Firmware upload and normal boot on the Waveshare ESP32-S3 Touch LCD 2.
- Physical regression of successful Wi-Fi startup, time synchronisation, Google Wi-Fi/IP location selection, current weather, hourly forecast and all existing Weather/Forecast interactions.
- A controlled unavailable-AP test confirming the existing 20-second fallback/error behaviour and confirming no new regression.
- Record build footprint and runtime heap/PSRAM observations separately.

## 6. Next implementation sequence

1. **Non-blocking Weather startup/update integration:** remove the compatibility wait loop and make connection progress, location resolution and first data loading explicit application states.
2. **Persistent Wi-Fi profiles:** add a versioned Preferences/NVS store with bounded profile count, safe credential replacement, preferred profile and forget semantics.
3. **On-device network management:** scan and select networks, password entry, save/forget controls and clear connection/error feedback.
4. **Reconnect policy:** introduce bounded backoff, link-loss recovery and a defined preferred-network policy using saved profiles.
5. **Network capability boundary:** move the remaining Wi-Fi scan/BSSID operations behind a platform-owned capability and review whether HTTP request transport should be abstracted separately from connectivity state.

Keep each slice behaviour-preserving and independently testable. Keep passwords out of logs, serial diagnostics and tracked files.
