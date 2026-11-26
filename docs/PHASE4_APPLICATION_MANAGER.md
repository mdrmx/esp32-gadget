# Phase 4 — Application Manager and lifecycle

**Status:** Lifecycle manager merged in PR #62; first launcher slice implemented on `phase4/02-application-launcher`. Weather remains the only registered application; second-app integration and launcher physical regression are pending.  
**Base:** PR #62 merge `bf4e0120d3be330f5b4113aa5e80bb3ff3eb9f14`.  
**Goal:** Give the platform one bounded, transport- and hardware-neutral way to register, start, update, switch and stop applications.

## Ownership

- `Application` (`src/apps/Application.h`) is the small lifecycle contract: stable id, `begin()`, `update()` and `end()`.
- `ApplicationManager` (`src/apps/ApplicationManager.h`) stores non-owning application pointers in a fixed-capacity array of four entries. It performs no heap allocation and does not own or initialise display, touch, connectivity, location, time or storage services.
- The sketch remains the composition root. It constructs platform services and application instances, configures each application's borrowed callbacks, registers applications and starts the initial app.
- `ApplicationLauncherController` coordinates launcher open/select/back operations through the manager; it owns no application instances or platform services. `ApplicationLauncherRenderer` owns only the bounded launcher presentation.
- `WeatherApp` implements `Application`. Its state machine still owns Weather startup/refresh sequencing; Weather-specific data, view state and rendering stay in their existing modules.

## Lifecycle semantics

1. `registerApplication()` rejects empty/null ids, duplicate ids and registrations beyond the fixed capacity. Registration does not transfer ownership.
2. `start(id)` starts an application only when none is active. Re-requesting the active id is idempotent; attempting to start a different id requires `switchTo()`.
3. `update()` advances only the active application's `update()`. With no active app it is a no-op.
4. `switchTo(id)` validates the destination before stopping the current app. It calls the outgoing app's `end()`, then the destination's `begin()`. If destination startup fails, the manager attempts to restart the previous app. If restoration also fails, no app is active.
5. `stopActive()` ends app-owned update work but leaves all platform-owned services alive. Individual apps may retain state across this inactive interval and resume it in `begin()`.

An application implementation must leave itself inactive when `begin()` returns false. Registered application objects and platform services must outlive the manager's registrations.

## Weather integration

- Setup builds the existing `WeatherApp::Hooks`, binds them once with `configure()`, registers Weather and starts it through the manager.
- The device loop continues to update platform connectivity and Wi-Fi settings before calling `ApplicationManager::update()`.
- Touch sampling now runs at platform-loop level before manager updates. Launcher input therefore remains available when the launcher has stopped the active app; Weather-specific gesture handling is dispatched only while `weather` is active.
- Weather retains the same staged connection/location/current-weather/forecast/asset/render sequence.
- On `end()`, Weather becomes inactive and no longer processes input, time or refresh work. It retains its current state-machine stage, last-refresh timestamp, fetched data and platform hook bindings, so re-entry resumes the same instance without rebuilding shared services.
- Returning to Weather resumes its prior state-machine stage and refresh schedule. If the shared platform connection is already established, Weather does not call `startConnection()` or show a false `Connecting Wi-Fi...` banner; the cached current view is redrawn immediately (or the unavailable-weather error is restored if there is no cached data). If the connection was lost while the launcher was open, Weather starts or reuses the platform connection attempt and resumes the appropriate startup/refresh path. `WeatherDataService`, `WeatherViewState` and `WeatherAssetCache` remain platform-composed objects and are not destroyed by an app switch.

## Application launcher slice

- A persistent four-square button in the top-left header opens the platform launcher. The location label is constrained to the remaining header width; the existing Wi-Fi button remains in its own hit area.
- The launcher renders up to four registered application ids as a two-column, two-row grid. Its single-line title is vertically centred; the redundant registered-app-count subtitle is omitted. The shared clock and live Wi-Fi signal indicator remain visible and continue updating while the launcher is open. It does not introduce a separate app metadata registry or allocate applications dynamically.
- Opening the launcher records the previous app id and calls `stopActive()`; platform display, touch, connectivity, location, time and storage services remain alive.
- Selecting a tile starts that registered app through `switchTo()`. BACK re-enters the app that was active before the launcher opened. A failed app start leaves the launcher visible so the user can retry or return.
- The Wi-Fi settings header includes the same launcher button, so the launcher can be opened from that platform modal too.
- Only Weather is registered today. Selecting Weather or pressing BACK therefore re-enters Weather, restores its prior in-memory workflow stage when the Wi-Fi link remains available, and redraws its cached view; this is not yet proof of switching between independent applications.

## Scope deliberately deferred

- Register and validate a second independent application, then verify launcher use and shared platform services while that app is active.
- Move remaining sketch-owned Weather aliases, presentation state and Weather-specific input fully behind the Weather boundary.
- Define more detailed state preservation and suspend/resume policy for longer-lived apps.
- Provide friendly app names/icons only when a second app makes the metadata need concrete.
- No dynamic loading, heap-owned app registration, FreeRTOS task model or asynchronous HTTP conversion is introduced.

## Host validation

Run both strict C++11 host suites:

```sh
sh scripts/test-application-manager.sh
sh scripts/test-application-launcher-renderer.sh
```

For merged PR #62, the user confirmed the strict C++11 runner passes (`ApplicationManager tests passed.`), including registration, null-id safety, lifecycle switching/restoration and Weather configure/start/stop/restart integration. The current branch extends the same runner with manager enumeration and launcher-controller coverage: open/stop, select by registration index, failed target start, BACK restoration and opening/closing with no active app. Weather-specific lifecycle tests now assert that returning while Wi-Fi is connected preserves the prior state, redraws cached data and does not call `startConnection()` or render `Connecting Wi-Fi...`; a separate offline-start case confirms connection setup still occurs when required. Neither expanded host suite has yet been run on the launcher branch; run both commands above before review.



## Launcher branch validation pending

- Run the expanded strict C++11 host runner after pulling `phase4/02-application-launcher`; the renderer assertions cover the single-line, vertically centred title and verify no availability subtitle is drawn.
- Run Arduino IDE Verify with the recorded Waveshare ESP32-S3 FQBN and ESP32 core 3.3.12.
- Upload and confirm the app grid opens from the header button, the clock and Wi-Fi indicator remain visible/live, the Weather tile and BACK resume Weather without a spurious connection attempt while online, the Wi-Fi modal can launch the grid, and no existing Weather/Forecast gestures or Wi-Fi settings interactions regress.
- Confirm the launcher remains touch-accessible while no app is active; multi-app selection remains pending until a second application is registered.

## Firmware and device validation (2026-11-26)

- Arduino IDE compile and upload passed using ESP32 core 3.3.12 / esptool 5.3.1 and the Waveshare ESP32-S3 Touch LCD 2 FQBN (`esp32:esp32:esp32s3:CDCOnBoot=cdc,UploadMode=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`).
- Build footprint: 1,204,579 bytes program storage (38% of the 3,145,728-byte app partition) and 100,788 bytes global dynamic memory (30% of 327,680 bytes), leaving 226,892 bytes.
- The uploaded image was written and its flash hash verified; the board hard-reset via RTS.
- The user confirmed the full physical Weather regression passed with no issues noted.
- These firmware figures and physical regression results are the historical PR #62 lifecycle-only baseline, not validation of the launcher branch. PR #63 adds the launcher, but its Arduino Verify/upload and physical launcher regression remain pending. Independent multi-app switching also remains deferred because only Weather is registered.
