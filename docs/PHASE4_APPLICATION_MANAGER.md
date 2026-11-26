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

The strict C++11 suite covers fixed-capacity registration, duplicate/invalid ids, start/idempotency, active-only update, switch/stop, unknown targets, failed-start restoration and Weather's configure/start/stop/restart integration. Firmware Verify/upload and physical app switching remain device-validation steps for this branch.
