# Phase 4 — Application Manager and lifecycle

**Status:** Lifecycle manager PR #62 and launcher PR #63 are merged into `main`. The final icon-grid, paper-and-ink card treatment, shared-header and card-outline revision passed the user's full hardware regression on 2026-11-27. The expanded strict C++11 host-suite results for the final launcher revision have not yet been confirmed. The clock-ownership refinement is in progress; host and device validation for it are pending. Weather remains the only registered application; second-app integration, remaining Weather-boundary cleanup and runtime memory profiling remain outstanding.  
**Base:** PR #62 merge `bf4e0120d3be330f5b4113aa5e80bb3ff3eb9f14`; launcher PR #63 merge `0af82bf94d2419f8d59be9aecb1d13f67e5d10c`.  
**Goal:** Give the platform one bounded, transport- and hardware-neutral way to register, start, update, switch and stop applications.

## Ownership

- `Application` (`src/apps/Application.h`) is the small lifecycle contract: stable id, `begin()`, `update()` and `end()`.
- `ApplicationManager` (`src/apps/ApplicationManager.h`) stores non-owning application pointers in a fixed-capacity array of four entries. It performs no heap allocation and does not own or initialise display, touch, connectivity, location, time or storage services.
- The sketch remains the composition root. It constructs platform services and application instances, configures each application's borrowed callbacks, registers applications and starts the initial app.
- `ApplicationLauncherController` coordinates launcher open/select/return lifecycle through the manager; it owns no application instances or platform services. The visible launcher has no BACK control; the previously active app is resumed by selecting its icon again. `ApplicationLauncherRenderer` owns only the bounded launcher presentation.
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
- On `end()`, Weather becomes inactive and no longer processes Weather input or refresh work. It retains its current state-machine stage, last-refresh timestamp, fetched data and platform hook bindings, so re-entry resumes the same instance without rebuilding shared services. Time synchronisation and shared-header clock redraw are platform-loop responsibilities and continue while no app is active.
- Weather observes the platform-owned connection state; it does not start Wi-Fi association. The launcher starts the saved preferred profile at boot and `ConnectivityManager` advances/retries it from the platform loop, independently of which app is selected. Weather waits for the shared connection when starting and keeps cached data visible during offline resume/refresh paths. `WeatherDataService`, `WeatherViewState` and `WeatherAssetCache` remain platform-composed objects and are not destroyed by an app switch.

## Application launcher slice

- A quick upward swipe that starts within the centred 64 px-wide band of the bottom 24 px opens the platform launcher. On the 320 px-wide panel the activation band is x=128–191. The swipe must travel at least 52 px upward, stay within 40 px horizontal drift and complete within 650 ms. Touches starting elsewhere along the bottom edge are not claimed by this gesture, so metric-card/list interactions remain available. There is no launcher button in the header.
- The launcher renders up to four apps as a two-column, two-row icon grid. One or two registered apps are centred in the full body area below the header as a single row; three or four apps use two rows centred in that body area. Tiles are 68 px rounded squares with paper-coloured fill and a dark-ink outline. There are no app-id, OPEN, LAST USED or SELECT AN APP labels, and no BACK button. Weather uses the supplied cloudy glyph, converted to a 32×32 one-bit monochrome bitmap stored in PROGMEM and drawn in the current ink colour. Apps without artwork use a simple plus glyph until dedicated icon metadata is introduced. The shared clock and live Wi-Fi signal indicator remain visible and continue updating while the launcher is open.
- Opening the launcher records the previous app id and calls `stopActive()`; platform display, touch, connectivity, location, time and storage services remain alive.
- Selecting a tile starts that registered app through `switchTo()`. To resume the previously active app, select its icon again; the launcher has no separate BACK control. A failed app start leaves the launcher visible with a brief diagnostic message.
- The same central-band bottom-edge gesture is handled at platform level before app/modal touch processing, so the launcher can be opened from any app or platform screen, including Wi-Fi settings. It only claims contacts that begin inside the central band; ordinary metric-card/list gestures outside that band continue to their app.
- At boot, the launcher is opened with no active application, then the platform starts the saved preferred Wi-Fi profile without waiting. The launcher stays drawn while association/retry proceeds; the header signal icon changes from disconnected to connected/strength as the manager reports the link. The launcher presents only app icons and optional failure feedback; it does not show a BACK button or selection prompt.
- The platform loop calls `maybeSyncTimeFromAPI()` independently of `ApplicationManager::update()`, then refreshes the shared header clock at a one-second cadence. This ensures the boot launcher can obtain valid time while no application is active and keeps the shared clock independent of Weather lifecycle.
- The Wi-Fi header target is dispatched before the launcher's catch-all touch handler, including while the launcher itself is open. This keeps network settings reachable from the boot launcher; once open, the modal owns touch interaction and the central-band swipe returns to the launcher.

## Scope deliberately deferred

- Register and validate a second independent application, then verify launcher use and shared platform services while that app is active.
- Move remaining sketch-owned Weather aliases, presentation state and Weather-specific input fully behind the Weather boundary.
- Define more detailed state preservation and suspend/resume policy for longer-lived apps.
- Add per-application icon metadata when a second app is introduced; the current Weather icon is intentionally a small renderer-side mapping until that requirement exists.
- No dynamic loading, heap-owned app registration, FreeRTOS task model or asynchronous HTTP conversion is introduced.

## Host validation

Run both strict C++11 host suites:

```sh
sh scripts/test-application-manager.sh
sh scripts/test-application-launcher-renderer.sh
sh scripts/test-bottom-edge-swipe-gesture.sh
sh scripts/test-weather-app-asset-preparation.sh
```

For merged PR #62, the user confirmed the strict C++11 runner passes (`ApplicationManager tests passed.`), including registration, null-id safety, lifecycle switching/restoration and Weather configure/start/stop/restart integration. This branch adds manager enumeration, launcher controller, Weather cached resume and offline reconnection-path coverage. Additional suites cover the launcher renderer and bottom-edge swipe recogniser. The launcher renderer suite now checks the icon bitmap coordinates, text-free tile layout, footer-free single/two-app centring, two-row geometry and touch targets. Run all commands above after pulling the latest branch; the expanded suite results must still be recorded for this revision.



## Remaining Phase 4 validation and development

- Run and record the focused strict C++11 suites for the merged final launcher revision: application manager, launcher renderer, bottom-edge swipe gesture and Weather state/asset preparation. Hardware regression does not substitute for these host-side checks.
- Register and validate a second independent application, then verify launcher switching and shared platform-service behaviour from both apps.
- Continue moving remaining Weather-specific sketch state and compatibility aliases behind the Weather application boundary.
- Refine state-preservation and suspend/resume policy for longer-lived applications.

## Firmware and device validation

### PR #62 lifecycle baseline (2026-11-26)

- Arduino IDE compile/upload passed using ESP32 core 3.3.12 / esptool 5.3.1 and the Waveshare ESP32-S3 Touch LCD 2 FQBN (`esp32:esp32:esp32s3:CDCOnBoot=cdc,UploadMode=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`).
- Build footprint: 1,204,579 bytes program storage and 100,788 bytes global dynamic memory; uploaded image hash verified and board hard-reset via RTS.
- The user confirmed the full physical Weather regression passed.

### PR #63 base launcher firmware validation (2026-11-26)

- Arduino IDE Verify/compile and upload passed with the same Waveshare ESP32-S3 FQBN, ESP32 core 3.3.12 and esptool 5.3.1.
- Build footprint: 1,209,815 bytes program storage (38% of the 3,145,728-byte app partition) and 100,868 bytes global dynamic memory (30% of 327,680 bytes), leaving 226,812 bytes.
- The log reports 1,044,480 bytes written (730,023 compressed), successful flash-data hash verification and hard reset via RTS.
- The user confirmed launcher-first boot, background Wi-Fi, central-band swipe, launcher redraw/touch behaviour and Wi-Fi settings access for the base navigation revision.

### Final launcher visual revision — post-merge hardware regression (2026-11-27)

- PR #63 merged into `main` at `0af82bf94d2419f8d59be9aecb1d13f67e5d10c`; source head: `ca6ba974e5e473e91b1c6a0bc7c1b622f0e9e035`.
- The user confirmed the full hardware regression passed for the final icon-grid, shared-header, paper/ink card-outline and four-colour palette revision.
- This confirmation covers the final physical regression, but no separate expanded host-suite output has yet been recorded here. Run the four commands in the Host validation section and append their actual results when available.

Independent multi-app switching remains deferred because only Weather is registered. Runtime heap/largest-block/PSRAM profiling remains outstanding.


## Shared visual treatment (2026-11-27)

- Every shared header uses the same paper background as the screen. Clock text and the Wi-Fi glyph are dark ink, with no coloured rectangle behind either indicator. The horizontal header divider is a two-pixel ink rule and is restored by clock/Wi-Fi partial redraws.
- Weather metric cards, hourly list cards, Wi-Fi network rows/fields/buttons and launcher icon tiles use the paper background with a dark-ink outline. Partially clipped Weather/Forecast cards draw the visible outline edges during scrolling.
- All Weather view text, including loading/error status, conditions, metric labels and values, uses the dark ink colour. Forecast list/graph labels use ink as well.
- The List/Graph selector track uses the paper background and ink outline. The selected segment uses the lighter burnt-orange tone (`#F4AE52`); both labels remain ink.
- Graph grid/divider lines and all graph text use ink. The plotted temperature/rain series retain burnt-orange emphasis.

## E-ink-inspired OS palette (2026-11-26)

The shared UI palette is based on the four supplied colours and is applied across the launcher, Weather, Forecast, Wi-Fi settings and status screens:

| Role | Hex | RGB565 | Use |
|---|---|---:|---|
| Paper | `#FEF3E2` | `0xFF9C` | Main screen background |
| Light burnt orange | `#F4AE52` | `0xF56A` | Selected List/Graph segment |
| Burnt orange | `#D4621A` | `0xD303` | Plot emphasis and stronger accents |
| Dark brown | `#2A1A0E` | `0x28C1` | Text, icons, card outlines and dividers |

All UI roles use those four supplied colours; generic cards and panels use the paper colour, with no grey card fills or green text/status palette. PNG weather-icon transparency blends against the exact paper colour (`00BBGGRR = 0x00E2F3FE`). The Weather launcher bitmap is monochrome and flash-resident: it requires no runtime PNG decoding, network request or heap allocation.
