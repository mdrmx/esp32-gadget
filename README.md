# ESP32 Gadget

A Weather-first ESP32-S3 touch-device project, being incrementally refactored into a small application platform for the Waveshare ESP32-S3 Touch LCD 2.

## Temporary refactor progress

> **Temporary tracker:** keep this section while the Phase 1 Weather rendering and asset-cache boundaries are being extracted. Remove it once the agreed Phase 1 sequence is complete. The canonical architecture and handover remain in [PLATFORM_REFACTOR_ROADMAP.md](PLATFORM_REFACTOR_ROADMAP.md).

### Completed

- [x] **PR #46 — Weather metric-card renderer:** merged at `f760713ac496fd3db57f55ccd7763f6b98fb92cd`; focused host tests and user-confirmed Waveshare hardware regression passed.
- [x] **[PR #47 — Weather conditions renderer](https://github.com/mdrmx/esp32-gadget/pull/47):** merged at `f983535c52adeaa3cf9e303960bb8c92af5545a6`; strict C++11 host tests, Arduino Verify, upload/boot and physical Waveshare validation passed.

### Validated branch awaiting review

- [x] **PR #48 — Weather asset-cache boundary** on `phase1/21-weather-asset-cache`: `WeatherAssetCache` owns the icon identity and PNG buffer; the WeatherApp pre-render hook performs cache preparation, leaving `drawWeather()` free of HTTP requests. Both latest strict C++11 host suites passed, Arduino Verify passed, firmware uploaded and booted, and the complete physical Waveshare regression passed (user-confirmed 2026-11-16). PR #48 is ready for review but has not been merged.

### Phase 1 sequence

- [x] Shared text renderer
- [x] Header renderer
- [x] Loading / error status-screen renderer
- [x] Forecast list-item renderer
- [x] Weather metric-card renderer
- [x] Current Weather conditions composition
- [x] Weather icon asset-cache ownership and pre-render preparation
- [ ] Review any remaining Weather screen composition that still has a useful independent boundary

### Validation gate for each extraction

- [x] Focused strict C++11 host tests pass for the current head (`test-weather-asset-cache.sh` and `test-weather-app-asset-preparation.sh`).
- [x] Arduino IDE Verify passes on the recorded Waveshare ESP32-S3 configuration.
- [x] Firmware uploads and boots on the device.
- [x] Physical Weather, Forecast, icon-cache, touch, scrolling, graph and tab-transition regression passes.
- [x] Existing interactions are preserved and build / memory figures are tied to the tested firmware build.

For the detailed module boundaries, regression procedures and hardware baseline, see the [Platform Refactor Roadmap](PLATFORM_REFACTOR_ROADMAP.md) and [Phase 0 regression checklist](docs/PHASE0_REGRESSION_CHECKLIST.md).
