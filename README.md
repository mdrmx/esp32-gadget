# ESP32 Gadget

A Weather-first ESP32-S3 touch-device project, being incrementally refactored into a small application platform for the Waveshare ESP32-S3 Touch LCD 2.

## Current refactor progress

> **Progress snapshot:** Weather rendering/asset-cache extraction is complete through PR #51. Phase 2 connectivity slices 1–3 (PRs #52–#54) are merged and validated. [PR #54](https://github.com/mdrmx/esp32-gadget/pull/54), merged into `main` at `4ebd4a5469f20677ee8759fc1bc3f2dae531be2b`, adds a versioned five-profile Preferences/NVS store, preferred-profile handling, replace/forget semantics and first-boot migration from `secrets.h`. Both strict C++11 host suites passed after final review fixes; device validation confirmed NVS credential use. All five inline review threads were answered and resolved. [PR #55](https://github.com/mdrmx/esp32-gadget/pull/55) implements the modal on-device Wi-Fi manager with asynchronous scanning, paginated results, profile availability tags from a fresh scan, a password Show / Hide toggle, pressed-key feedback and a four-level averaged-RSSI header icon crossed out while disconnected. The five-sample RSSI helper's strict C++11 tests pass. Initial scanner/build/upload and network-join tests passed, but further hardware testing exposed a diagonal redraw artefact on scan-page navigation. The redraw fix and latest UI refinements now await fresh Verify/upload and physical revalidation; PR #55 remains in draft. The Google Wi-Fi location scan remains synchronous and separate. Neither the profile store nor this UI enables NVS/flash encryption. The canonical architecture and handover are in [PLATFORM_REFACTOR_ROADMAP.md](PLATFORM_REFACTOR_ROADMAP.md).

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


## Phase 2 — Shared connectivity (PRs #52–#54 merged)

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

**Remaining limitation:** the connection wait itself no longer blocks touch/input scheduling. LocationService geolocation, due TimeService/TimeAPI sync requests, current/forecast HTTP requests and Weather icon asset preparation remain synchronous when their hooks/stages run. PRs #52–#54 are merged; network scan/selection/password UI remains separate.

**Remaining Phase 2 work:** review and device validation of the Wi-Fi manager UI on `phase2/04-wifi-network-management`, manager-owned retry/backoff, moving Wi-Fi scan/BSSID operations out of `LocationService`, and transport-boundary review for the Wi-Fi-specific `HttpRequestClient`.

See [Phase 2 connectivity design](docs/PHASE2_CONNECTIVITY_MANAGER.md) and the [canonical roadmap](PLATFORM_REFACTOR_ROADMAP.md).


## Phase 2 — Persistent Wi-Fi profile store (merged PR #54)

The storage slice was merged into `main` in [PR #54](https://github.com/mdrmx/esp32-gadget/pull/54) at `4ebd4a5469f20677ee8759fc1bc3f2dae531be2b`.

- Adds `WiFiProfileStore`: five fixed profile slots, versioned binary schema, CRC32 validation, preferred profile selection, same-SSID credential replacement and forget semantics.
- Adds `PreferencesWiFiProfileStorage`, using one NVS blob key rather than multiple partially updated keys.
- On first boot only, imports the legacy `secrets.h` profile when the NVS key is absent. Corrupt or unsupported stored records are not overwritten; an intentionally empty valid store does not restore the legacy profile.
- Copies the selected profile into device-lifetime RAM before configuring `WiFiBackend`, whose interface borrows credential pointers.

**Validation record:** the user ran `sh scripts/test-wifi-profile-store.sh` before and after the final review fixes; both strict C++11 suites passed on the final PR head. Arduino IDE Verify and upload/hash verification passed on the Waveshare ESP32-S3 16 MB / OPI PSRAM target with ESP32 core 3.3.12. The recorded build used 1,172,523 bytes program storage (37%) and 98,092 bytes global dynamic memory (29%), leaving 229,588 bytes; no new footprint figures were supplied for the final review-fixed firmware. Hardware validation confirmed initial connection, reset/reconnect and preservation of all existing interactions. For the NVS source-isolation test, `secrets.h` was temporarily changed to deliberately invalid Wi-Fi credentials; Google Wi-Fi positioning and OpenWeather reverse geocoding still succeeded after the device reported `[WIFI] Loaded the saved preferred network profile.` The original `secrets.h` was restored and normal firmware uploaded again. Following the final review fixes, the latest firmware startup again reported `[WIFI] Loaded the saved preferred network profile.` All five inline review threads were answered and resolved. PR #54 was merged into `main` at `4ebd4a5469f20677ee8759fc1bc3f2dae531be2b`.

**Security limitation:** this code does not enable NVS or ESP32 flash encryption. Credential confidentiality at rest depends on the board's flash-encryption configuration. Network scan/selection/password-entry UI is the next separate slice.

## Phase 2 — On-device Wi-Fi management (redraw and UI refinements awaiting validation)

The current implementation is on [`phase2/04-wifi-network-management`](https://github.com/mdrmx/esp32-gadget/tree/phase2/04-wifi-network-management). It adds saved-profile selection/connect controls; asynchronous scan results (up to 24 entries, four per page); scan-derived Available / Unavailable tags on saved profiles (profile selection waits for the initial background scan to finish); a masked-by-default touchscreen password keyboard with Show / Hide and 180 ms pressed-key feedback; open-network selection; a two-step Forget action; and a header Wi-Fi signal icon that shows a five-sample averaged RSSI level or a crossed-out icon while disconnected. A connection change updates the NVS preferred profile only after successful association. Failed changes attempt to restore the previous preferred network.

The scan state machine and bounded result container are isolated in `WiFiNetworkScanner.h`; the ESP32 adapter lives in `Esp32WiFiScanBackend.h`. The settings renderer is isolated in `WiFiSettingsRenderer.h`. Weather's staged fetch/render workflow pauses while settings are open, while the device loop, clock and connectivity manager continue to update.

The scanner test cases pass in a strict C++11 host compile/run (`-Wall -Wextra -Werror -pedantic`), and `sh scripts/test-wifi-signal-meter.sh` passes strict C++11 tests for RSSI averaging, thresholds, reset and invalid values. The user-supplied Arduino log confirmed the original Wi-Fi manager implementation's Verify/upload passed on ESP32 core 3.3.12 / esptool 5.3.1 with the recorded 16 MB / OPI PSRAM Waveshare target; that earlier build reported 1,195,063 bytes program storage, 99,844 bytes global dynamic memory and 227,836 bytes remaining, and its uploaded application hash verified. Initial hardware tests confirmed scanning and joining other networks. Additional testing then exposed a diagonal redraw artefact on scan-page navigation; the renderer now stages full frames in the existing PSRAM canvas. Further UI refinements add availability tags, password reveal and touch feedback, and the averaged-strength/crossed-out header icon. The latest redraw and UI-refinement head has not yet been Arduino-verified or hardware-tested. All five existing inline review threads remain resolved. [PR #55](https://github.com/mdrmx/esp32-gadget/pull/55) remains in draft pending validation. See [the Phase 2 design and validation record](docs/PHASE2_CONNECTIVITY_MANAGER.md#10-follow-up-slice-4--on-device-wi-fi-network-management). This UI supports open and WPA personal security only; enterprise/WEP/WAPI/hidden networks remain unsupported. The existing NVS encryption limitation is unchanged.

