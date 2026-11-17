# ESP32 Gadget

A Weather-first ESP32-S3 touch-device project, being incrementally refactored into a small application platform for the Waveshare ESP32-S3 Touch LCD 2.

## Temporary refactor progress

> **Temporary tracker:** keep this section while the Phase 1 Weather rendering and asset-cache boundaries are being extracted. Remove it once the agreed Phase 1 sequence is complete. The canonical architecture and handover remain in [PLATFORM_REFACTOR_ROADMAP.md](PLATFORM_REFACTOR_ROADMAP.md).

### Completed

- [x] **PR #46 — Weather metric-card renderer:** merged at `f760713ac496fd3db57f55ccd7763f6b98fb92cd`; focused host tests and user-confirmed Waveshare hardware regression passed.
- [x] **[PR #47 — Weather conditions renderer](https://github.com/mdrmx/esp32-gadget/pull/47):** merged at `f983535c52adeaa3cf9e303960bb8c92af5545a6`; strict C++11 host tests, Arduino Verify, upload/boot and physical Waveshare validation passed.

### Completed on main

- [x] **[PR #48 — Weather asset-cache boundary](https://github.com/mdrmx/esp32-gadget/pull/48):** merged into `main` at `e59a8e508670959e99115c33dbff38c200503e9a`. Both strict C++11 host suites passed, Arduino Verify passed, firmware uploaded and booted, and the complete physical Waveshare regression passed (user-confirmed 2026-11-16).

### Current work

- [ ] **PR #49 — Forecast graph renderer** on `phase1/22-forecast-graph-renderer`: extract chart drawing into `ForecastGraphRenderer`, leaving forecast data, graph state/clamping, touch/gesture policy, animation and canvas lifecycle in Weather. `sh scripts/test-forecast-graph-renderer.sh` passed (`ForecastGraphRenderer tests passed.`) on 2026-11-17. Arduino IDE Verify also passed on 2026-11-17 with ESP32 core 3.3.12 / esptool 5.3.1: 1,166,767 bytes program storage (37% of the 3 MiB app partition), 97,444 bytes dynamic/global memory (29%), leaving 230,236 bytes. The user confirmed firmware upload/boot and full physical Waveshare graph/List/Weather regression passed on 2026-11-17. PR #49 remains open in draft pending review/readiness.

### Phase 1 sequence

- [x] Shared text renderer
- [x] Header renderer
- [x] Loading / error status-screen renderer
- [x] Forecast list-item renderer
- [x] Weather metric-card renderer
- [x] Current Weather conditions composition
- [x] Weather icon asset-cache ownership and pre-render preparation
- [ ] Forecast graph renderer
- [ ] Review any remaining Weather screen composition that still has a useful independent boundary

### Validation gate for PR #49

- [x] Focused strict C++11 host test passes (`sh scripts/test-forecast-graph-renderer.sh`; user-confirmed 2026-11-17).
- [x] Arduino IDE Verify passed on 2026-11-17 using the recorded Waveshare ESP32-S3 configuration (ESP32 core 3.3.12 / esptool 5.3.1); build: 1,166,767 bytes program storage and 97,444 bytes dynamic/global memory, leaving 230,236 bytes.
- [x] Firmware uploaded and successful boot confirmed by the user on 2026-11-17.
- [x] Physical graph/List/Weather, scroll, touch, gesture and tab-transition regression passed, user-confirmed 2026-11-17.
- [x] User confirmed existing Weather/Forecast interactions remain working; build / memory figures are recorded against the verified firmware build.

For the detailed module boundaries, regression procedures and hardware baseline, see the [Platform Refactor Roadmap](PLATFORM_REFACTOR_ROADMAP.md) and [Phase 0 regression checklist](docs/PHASE0_REGRESSION_CHECKLIST.md).
