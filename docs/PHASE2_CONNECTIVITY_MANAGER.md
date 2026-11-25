# Phase 2 — Connectivity Manager and Wi-Fi backend

**Status:** Connectivity slices 1–3 are merged and validated in PRs #52–#54. Slice 4 (on-device Wi-Fi management) is implemented and validated on `phase2/04-wifi-network-management`; strict C++11 scanner tests, Arduino IDE Verify, firmware upload/hash verification and user-confirmed physical Wi-Fi UI regression have passed. All five PR review threads are addressed/resolved. PR #55 is ready for review. Phase 2 overall remains in progress.  
**Base:** `main` at `f4ae999e3f37eb893dee777f9ec404997856b091`  
**Latest merged checkpoint:** PR #54 at `4ebd4a5469f20677ee8759fc1bc3f2dae531be2b`; subsequent docs-only main checkpoint `f4ae999e3f37eb893dee777f9ec404997856b091`  
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

The default connection timeout is 20 seconds, matching the previous sketch-level connection wait. Elapsed-time arithmetic is unsigned and is tested across `millis()` rollover. A timeout disconnects the backend. When the manager is no longer in `Connecting`, it mirrors the backend's live connection state, so a driver-managed reconnection returns the manager to `Connected` without another `connect()` call. This observes reconnection performed by the underlying backend/driver; it is not a manager-owned retry or backoff policy. The manager does not retain profile credentials; profile selection and credential lifetime remain outside the manager. The NVS follow-up keeps the selected profile in a device-lifetime RAM object before configuring the backend, because `WiFiBackend` borrows its credential pointers.

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

1. **On-device network management:** implemented on `phase2/04-wifi-network-management` (see Section 10). Review, Arduino IDE Verify and physical validation remain open.
2. **Manager-owned reconnect policy:** introduce bounded backoff, link-loss recovery and a defined preferred-network policy using saved profiles. Driver-managed recovery is already observed by the manager, but this does not replace an app-level retry policy.
3. **Network capability boundary:** move the remaining Wi-Fi scan/BSSID/RSSI operations behind a platform-owned capability and review whether HTTP request transport should be abstracted separately from connectivity state.
4. **Runtime memory profiling:** capture internal free heap/largest block and PSRAM under representative operation; keep these separate from compile-time build footprint.

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
**Status:** Merged in PR #53 at `7650072bc0ab4e778f97e05519ec475e8f33fba4`; firmware and physical-device validation passed (user-confirmed 2026-11-25).

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


## 9. Follow-up slice 3 — versioned Wi-Fi profile storage (merged PR #54)

**Branch:** `phase2/03-persistent-wifi-profiles`  
**Merge:** `4ebd4a5469f20677ee8759fc1bc3f2dae531be2b` into `main`  
**Status:** Implemented, host-tested and physically validated; all review threads resolved.

### Storage contract

- `WiFiProfileStore` is independent of Arduino/ESP32 APIs and receives a small storage adapter. It supports a fixed maximum of five saved profiles.
- Each profile stores a 1–32 byte SSID and either an empty passphrase for an open network or an 8–63 byte passphrase. The store identifies a profile by SSID; saving an existing SSID replaces its credentials rather than creating a duplicate.
- One versioned binary record stores the profile set and preferred-profile index. The record has an explicit little-endian header, schema version 1, fixed-size slots and CRC32 validation.
- Corrupt or unsupported records fail closed: the store remains unavailable for writes instead of silently clearing or replacing the record. Forgetting the preferred profile selects the first remaining profile; forgetting the last profile persists an intentionally empty set.
- `PreferencesWiFiProfileStorage` uses the ESP32 Preferences API and a single NVS blob key. The store changes its in-memory profile set only after the storage adapter confirms the full write.
- At boot, a missing NVS record triggers one-time import of `WIFI_SSID` / `WIFI_PASSWORD` from the ignored `secrets.h`. Existing corrupt/unsupported records are preserved and use the compile-time fallback. A valid empty store does not repopulate the fallback.
- The selected profile is copied into a device-lifetime RAM object before `WiFiBackend::configure()`, because the backend borrows the credential pointers.

### Host coverage added

`sh scripts/test-wifi-profile-store.sh` compiles two strict C++11 suites with `-Wall -Wextra -Werror -pedantic`:
- Profile-store lifecycle, first-run state, persistence across instances, credential replacement, open networks, validation bounds, profile cap, preferred-index updates, forget, failed-write behaviour, corrupt checksum and unsupported schema.
- Preferences adapter missing-key/oversized-key handling, failed begin/read/write, key/namespace selection and a profile-store round trip through the adapter.

**Validation record (2026-11-25):** the user ran `sh scripts/test-wifi-profile-store.sh`; both strict C++11 suites passed. Arduino IDE Verify passed on the Waveshare ESP32-S3 16 MB / OPI PSRAM target using ESP32 core 3.3.12. Build: 1,172,523 bytes program storage (37%), 98,092 bytes global dynamic memory (29%) and 229,588 bytes remaining. Firmware upload completed, all flashed image hashes were verified and the board hard-reset. Physical checks confirmed normal boot and connection using the one network/password previously held in `secrets.h`, reset-button reconnect after a few seconds, and preservation of all existing Weather/Forecast interactions.

**NVS source-isolation proof:** the user temporarily changed the compiled Wi-Fi SSID/password in `secrets.h` to deliberately invalid values, uploaded without erasing flash and observed `[WIFI] Loaded the saved preferred network profile.` followed by successful Google Wi-Fi positioning and OpenWeather reverse geocoding. This proves the working network connection came from the saved NVS profile rather than the compiled fallback. The original `secrets.h` was restored and the normal firmware uploaded again.

**Automated review follow-up:** all five inline review threads were answered and resolved. The adapter opens Preferences read-only for reads and returns failure if stored bytes exceed the destination capacity. Both `getBytes()` and `putBytes()` in the host Preferences stub reject overlong key names before copying. The user reran `sh scripts/test-wifi-profile-store.sh` after these fixes; both suites passed. The latest uploaded firmware also reported `[WIFI] Loaded the saved preferred network profile.`

**Security limitation:** this implementation does not enable NVS encryption or ESP32 flash encryption. It stores Wi-Fi passphrases in the device's NVS partition; confidentiality at rest depends on the board's flash-encryption configuration. No credentials are printed to serial or included in tracked configuration files.

**Not included:** Wi-Fi scan/selection/password-entry UI, manager-owned retry/backoff, profile roaming policy and a cellular backend. Those remain separate follow-up slices.

## 10. Follow-up slice 4 — on-device Wi-Fi network management

**Branch:** `phase2/04-wifi-network-management`  
**Base:** `main` at `f4ae999e3f37eb893dee777f9ec404997856b091`  
**Status:** Implemented and validated on the feature branch; strict C++11 scanner tests, Arduino IDE Verify, firmware upload/hash verification and user-confirmed Waveshare hardware regression pass. All five PR review threads are addressed/resolved. PR #55 is ready for review.

### User-facing flow

- A `WIFI` control is drawn in the existing header on the Weather and Forecast screens. Tapping it opens a modal settings flow; existing swipe, scroll and forecast controls remain in the Weather touch path when the modal is closed.
- The saved-network screen lists up to the five profiles supported by NVS and marks the preferred and currently selected profile. Each profile opens a detail screen with Connect and Forget actions. Forget requires a second explicit confirmation.
- Scan runs asynchronously through `WiFi.scanNetworks(true, false, false, 120)`; the cooperative device loop continues to process clock, connectivity and touch updates while the driver scans. The scanner copies at most 24 visible results into fixed storage, sorts strongest RSSI first, deduplicates repeated SSIDs and releases the driver scan buffer after copying.
- The result list is paginated four networks at a time. Open networks can be saved and selected directly. Supported WPA personal networks use a touchscreen password keyboard (masked input, letters, digits and symbols) and are saved before association. WPA-Enterprise, WEP, WAPI and other unrecognised security modes are shown as unsupported rather than attempting a connection. Hidden SSIDs are not listed.
- A successful connection makes the selected profile preferred. A failed attempt to change away from a currently preferred network triggers a reconnect attempt to the previous preferred profile. A newly added first profile that cannot connect is removed so it does not become the boot-time preferred network.
- Weather's staged fetch/render workflow is paused while the settings surface is open, but Weather input, time synchronisation, header-clock updates and the platform connectivity/scanner state machines continue. After a successful network change, closing settings requests a Weather refresh through `WeatherApp` rather than calling Weather services from the UI.

### Boundaries and limitations

- `WiFiNetworkScanner.h` owns bounded result ordering and duplicate handling behind a platform-neutral scan interface. `Esp32WiFiScanBackend.h` contains the Arduino/ESP32 Wi-Fi API calls. `WiFiSettingsRenderer.h` owns the settings-screen drawing; profile persistence, touch hit-testing and connection lifecycle remain at the sketch composition boundary for this first UI slice.
- The existing `WiFiProfileStore` still caps storage at five profiles. The scan UI may display up to 24 discovered SSIDs over multiple pages.
- The new scanner path is asynchronous. If the settings screen closes mid-scan, the scan is allowed to complete in the background because the Arduino API does not expose cancellation; the result buffer is copied and released before another scan begins. The existing Google Wi-Fi geolocation scan in `LocationService` is unchanged and remains a separate synchronous operation; moving Wi-Fi scan/BSSID/RSSI behind a shared platform capability is still follow-up work.
- Manager-owned retry/backoff and roaming between preferred/saved profiles are not introduced here. Driver-managed reconnect observation remains as before.
- Passwords are masked in the UI and are never written to serial diagnostics. The existing NVS encryption limitation remains: this code does not enable NVS or ESP32 flash encryption.
- Networks requiring enterprise authentication, certificates, WEP/WAPI or hidden SSIDs cannot be added through this UI.

### Validation

- `WiFiNetworkScanner` has a strict C++11 host runner at `sh scripts/test-wifi-network-scanner.sh` (`-Wall -Wextra -Werror -pedantic`). Coverage includes asynchronous scan state, retry after failures, strongest-signal sorting, same-SSID deduplication, the 24-result cap and clearing results.
- The scanner implementation and corresponding test cases passed a strict C++11 compile/run (`-Wall -Wextra -Werror -pedantic`) after correcting the expected ordering to match strongest-first RSSI sorting. The checked-in shell runner provides the same repeatable command for a full repository checkout.
- Automated review follow-up: all five inline review threads were answered and resolved. The settings enums/state and scanner/renderer instances are declared with their required includes; all public settings draw methods guard a null display target; the selected SSID buffer is explicitly null-terminated after copying.
- **Arduino IDE Verify and upload (user-supplied build log, 2026-11-25):** passed with ESP32 core 3.3.12 / esptool 5.3.1 and the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN. Build reported 1,195,063 bytes program storage (37% of the 3,145,728-byte application partition), 99,844 bytes global dynamic memory (30%), leaving 227,836 bytes. The ESP32-S3 image and merged 16 MB flash image were generated; the application write completed with `Hash of data verified`, followed by a hard reset via RTS.
- **Physical validation (user-confirmed 2026-11-25):** all hardware tests passed. The user confirmed the on-device Wi-Fi UI is interactive and every element within it works; a follow-up confirmed that scanning discovers other networks and the device successfully joins them. This closes the device-validation gate for this slice; retain the user's confirmation as the evidence record rather than inferring individual unreported measurements.
- Existing Wi-Fi UI scope remains limited to open and supported WPA personal networks; WPA-Enterprise, WEP, WAPI, hidden SSIDs and unknown security modes remain unsupported. The NVS encryption limitation is unchanged.

