# Phase 4 — Application Manager and lifecycle

**Status:** First lifecycle slice implemented on `phase4/01-application-manager-lifecycle`; launcher UI and a second application are not part of this slice.  
**Base:** PR #61 merge `451f3d158a892b00513fca462c24fa3ba3e2bc8b`.  
**Goal:** Give the platform one bounded, transport- and hardware-neutral way to register, start, update, switch and stop applications.

## Ownership

- `Application` (`src/apps/Application.h`) is the small lifecycle contract: stable id, `begin()`, `update()` and `end()`.
- `ApplicationManager` (`src/apps/ApplicationManager.h`) stores non-owning application pointers in a fixed-capacity array of four entries. It performs no heap allocation and does not own or initialise display, touch, connectivity, location, time or storage services.
- The sketch remains the composition root. It constructs platform services and application instances, configures each application's borrowed callbacks, registers applications and starts the initial app.
- `WeatherApp` implements `Application`. Its state machine still owns Weather startup/refresh sequencing; Weather-specific data, view state and rendering stay in their existing modules.

## Lifecycle semantics

1. `registerApplication()` rejects empty/null ids, duplicate ids and registrations beyond the fixed capacity. Registration does not transfer ownership.
2. `start(id)` starts an application only when none is active. Re-requesting the active id is idempotent; attempting to start a different id requires `switchTo()`.
3. `update()` advances only the active application's `update()`. With no active app it is a no-op.
4. `switchTo(id)` validates the destination before stopping the current app. It calls the outgoing app's `end()`, then the destination's `begin()`. If destination startup fails, the manager attempts to restart the previous app. If restoration also fails, no app is active.
5. `stopActive()` ends app-owned work but leaves all platform-owned services alive.

An application implementation must leave itself inactive when `begin()` returns false. Registered application objects and platform services must outlive the manager's registrations.

## Weather integration

- Setup builds the existing `WeatherApp::Hooks`, binds them once with `configure()`, registers Weather and starts it through the manager.
- The device loop continues to update platform connectivity and Wi-Fi settings before calling `ApplicationManager::update()`.
- Weather retains the same staged connection/location/current-weather/forecast/asset/render sequence.
- On `end()`, Weather becomes inactive and no longer processes input, time or refresh work. It retains its platform hook bindings so the manager can reactivate the same instance without rebuilding shared services.
- Returning to Weather starts its normal startup sequence again. `WeatherDataService`, `WeatherViewState` and `WeatherAssetCache` remain platform-composed objects, so their existing data/view/cache lifetimes are not destroyed by an app switch. This slice does not promise preservation of WeatherApp's in-progress refresh stage or refresh timer across a switch.

## Scope deliberately deferred

- No user-facing app launcher or new tab/menu is added yet.
- No second application is registered.
- Remaining sketch-owned Weather aliases and presentation/input state are not all moved in this slice.
- No background suspension policy for future apps beyond the manager's active-only `update()` dispatch is implemented.
- No dynamic loading, heap-owned app registration, FreeRTOS task model or asynchronous HTTP conversion is introduced.

## Host validation

Run:

```sh
sh scripts/test-application-manager.sh
```

The user confirmed the checked-in strict C++11 runner passes after the test-double callback was corrected to match the `void (*)()` hook contract (the test callback now returns `void`). Result: `ApplicationManager tests passed.` Coverage includes fixed-capacity registration, duplicate/invalid ids, changing a registered id to null, start/idempotency, active-only update, switch/stop, unknown targets, failed-start restoration and Weather's configure/start/stop/restart integration.

## Firmware and device validation (2026-11-26)

- Arduino IDE compile and upload passed using ESP32 core 3.3.12 / esptool 5.3.1 and the Waveshare ESP32-S3 Touch LCD 2 FQBN (`esp32:esp32:esp32s3:CDCOnBoot=cdc,UploadMode=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`).
- Build footprint: 1,204,579 bytes program storage (38% of the 3,145,728-byte app partition) and 100,788 bytes global dynamic memory (30% of 327,680 bytes), leaving 226,892 bytes.
- The uploaded image was written and its flash hash verified; the board hard-reset via RTS.
- The user confirmed the full physical Weather regression passed with no issues noted.
- Physical switching between multiple applications remains deferred because this slice registers only Weather and does not include a launcher or second application.
