# Phase 2 — Connectivity Manager and Wi-Fi backend

**Status:** Slice 1 merged and validated in PR #52. Slice 2 is implemented and firmware/device-validated on `phase2/02-nonblocking-weather-startup` in PR #53; awaiting merge. Phase 2 overall remains in progress.  
**Base:** `main` at `ed4991e249a42d68ec59cecc6d3f0931d06af19a`  
**Merged checkpoint:** PR #52 at `bc1948019c31ce466ea97be34f205373d05c7954`  
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
Disconnected --backend/driver reconnects--> Connected
Connected --disconnect------------> Disconnected
TimedOut / Failed --connect-------> Connecting (new attempt)
```

The default connection timeout is 20 seconds, matching the previous sketch-level connection wait. Elapsed-time arithmetic is unsigned and is tested across `millis()` rollover. A timeout disconnects the backend. When the manager is no longer in `Connecting`, it mirrors the backend's live connection state, so a driver-managed reconnection returns the manager to `Connected` without another `connect()` call. This observes reconnection performed by the underlying backend/driver; it is not a manager-owned retry or backoff policy. The manager does not retain profile credentials; profile selection and credential lifetime remain outside the manager.

The backend owns platform-specific profile configuration. For this first slice, `WiFiBackend::configure()` borrows the SSID/password strings. A null or empty password is normalised to `nullptr` and passed directly to `WiFi.begin(ssid, password)`, explicitly supporting open access points. The current sketch supplies static values from the ignored `secrets.h`, whose lifetime is process-wide. When NVS-backed profiles are introduced, the profile store must keep the selected strings alive for as long as the backend may use them or the backend must take explicit ownership.

## 3. Current Weather integration

`weatherDashboard.ino` creates `WiFiBackend` and `ConnectivityManager` at the composition root. This implementation is now part of `main` through merged PR #52 (`bc1948019c31ce466ea97be34f205373d05c7954`).

- The existing `connectWiFi()` helper delegates association start and polling to the manager.
- Time synchronisation receives `connectivityManager.isConnected()` instead of reading `WiFi.status()` in the sketch.
- Location startup receives the manager's connection state.
- `loop()` calls `connectivityManager.update(millis())` so link loss is reflected in shared state during normal operation.

**Important transitional limitation:** the existing `connectWiFi()` call sites are synchronous. The helper currently pumps `ConnectivityManager::update()` and retains the existing 500 ms polling delay so Weather startup and refresh sequencing are not changed in this PR. Therefore, the manager API is non-blocking, but the complete Weather startup/refresh path is not yet non-blocking and can still wait up to 20 seconds during association. This slice must not be reported as having removed the UI freeze on a missing access point.

That next integration slice is now implemented on `phase2/02-nonblocking-weather-startup`; see Section 8. The branch removes this compatibility wait loop and moves startup/refresh sequencing into `WeatherApp.update()` without changing the existing success, stale-data or fallback presentation.

## 4. Deliberately deferred

This first slice does not yet implement:

- Preferences / NVS storage or multiple saved Wi-Fi profiles.
- Network scanning, selection, password-entry UI, preferred-network ordering or forget.
- Migration from compile-time credentials.
- Manager-initiated automatic retry/backoff policy and preferred-network selection/recovery. Driver-managed background reconnect is observed and reflected in manager state, but the manager does not itself initiate another attempt.
- Moving all Weather connection-dependent work to an asynchronous application state machine.
- Moving Wi-Fi scanning/BSSID collection out of `LocationService`; its Google Wi-Fi positioning path still uses the ESP32 Wi-Fi scan API.
- Replacing the Wi-Fi-specific `HttpRequestClient` transport or changing the existing TLS compatibility policy.
- Cellular modem support.

These are separate reviewable follow-up slices. Do not introduce FreeRTOS tasks, concurrent HTTP requests or a modem-specific network path as part of this boundary extraction.

## 5. Validation record after merge

Run:

```sh
sh scripts/test-connectivity-manager.sh
sh scripts/test-wifi-backend.sh
```

The two focused strict C++11 host test suites passed. They cover accepted and duplicate starts, backend failure, connected/disconnected transitions, driver-managed background reconnection without a second `connect()` call, timeout, rollover, manager reconnect after disconnect, network-type reporting, secured and open Wi-Fi profile configuration, explicit null-passphrase forwarding and RSSI reporting.

Arduino IDE Verify passed using the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN, ESP32 core 3.3.12 and esptool 5.3.1. The build generated the ESP32-S3 application image and merged 16 MB flash image. Reported footprint: 1,168,551 bytes program storage (37% of 3,145,728 bytes); 97,476 bytes global dynamic memory (29% of 327,680 bytes), leaving 230,204 bytes. These are compile-time footprint figures, not runtime heap/PSRAM measurements.

Firmware was uploaded to the Waveshare ESP32-S3 Touch LCD 2. The upload log records successful writes and `Hash of data verified` for the bootloader, partition table, boot-app image and 1,168,704-byte application image, followed by hard reset via RTS. The user confirmed the physical tests passed after upload (2026-11-25).

Still open as distinct follow-up validation:
- Controlled unavailable-AP test confirming the existing 20-second fallback/error behaviour. This call path still blocks while the compatibility adapter is in use.
- Physical test of Wi-Fi loss followed by a driver-managed background reconnection, confirming the manager returns to `Connected` without another `connect()` call.
- Runtime internal free-heap/largest-block and PSRAM measurements; these are not represented by compile-time program/global-memory figures.

## 6. Remaining implementation sequence

1. **Persistent Wi-Fi profiles:** add a versioned Preferences/NVS store with bounded profile count, safe credential replacement, preferred profile and forget semantics.
2. **On-device network management:** scan and select networks, password entry, save/forget controls and clear connection/error feedback.
3. **Manager-owned reconnect policy:** introduce bounded backoff, link-loss recovery and a defined preferred-network policy using saved profiles. Driver-managed recovery is already observed by the manager, but this does not replace an app-level retry policy.
4. **Network capability boundary:** move the remaining Wi-Fi scan/BSSID operations behind a platform-owned capability and review whether HTTP request transport should be abstracted separately from connectivity state.

Keep each slice behaviour-preserving and independently testable. Keep passwords out of logs, serial diagnostics and tracked files.


## 7. Merge and device validation record

PR #52 was merged into `main` on 2026-11-25 at `bc1948019c31ce466ea97be34f205373d05c7954`.

- Both strict C++11 host suites passed.
- Arduino IDE Verify passed with ESP32 core 3.3.12 and esptool 5.3.1 using the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN.
- Build footprint: 1,168,551 bytes program storage (37% of the 3,145,728-byte application partition); 97,476 bytes global dynamic memory (29% of 327,680 bytes), leaving 230,204 bytes.
- Upload wrote the bootloader, partition table, boot-app image and 1,168,704-byte application image. Each write reported `Hash of data verified`; upload ended with hard reset via RTS.
- The user confirmed the physical tests passed after upload.

No new runtime free-heap, largest-block or PSRAM snapshot was supplied at the PR #52 checkpoint. Do not infer one from its program/global-memory build report. The controlled unavailable-AP and driver-managed reconnect tests are recorded as passed for the PR #53 follow-up below; runtime memory sampling remains outstanding.


## 8. Follow-up slice 2 — state-driven Weather startup and refresh

**Branch:** `phase2/02-nonblocking-weather-startup`  
**Status:** Firmware and physical-device validation passed (user-confirmed 2026-11-25); PR #53 remains open and unmerged.

The Weather application no longer calls a synchronous connection helper. `WeatherApp::begin()` binds its hooks, draws the connection status and requests a connection start, then returns. `WeatherApp::update()` advances the lifecycle through explicit states:

```text
Connecting
  -> ResolvingLocation
  -> FetchingCurrentWeather
  -> FetchingForecast
  -> PreparingStartupAssets
  -> Ready
```

The update path checks the platform connection state. While the manager remains in `Connecting`, Weather continues to run its input, time-sync and clock hooks on subsequent loop passes. Once connected or the manager reports that the attempt has ended (failure/timeout), the app advances. A failed association uses configured fallback coordinates and does not start another IP-geolocation request without a network.

Periodic refresh uses a corresponding `RefreshConnecting`, `RefreshingCurrentWeather`, `RefreshingForecast` and `PreparingRefreshAssets` sequence. It starts at most one association attempt for the refresh; current and forecast adapters only read the manager's current connection state. Driver-managed reconnection is still observed by the manager. If a refresh connection attempt fails, current Weather retains the stale-data/error path and Forecast retains its unavailable state.

The sketch no longer contains `connectWiFi()` or a synchronous loop that polls `ConnectivityManager::update()`. The device `loop()` remains responsible for polling the manager before calling `weatherApp.update()`.

### Host validation

The strict C++11 WeatherApp state/asset-preparation harness passed with `-Wall -Wextra -Werror -pedantic`. Coverage includes:

- startup returns while connection is pending and keeps input/time hooks advancing;
- successful connection, location, current weather, forecast, asset preparation and first render order;
- failed/offline connection progresses to configured-location fallback and a stable Weather-unavailable state;
- periodic refresh waits for connection before current/forecast fetches;
- Forecast refresh skips Weather-only asset preparation;
- failed current-weather refresh can keep stale-but-available Weather visible.

### Scope limitation — not fully asynchronous networking

This slice removes the up-to-20-second Wi-Fi association wait from the application call stack. It does **not** make every subsequent operation non-blocking. The following work still runs synchronously inside its individual WeatherApp hook or update stage:

- a due TimeService / TimeAPI synchronisation request;
- Google Wi-Fi scan / geolocation and IP/reverse-geocoding work in `LocationService`;
- current-weather and hourly-forecast HTTP/TLS requests in `WeatherDataService` / `HttpRequestClient`;
- Weather icon download and asset preparation.

During one of those operations, the loop cannot process touch input until that call returns. A later request-execution slice must define bounded request ownership, completion/error delivery, cancellation and response lifetimes before attempting to make these services asynchronous or concurrent. The connection manager itself remains platform-owned; Weather does not start/stop Wi-Fi directly.

### Device validation — passed (2026-11-25)

- [x] Arduino IDE Verify passed with ESP32 core 3.3.12 / esptool 5.3.1 and the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN: `esp32:esp32:esp32s3:CDCOnBoot=cdc,UploadMode=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`.
- [x] Build output: 1,169,075 bytes program storage (37% of 3,145,728 bytes); 97,492 bytes global dynamic memory (29% of 327,680 bytes), leaving 230,188 bytes.
- [x] ESP32-S3 firmware image and merged 16 MB flash image generated. The 1,171,456-byte application image was written; bootloader, partition table, boot-app and application writes were verified, with the boot-app/application writes reporting `Hash of data verified`; upload ended with a hard reset via RTS.
- [x] User confirmed all physical tests passed on the Waveshare ESP32-S3 Touch LCD 2, including pending-association responsiveness, controlled unavailable-AP fallback, location/current-weather/forecast loading, periodic reconnect behaviour and Weather/Forecast/List/Graph/touch/scroll/gesture/tab-transition regression.

### Separate runtime-memory measurement

- [ ] Capture runtime free internal heap, largest free block and PSRAM separately. The global dynamic-memory build figure above is not a runtime heap/PSRAM snapshot; this measurement remains a separate profiling task and is not a failed firmware-validation gate.
