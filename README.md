# ESP32 Gadget

A Weather-first ESP32-S3 touch-device project, being incrementally refactored into a small application platform for the Waveshare ESP32-S3 Touch LCD 2.

## Temporary refactor progress

> **Temporary tracker:** keep this section while the Phase 1 Weather rendering and asset-cache boundaries are being extracted. Remove it once the agreed Phase 1 sequence is complete. The canonical architecture and handover remain in [PLATFORM_REFACTOR_ROADMAP.md](PLATFORM_REFACTOR_ROADMAP.md).

### Completed

- [x] **PR #46 — Weather metric-card renderer:** merged at `f760713ac496fd3db57f55ccd7763f6b98fb92cd`; focused host tests and user-confirmed Waveshare hardware regression passed.
- [x] **[PR #47 — Weather conditions renderer](https://github.com/mdrmx/esp32-gadget/pull/47):** merged at `f983535c52adeaa3cf9e303960bb8c92af5545a6`; strict C++11 host tests, Arduino Verify, upload/boot and physical Waveshare validation passed.

### Completed on main

- [x] **[PR #48 — Weather asset-cache boundary](https://github.com/mdrmx/esp32-gadget/pull/48):** merged into `main` at `e59a8e508670959e99115c33dbff38c200503e9a`. Both strict C++11 host suites passed, Arduino Verify passed, firmware uploaded and booted, and the complete physical Waveshare regression passed (user-confirmed 2026-11-16).
- [x] **[PR #49 — Forecast graph renderer](https://github.com/mdrmx/esp32-gadget/pull/49):** merged into `main` at `90311b142993465c37c6460b2a79ff56ac136c25`. Strict C++11 host tests, Arduino IDE Verify, firmware upload/boot and full physical Waveshare graph/List/Weather regression passed (user-confirmed 2026-11-17). Verified build: 1,166,767 bytes program storage (37% of the 3 MiB app partition), 97,444 bytes dynamic/global memory (29%), leaving 230,236 bytes. `ForecastGraphRenderer` owns graph visual composition; Weather retains data, scrolling, interaction and canvas lifecycle.
- [x] **[PR #50 — Weather metrics viewport renderer](https://github.com/mdrmx/esp32-gadget/pull/50):** merged into `main` at `9bcddc503ab6a0d1eb15b1d32ecba9c255db80ca`. The checked-in strict C++11 runner passed; Arduino Verify passed with 1,167,163 bytes program storage, 97,444 bytes dynamic/global memory and 230,236 bytes remaining. The user confirmed upload/boot and complete physical metric viewport regression, with all existing interactions preserved. `WeatherMetricsViewportRenderer` owns metric viewport row selection, placement, background fill and card delegation; Weather retains formatting, canvas lifecycle, scroll/animation and composition.

### Latest merged — PR #51

- **[PR #51 — Forecast List viewport renderer](https://github.com/mdrmx/esp32-gadget/pull/51):** merged into `main` at `1763b30cc77a9ab0e04919ae5e4cc2f1ce4e7276`. `ForecastListViewportRenderer` owns List-mode visible-row selection, positioning, neighbouring-row clipping and the settled direct fallback, reusing `ForecastListItemRenderer`. Weather retains forecast data/formatting, List/Graph selection, scroll and gesture policy, canvas lifecycle, Graph composition, controls and transitions. The PR description left the repository host test, Arduino Verify and upload/boot/physical regression unchecked at merge; confirm these against the merged `main` revision before treating PR #51 as fully validated. No build/memory figures are recorded for PR #51.

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
- [x] Forecast List viewport renderer — extracted and merged in PR #51 (post-merge validation still to confirm)

### PR #50 validation record (complete; merged)

- [x] Checked-in strict C++11 repository runner: `sh scripts/test-weather-metrics-viewport-renderer.sh` → `WeatherMetricsViewportRenderer tests passed.`
- [x] Arduino IDE Verify passed using ESP32 core 3.3.12 and the recorded Waveshare ESP32-S3 16 MB / OPI PSRAM FQBN.
- [x] Verify build: 1,167,163 bytes program storage (37% of 3 MiB app partition), 97,444 bytes dynamic/global memory (29%), leaving 230,236 bytes.
- [x] User confirmed upload/boot and physical metric viewport regression: metric ordering/formatting, partial-card clipping, drag/overscroll/snap, canvas/direct fallback and tab transitions; all existing interactions remained intact.
- [x] Merged into `main` at `9bcddc503ab6a0d1eb15b1d32ecba9c255db80ca`.

### PR #51 validation record (merged; results not recorded)

- [ ] Run `sh scripts/test-forecast-list-viewport-renderer.sh` with strict C++11 warnings.
- [ ] Arduino IDE Verify using Arduino IDE 2.3.10, ESP32 core 3.3.12 and the recorded Waveshare ESP32-S3 FQBN.
- [ ] Upload/boot and physically verify Forecast List row order, three-row layout, partially visible edge-card clipping, scroll drag/overscroll/snap and transition/no-canvas fallback.
- [ ] Confirm List/Graph switching, graph drawing and controls, Weather metrics, header, footer and all existing touch/gesture interactions remain unchanged.
- PR #51 is merged into `main` at `1763b30cc77a9ab0e04919ae5e4cc2f1ce4e7276`. The PR description left these checks unchecked at merge; run them against the merged revision and update this record before calling it fully validated.

### PR #49 validation record (all checks passed)

- [x] Focused strict C++11 host test passes (`sh scripts/test-forecast-graph-renderer.sh`; user-confirmed 2026-11-17).
- [x] Arduino IDE Verify passed on 2026-11-17 using the recorded Waveshare ESP32-S3 configuration (ESP32 core 3.3.12 / esptool 5.3.1); build: 1,166,767 bytes program storage and 97,444 bytes dynamic/global memory, leaving 230,236 bytes.
- [x] Firmware uploaded and successful boot confirmed by the user on 2026-11-17.
- [x] Physical graph/List/Weather, scroll, touch, gesture and tab-transition regression passed, user-confirmed 2026-11-17.
- [x] User confirmed existing Weather/Forecast interactions remain working; build / memory figures are recorded against the verified firmware build.

For the detailed module boundaries, regression procedures and hardware baseline, see the [Platform Refactor Roadmap](PLATFORM_REFACTOR_ROADMAP.md) and [Phase 0 regression checklist](docs/PHASE0_REGRESSION_CHECKLIST.md).
