# ESP32 Gadget

A Weather-first ESP32-S3 touch-device project, being incrementally refactored into a small application platform for the Waveshare ESP32-S3 Touch LCD 2.

## Temporary refactor progress

> **Temporary tracker:** keep this section while the Phase 1 Weather rendering boundaries are being extracted. Remove it once the agreed Phase 1 renderer sequence is complete. The canonical architecture and handover remain in [PLATFORM_REFACTOR_ROADMAP.md](PLATFORM_REFACTOR_ROADMAP.md).

### Current work

- [x] **PR #46 — Weather metric-card renderer:** merged into `main` at `f760713ac496fd3db57f55ccd7763f6b98fb92cd`. Host tests and user-confirmed Waveshare hardware regression passed; existing interactions were preserved.
- [ ] **[PR #47 — Weather conditions renderer](https://github.com/mdrmx/esp32-gadget/pull/47) (draft):** isolate the responsive description / temperature / feels-like text stack in `src/ui/WeatherConditionsRenderer.h`. Host test and Arduino / hardware validation are pending on `phase1/20-weather-conditions-renderer`.

### Renderer extraction sequence

- [x] Shared text renderer
- [x] Header renderer
- [x] Loading / error status-screen renderer
- [x] Forecast list-item renderer
- [x] Weather metric-card renderer
- [ ] Current Weather conditions composition
- [ ] Review the remaining Weather screen composition and asset-cache boundary

### Validation gate for each renderer change

- [ ] Focused strict C++11 host test passes.
- [ ] Arduino IDE Verify passes on the recorded Waveshare ESP32-S3 configuration.
- [ ] Firmware uploads and boots on the device.
- [ ] Physical Weather, Forecast, touch, scrolling, graph and tab-transition regression passes.
- [ ] Existing interactions are preserved and any build / memory figures are tied to the exact tested revision.

For the detailed module boundaries, regression procedures and hardware baseline, see the [Platform Refactor Roadmap](PLATFORM_REFACTOR_ROADMAP.md) and [Phase 0 regression checklist](docs/PHASE0_REGRESSION_CHECKLIST.md).
