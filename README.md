# ESP32 Gadget

A Weather-first ESP32-S3 touch-device project, being incrementally refactored into a small application platform for the Waveshare ESP32-S3 Touch LCD 2.

## Current refactor progress

> **Progress snapshot:** the Weather rendering and asset-cache extraction sequence is complete through PR #51, including post-merge host, firmware and physical validation. Phase 2 slice 1 was merged in PR #52 at `bc1948019c31ce466ea97be34f205373d05c7954`; host suites, Arduino Verify, upload/hash verification and user-confirmed physical tests passed. Phase 2 slice 2 is implemented and device-validated on `phase2/02-nonblocking-weather-startup` in open PR #53: the strict C++11 state-machine harness, Arduino IDE Verify, upload/boot and user-confirmed physical regression passed. Verify reports 1,169,075 bytes program storage and 97,492 bytes global dynamic memory, leaving 230,188 bytes; runtime heap/PSRAM measurements remain separate and unrecorded. Wi-Fi association no longer waits synchronously in the WeatherApp call stack. LocationService geolocation, due TimeService/TimeAPI sync, HTTP/TLS requests and icon-asset preparation remain synchronous in their individual stages. Phase 1 remains in progress for the broader module-boundary/platform ownership review, and Phase 2 remains in progress for persistent Wi-Fi profiles, network UI and manager-owned retry/backoff. The canonical architecture and handover remain in [PLATFORM_REFACTOR_ROADMAP.md](PLATFORM_REFACTOR_ROADMAP.md).

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


## Phase 2 — Shared connectivity (first slice merged; second slice validated on branch)

The first implementation slice was developed on [`phase2/01-connectivity-manager-wifi-backend`](https://github.com/mdrmx/esp32-gadget/tree/phase2/01-connectivity-manager-wifi-backend) and merged into `main` in [PR #52](https://github.com/mdrmx/esp32-gadget/pull/52) at `bc1948019c31ce466ea97be34f205373d05c7954`.

- Adds a transport-neutral `ConnectivityBackend` contract and `NetworkType`.
- Adds an ESP32 `WiFiBackend` and a `ConnectivityManager` with non-blocking start/update calls, a 20-second connection timeout, link-loss detection and awareness of backend/driver background reconnection. Wi-Fi profiles with null or empty passwords retain `nullptr` for open-network association.
- Adds strict C++11 host runners:
  - `sh scripts/test-connectivity-manager.sh`
  - `sh scripts/test-wifi-backend.sh`

Both focused strict C++11 host suites passed. Arduino IDE Verify passed using ESP32 core 3.3.12 / esptool 5.3.1; the ESP32-S3 application image and merged 16 MB flash image were generated. Build footprint: 1,168,551 bytes program storage (37%), 97,476 bytes global dynamic memory (29%), leaving 230,204 bytes. The upload log confirms bootloader, partition table and firmware were written and hash-verified, followed by hard reset via RTS. The user confirmed the physical tests passed.

The second slice is implemented on [`phase2/02-nonblocking-weather-startup`](https://github.com/mdrmx/esp32-gadget/tree/phase2/02-nonblocking-weather-startup) in [PR #53](https://github.com/mdrmx/esp32-gadget/pull/53):

- `WeatherApp` now advances through connection, location, current-weather, forecast, asset-preparation and ready states from its regular `update()` call.
- Periodic refresh uses a separate state sequence and waits for a pending connection without blocking the application loop.
- Removed `connectWiFi()` and its synchronous 20-second polling loop; fetch adapters now consume connectivity state rather than independently starting Wi-Fi.
- The strict C++11 WeatherApp state/asset-preparation host harness passed.

**Firmware/device validation (2026-11-25):** Arduino IDE Verify passed using ESP32 core 3.3.12 / esptool 5.3.1 and the Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN. Build: 1,169,075 bytes program storage, 97,492 bytes global dynamic memory and 230,188 bytes remaining. The 1,171,456-byte application image was written and hash-verified; bootloader, partition and boot-app writes were verified and the board hard-reset via RTS. The user confirmed all physical regression tests passed. Runtime free-heap/largest-block/PSRAM sampling remains outstanding and is not inferred from the build figures.

**Remaining limitation:** the connection wait itself no longer blocks touch/input scheduling. LocationService geolocation, a due TimeService/TimeAPI sync request, current/forecast HTTP requests and Weather icon asset preparation are still synchronous when their hooks/stages run; converting those operations into asynchronous jobs is separate work. PR #53 remains open and unmerged.

**Remaining Phase 2 work:** persistent NVS profiles, safe migration from compile-time credentials, network scan/selection/password-entry/save/forget UI, manager-owned retry/backoff, moving Wi-Fi scan/BSSID operations out of `LocationService`, and transport-boundary review for the Wi-Fi-specific `HttpRequestClient`.

See [Phase 2 connectivity design](docs/PHASE2_CONNECTIVITY_MANAGER.md) and the [canonical roadmap](PLATFORM_REFACTOR_ROADMAP.md).
