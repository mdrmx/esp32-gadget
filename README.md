# ESP32 Gadget

A Weather-first ESP32-S3 touch-device project, being incrementally refactored into a small application platform for the Waveshare ESP32-S3 Touch LCD 2.

## Current refactor progress

> **Progress snapshot:** Weather rendering and asset-cache extraction is merged through PR #51. Phase 2 connectivity PRs #52–#56 are merged into `main`; PR #56 merged at `32a9e3908b5af96759590c94cd8a3a123a1ac223`. It completes bounded manager-owned retry/recovery, 30-second saved-profile availability refresh, immediate scans on connection edges and retained last-completed availability snapshots. The user confirmed all three strict C++11 host suites (`test-connectivity-manager`, `test-wifi-availability-refresh-policy`, `test-wifi-availability-snapshot`) and the full physical regression passed. Final Arduino Verify/upload (2026-11-25, ESP32 core 3.3.12 / esptool 5.3.1, Waveshare ESP32-S3 16 MB / OPI PSRAM) reports 1,202,271 bytes program storage (38%), 100,740 bytes global dynamic memory (30%) and 226,940 bytes remaining; the application image was hash-verified and the board hard-reset via RTS. These are build figures, not runtime heap/PSRAM measurements. The five-profile NVS store does not enable NVS/flash encryption. The current next slice is [PR #57 — Google Wi-Fi geolocation scan capability](https://github.com/mdrmx/esp32-gadget/pull/57), on `phase2/06-wifi-geolocation-scan-capability`: a platform-neutral scanner and ESP32 adapter replace direct scan/BSSID/RSSI/channel calls in `LocationService`. Its strict C++11 host runner was recorded passing before the final workspace-buffer refinement; a post-refinement rerun remains to be recorded. Arduino IDE Verify/upload and the user's general physical regression passed on 2026-11-26 (1,203,379 bytes program storage, 100,748 bytes global dynamic memory, 226,932 bytes remaining; flash hashes verified and board reset via RTS). Device fault-injection coverage of Google → IP → configured-coordinate fallback remains unverified because no mechanism is available to force provider failures. HTTP/TLS transport review, remaining app-level Wi-Fi assumptions and runtime memory/PSRAM profiling remain open. The canonical architecture and handover are in [PLATFORM_REFACTOR_ROADMAP.md](PLATFORM_REFACTOR_ROADMAP.md).

### Completed

- [x] **PR #46 — Weather metric-card renderer:** merged at `f760713ac496fd3db57f55ccd7763f6b98fb92cd`; focused host tests and user-confirmed Waveshare hardware regression passed.
- [x] **[PR #47 — Weather conditions renderer](https://github.com/mdrmx/esp32-gadget/pull/47):** merged at `f983535c52adeaa3cf9e303960bb8c92af5545a6`; strict C++11 host tests, Arduino Verify, upload/boot and physical Waveshare validation passed.

### Completed on main

- [x] **[PR #48 — Weather asset-cache boundary](https://github.com/mdrmx/esp32-gadget/pull/48):** merged into `main` at `e59a8e508670959e99115c33dbff38c200503e9a`. Both strict C++11 host suites passed, Arduino Verify passed, firmware uploaded and booted, and the complete physical Waveshare regression passed (user-confirmed 2026-11-16).
- [x] **[PR #49 — Forecast graph renderer](https://github.com/mdrmx/esp32-gadget/pull/49):** merged into `main` at `90311b142993465c37c6460b2a79ff56ac136c25`. Strict C++11 host tests, Arduino IDE Verify, firmware upload/boot and full physical Waveshare graph/List/Weather regression passed (user-confirmed 2026-11-17). Verified build: 1,166,767 bytes program storage (37% of the 3 MiB app partition), 97,444 bytes dynamic/global memory (29%), leaving 230,236 bytes. `ForecastGraphRenderer` owns graph visual composition; Weather retains data, scrolling, interaction and canvas lifecycle.
- [x] **[PR #50 — Weather metrics viewport renderer](https://github.com/mdrmx/esp32-gadget/pull/50):** merged into `main` at `9bcddc503ab6a0d1eb15b1d32ecba9c255db80ca`. The checked-in strict C++11 runner passed; Arduino Verify passed with 1,167,163 bytes program storage, 97,444 bytes dynamic/global memory and 230,236 bytes remaining. The user confirmed upload/boot and complete physical metric viewport regression, with all existing interactions preserved. `WeatherMetricsViewportRenderer` owns metric viewport row selection, placement, background fill and card delegation; Weather retains formatting, canvas lifecycle, scroll/animation and composition.

### Latest merged — PR #51

- **[PR #51 — Forecast List viewport renderer](https://github.com/mdrmx/esp32-gadget/pull/51):** merged into `main` at `1763b30cc77a9ab0e04919ae5e4cc2f1ce4e7276` and fully validated after merge. `ForecastListViewportRenderer` owns List-mode visible-row selection, positioning, neighbouring-row clipping and the settled direct fallback, reusing `ForecastListItemRenderer`. Weather retains forecast data/formatting, List/Graph selection, scroll and gesture policy, canvas lifecycle, Graph composition, controls and transitions. The strict C++11 host runner passed; Arduino IDE Verify passed; the user confirmed firmware upload/boot and the complete physical Waveshare regression, with all existing interactions preserved. Latest Verify build: 1,167,139 bytes program storage (37%), 97,444 bytes global dynamic memory (29%), 230,236 bytes remaining. The PR description's unchecked-at-merge boxes are superseded by these post-merge results.

### Phase 1 sequence

- [x] Shared text renderer
- [x] Header renderer
- [x] Loading / error status-screen renderer
- [x] Forecast list-item renderer
- [x] Weather metric-card renderer
- [x] Current Weather conditions composition
- [x] Weather icon asset-cache ownership and pre-render preparation
- [x] Forecast graph renderer — extracted, fully validated and merged into `main` in PR #49
- [x] Weather metrics viewport renderer — extracted and fully validated in merged PR #50
- [x] Forecast List viewport renderer — extracted, host-tested, firmware-verified and physically validated after merge in PR #51

### PR #50 validation record (complete; merged)

- [x] Checked-in strict C++11 repository runner: `sh scripts/test-weather-metrics-viewport-renderer.sh` → `WeatherMetricsViewportRenderer tests passed.`
- [x] Arduino IDE Verify passed using ESP32 core 3.3.12 and the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN.
- [x] Verify build: 1,167,163 bytes program storage (37% of 3 MiB app partition), 97,444 bytes dynamic/global memory (29%), leaving 230,236 bytes.
- [x] User confirmed upload/boot and physical metric viewport regression: metric ordering/formatting, partial-card clipping, drag/overscroll/snap, canvas/direct fallback and tab transitions; all existing interactions remained intact.
- [x] Merged into `main` at `9bcddc503ab6a0d1eb15b1d32ecba9c255db80ca`.

### PR #51 validation record (complete; merged)

- [x] Checked-in strict C++11 repository runner: `sh scripts/test-forecast-list-viewport-renderer.sh` → `ForecastListViewportRenderer tests passed.` (user-confirmed 2026-11-25).
- [x] The companion Weather metrics viewport runner also passes on merged `main`: `sh scripts/test-weather-metrics-viewport-renderer.sh` → `WeatherMetricsViewportRenderer tests passed.`
- [x] Arduino IDE Verify passed using ESP32 core 3.3.12, esptool 5.3.1 and the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN.
- [x] Verify build: 1,167,139 bytes program storage (37% of 3,145,728 bytes); 97,444 bytes global dynamic memory (29%), leaving 230,236 bytes.
- [x] ESP32-S3 firmware image and merged 16 MB flash image generated successfully. The Verify log reused cached object files; it completed the link and image-generation stages.
- [x] User confirmed firmware upload/boot and physical Waveshare regression passed, including Forecast List row order/layout, partial-edge clipping, scroll/overscroll/snap, List/Graph and Weather/Forecast switching, direct fallback, controls, graph, header/footer, touch/gesture behaviour and tab transitions; all existing interactions remained intact (2026-11-25).
- [x] Merged into `main` at `1763b30cc77a9ab0e04919ae5e4cc2f1ce4e7276`.

**Validation note:** the PR description left several checks unchecked at merge. The post-merge results above supersede those unchecked boxes. Program-storage/global-memory figures are build output, not a new runtime heap/PSRAM snapshot.

### PR #49 validation record (all checks passed)

- [x] Focused strict C++11 host test passes (`sh scripts/test-forecast-graph-renderer.sh`; user-confirmed 2026-11-17).
- [x] Arduino IDE Verify passed on 2026-11-17 using the recorded Waveshare ESP32-S3 configuration (ESP32 core 3.3.12 / esptool 5.3.1); build: 1,166,767 bytes program storage and 97,444 bytes dynamic/global memory, leaving 230,236 bytes.
- [x] Firmware uploaded and successful boot confirmed by the user on 2026-11-17.
- [x] Physical graph/List/Weather, scroll, touch, gesture and tab-transition regression passed, user-confirmed 2026-11-17.
- [x] User confirmed existing Weather/Forecast interactions remain working; build / memory figures are recorded against the verified firmware build.

For the detailed module boundaries, regression procedures and hardware baseline, see the [Platform Refactor Roadmap](PLATFORM_REFACTOR_ROADMAP.md) and [Phase 0 regression checklist](docs/PHASE0_REGRESSION_CHECKLIST.md).


## Phase 2 — Shared connectivity (PRs #52–#56 merged)

The first implementation slice was developed on [`phase2/01-connectivity-manager-wifi-backend`](https://github.com/mdrmx/esp32-gadget/tree/phase2/01-connectivity-manager-wifi-backend) and merged into `main` in [PR #52](https://github.com/mdrmx/esp32-gadget/pull/52) at `bc1948019c31ce466ea97be34f205373d05c7954`.

- Adds a transport-neutral `ConnectivityBackend` contract and `NetworkType`.
- Adds an ESP32 `WiFiBackend` and a `ConnectivityManager` with non-blocking start/update calls, a 20-second connection timeout, link-loss detection and awareness of backend/driver background reconnection. Wi-Fi profiles with null or empty passwords retain `nullptr` for open-network association.
- Adds strict C++11 host runners:
  - `sh scripts/test-connectivity-manager.sh`
  - `sh scripts/test-wifi-backend.sh`

Both focused strict C++11 host suites passed. Arduino IDE Verify passed using ESP32 core 3.3.12 / esptool 5.3.1; the ESP32-S3 application image and merged 16 MB flash image were generated. Build footprint: 1,168,551 bytes program storage (37%), 97,476 bytes global dynamic memory (29%), leaving 230,204 bytes. The upload log confirms bootloader, partition table and firmware were written and hash-verified, followed by hard reset via RTS. The user confirmed the physical tests passed.

The second slice was merged from [`phase2/02-nonblocking-weather-startup`](https://github.com/mdrmx/esp32-gadget/tree/phase2/02-nonblocking-weather-startup) in [PR #53](https://github.com/mdrmx/esp32-gadget/pull/53):

- `WeatherApp` now advances through connection, location, current-weather, forecast, asset-preparation and ready states from its regular `update()` call.
- Periodic refresh uses a separate state sequence and waits for a pending connection without blocking the application loop.
- Removed `connectWiFi()` and its synchronous 20-second polling loop; fetch adapters now consume connectivity state rather than independently starting Wi-Fi.
- The strict C++11 WeatherApp state/asset-preparation host harness passed.

**Firmware/device validation (2026-11-25):** Arduino IDE Verify passed using ESP32 core 3.3.12 / esptool 5.3.1 and the Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN. Build: 1,169,075 bytes program storage, 97,492 bytes global dynamic memory and 230,188 bytes remaining. The 1,171,456-byte application image was written and hash-verified; bootloader, partition and boot-app writes were verified and the board hard-reset via RTS. The user confirmed all physical regression tests passed. Runtime free-heap/largest-block/PSRAM sampling remains outstanding and is not inferred from the build figures.

**Remaining limitation:** the connection wait itself no longer blocks touch/input scheduling. Google Wi-Fi geolocation scanning, due TimeService/TimeAPI sync requests, current/forecast HTTP requests and Weather icon asset preparation remain synchronous when their hooks/stages run. PRs #52–#56 are merged, including the scan/selection/password UI (PR #55) and retry/recovery policy (PR #56). PR #57 moves the Google scan/BSSID/RSSI/channel capability behind an adapter but deliberately does not make it asynchronous.

**Remaining Phase 2 work:** review the Wi-Fi-specific `HttpRequestClient` transport boundary; remove remaining app-level Wi-Fi assumptions; record PR #57 device validation (Verify/upload and general physical regression passed; provider-failure fallback paths remain unverified because device fault injection is unavailable); and capture runtime memory/PSRAM measurements.

**Saved-profile availability freshness (merged PR #56):** The Profiles screen refreshes availability immediately on entry and every 30 seconds while visible. Failed scans retry after 5 seconds, and scans remain asynchronous/non-overlapping. Link loss/recovery while Profiles remains open forces an immediate refresh. `WiFiAvailabilitySnapshot` retains the last successful SSID set while scanning or after scan failure, so badges do not switch to CHECKING/UNKNOWN mid-refresh; they update only after a successful scan completes. The user confirmed all three strict C++11 connectivity/availability suites and the full physical regression passed. The final Verify/upload figures for PR #56 are recorded above.

See [Phase 2 connectivity design](docs/PHASE2_CONNECTIVITY_MANAGER.md) and the [canonical roadmap](PLATFORM_REFACTOR_ROADMAP.md).


## Phase 2 — Google Wi-Fi geolocation scan capability (PR #57)

PR #57 extracts Google's access-point scan from `LocationService`. `WiFiGeolocationScanner` owns platform-neutral BSSID eligibility, deduplication and strongest-15 selection; `Esp32WiFiGeolocationScanBackend` owns the synchronous ESP32 scan/BSSID/RSSI/channel/cleanup calls. The sketch injects the scanner into `LocationService`, which no longer includes `WiFi.h` or calls the Wi-Fi driver directly. Google request construction and Google → IP → configured-coordinate fallback are preserved. The strict C++11 scanner runner passed before the final in-place workspace-buffer refinement; a post-refinement rerun remains to be recorded. Arduino IDE Verify/upload and the user's general physical regression passed on 2026-11-26. The Google/IP/configured-coordinate provider-failure paths were not forced on-device because no fault-injection mechanism is available; do not treat them as physically validated.

## Phase 2 — Persistent Wi-Fi profile store (merged PR #54)

The storage slice was merged into `main` in [PR #54](https://github.com/mdrmx/esp32-gadget/pull/54) at `4ebd4a5469f20677ee8759fc1bc3f2dae531be2b`.

- Adds `WiFiProfileStore`: five fixed profile slots, versioned binary schema, CRC32 validation, preferred profile selection, same-SSID credential replacement and forget semantics.
- Adds `PreferencesWiFiProfileStorage`, using one NVS blob key rather than multiple partially updated keys.
- On first boot only, imports the legacy `secrets.h` profile when the NVS key is absent. Corrupt or unsupported stored records are not overwritten; an intentionally empty valid store does not restore the legacy profile.
- Copies the selected profile into device-lifetime RAM before configuring `WiFiBackend`, whose interface borrows credential pointers.

**Validation record:** the user ran `sh scripts/test-wifi-profile-store.sh` before and after the final review fixes; both strict C++11 suites passed on the final PR head. Arduino IDE Verify and upload/hash verification passed on the Waveshare ESP32-S3 16 MB / OPI PSRAM target with ESP32 core 3.3.12. The recorded build used 1,172,523 bytes program storage (37%) and 98,092 bytes global dynamic memory (29%), leaving 229,588 bytes; no new footprint figures were supplied for the final review-fixed firmware. Hardware validation confirmed initial connection, reset/reconnect and preservation of all existing interactions. For the NVS source-isolation test, `secrets.h` was temporarily changed to deliberately invalid Wi-Fi credentials; Google Wi-Fi positioning and OpenWeather reverse geocoding still succeeded after the device reported `[WIFI] Loaded the saved preferred network profile.` The original `secrets.h` was restored and normal firmware uploaded again. Following the final review fixes, the latest firmware startup again reported `[WIFI] Loaded the saved preferred network profile.` All five inline review threads were answered and resolved. PR #54 was merged into `main` at `4ebd4a5469f20677ee8759fc1bc3f2dae531be2b`.

**Security limitation:** this code does not enable NVS or ESP32 flash encryption. Credential confidentiality at rest depends on the board's flash-encryption configuration. Network scan/selection/password-entry UI is the next separate slice.

## Phase 2 — On-device Wi-Fi management (merged PR #55)

Merged into `main` in [PR #55](https://github.com/mdrmx/esp32-gadget/pull/55) at `4ba2017a02eb6e614b94c0f3df79e7540ed81e12`. The user confirmed the final revision and hardware validation before merge.

The Wi-Fi manager adds saved-profile selection/connect controls; asynchronous scan results (up to 24 entries, three rows visible) with larger SSID text, lock indicators, four signal bars and touch scrolling; scan-derived Available / Unavailable tags on saved profiles; a masked-by-default password keyboard with Show / Hide and 180 ms key feedback; open-network selection; two-step Forget confirmation; and a header Wi-Fi signal icon with a five-sample averaged RSSI level or crossed-out disconnected state. Saved Profiles and Available Networks share 43 px row slots, 41 px cards and the same finger-following scroll, resisted edge elasticity, clipping and eased settle animation. The preferred/active saved profile is outlined, and its availability badge is inset 8 px from the card edge. The header Wi-Fi target opens settings from Weather/Forecast and closes the modal. A connection change updates the NVS preferred profile only after successful association; failed changes attempt to restore the previous preferred network.

The scan state machine and bounded result container are isolated in `WiFiNetworkScanner.h`; the ESP32 adapter is in `Esp32WiFiScanBackend.h`. The settings renderer is in `WiFiSettingsRenderer.h`. Weather's staged fetch/render workflow pauses while settings are open, while the device loop, clock and connectivity manager continue to update.

The scanner and RSSI-meter host tests passed under strict C++11 warnings. The preceding Wi-Fi UI revision passed Arduino IDE Verify/upload on ESP32 core 3.3.12 / esptool 5.3.1 (1,195,063 bytes program storage; 99,844 bytes global dynamic memory; 227,836 bytes remaining). This UI supports open and WPA personal security only; enterprise/WEP/WAPI/hidden networks remain unsupported. NVS/flash encryption is not enabled by this code.

**Availability snapshot follow-up (merged PR #56):** retained scan results and in-place status refresh are merged. All three strict C++11 connectivity/availability suites, final Arduino Verify/upload and full physical regression are user-confirmed passed. Final build figures: 1,202,271 program bytes, 100,740 global dynamic bytes and 226,940 bytes remaining; application hash verified and board hard-reset via RTS.

**Active connectivity follow-up:** PR #56 on `phase2/05-connectivity-retry-policy` is merged. The manager-owned retry/backoff and periodic saved-profile availability refresh are covered by all three user-confirmed strict C++11 host suites and the completed Arduino Verify/upload plus full physical regression. The current follow-up is PR #57, extracting Google Wi-Fi geolocation scanning behind a testable platform capability; see the Phase 2 design document.
