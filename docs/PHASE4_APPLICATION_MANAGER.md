# Phase 4 — Application Manager and lifecycle

**Status:** Lifecycle manager PR #62 and launcher PR #63 are merged into `main`. The clock-ownership refinement was merged in [PR #64](https://github.com/mdrmx/esp32-gadget/pull/64) at `c2d2f77ab5590c2cba58c43685c1a109c3c586b2`; all four focused strict C++11 host suites passed, and the user confirmed the cold-boot, offline-to-online time recovery, launcher/Weather switching and Wi-Fi settings hardware regression passed (2026-11-27). TimeAPI synchronisation and one-second shared-header clock redraw now run independently of the active application. The current Weather UI refinement on `phase4/03-weather-header-current-conditions` changes the header identity from resolved place name to app title (`Weather`) and moves the place name into the Current Conditions content. It also rearranges the current-conditions body into a left summary and right three-card scrollable metric column. Metric, selector, viewport and graph host suites have passed; latest launcher/list-item/graph icon assertions need local reruns. Arduino Verify/upload and physical regression for this branch remain pending. Weather remains the only registered application; second-app integration, remaining Weather-boundary cleanup and runtime memory profiling remain outstanding.  
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

## Weather app identity and Current Conditions layout refinement (in progress)

- The shared header identifies the active application rather than the resolved weather location. The Weather app displays `Weather`; the clock and Wi-Fi indicator retain their shared-header positions.
- The resolved location remains visible as a title within Current Conditions, directly below the header.
- Current Conditions uses a two-column composition: the left side contains the location heading followed by the provider icon, condition description, temperature and feels-like temperature in a vertical stack; the right side retains the existing three-visible-card metric viewport and its eight metric records.
- The metric cards now render every value at the same size-2 font and keep labels at size 1. Value baselines are consistent for complete and clipped cards. Card order is Rain Chance, Wind, UV Index, then Humidity, Pressure, Cloud Cover, Visibility and Rain (1H). `WeatherMetricCatalog` owns stable metric identities, labels and card order; live values remain formatted by the Weather sketch. Wind speed drops its decimal precision at 100 km/h and above so the uniform value size fits the card width. The focused strict C++11 runners are `sh scripts/test-weather-metric-card-renderer.sh`, `sh scripts/test-weather-metrics-viewport-renderer.sh` and `sh scripts/test-weather-metric-catalog.sh`. The user confirmed all three runners passed on 2026-11-27.
- The Forecast List viewport now starts at screen y=72, immediately below the List/Graph selector's reserved control area, and ends at y=216. Its canvas is positioned at the same origin and reduced to 144 px high. The first row remains at its original screen position (y=72), and the existing clipped-card animation is retained: a card can be partially visible at the viewport edge as it moves, but nothing is drawn above y=72 and the card is skipped once it has moved completely out of view. The canvas flush therefore never touches the selector, so it cannot erase the selector border or selected segment during scroll/snap redraws. Graph rendering uses an inverse 8 px canvas-origin offset to preserve its previous on-screen plot coordinates.
- Forecast List condition icons remain at their original compact vector size. Graph uses the same vector primitives at 120% scale, positioned 4 px lower and centred over each hourly point; neither view has raster backing or transparent bitmap padding. The graph now fits seven hourly points across the 280 px plot at 46 px spacing (previously eight at 40 px), removes the middle horizontal gridline, and draws the temperature curve with a two-pixel stroke. The eighth forecast record remains a neighbouring sample for vertical-scale continuity, not a displayed hour. The Current Conditions provider PNG already uses its alpha-bound crop path.
- The metrics viewport begins at y=39 and retains its 177 px height, 54 px card height, 59 px row step, canvas-backed redraw and touch-scroll boundaries. The 2× provider icon is scanned for visible alpha bounds before rendering; transparent canvas padding is cropped away, then the crop is proportionally scaled and centred within a 70×70 px footprint. The scan uses PNGdec's alpha mask (threshold 24) and a second decode draws the crop with transparent pixels blended to the paper background. Truecolour/grayscale colour-key transparency is handled explicitly. All masks and line buffers are fixed-size stack arrays; no full-image or heap buffer is added. The icon starts at y=66 and the description at y=142. Temperature and feels-like positions are calculated from the actual rendered text heights, keeping a consistent 4 px gap between the description and temperature and between the two temperature lines, whether the description occupies one or two lines.
- The Weather and header renderer host tests have been updated for app-title and new content positioning. The user's earlier metric-card, metrics-viewport, metric-catalog, selector, Forecast List viewport and Forecast Graph host-test runs passed (2026-11-27). The current launcher restore and Graph renderer styling have changed since those runs; `sh scripts/test-application-launcher-renderer.sh`, `sh scripts/test-forecast-list-item-renderer.sh` and `sh scripts/test-forecast-graph-renderer.sh` need rerunning against this revision. Arduino Verify/upload and physical regression remain pending for this branch.

## Application launcher slice

- A quick upward swipe that starts within the centred 64 px-wide band of the bottom 24 px opens the platform launcher. On the 320 px-wide panel the activation band is x=128–191. The swipe must travel at least 52 px upward, stay within 40 px horizontal drift and complete within 650 ms. Touches starting elsewhere along the bottom edge are not claimed by this gesture, so metric-card/list interactions remain available. There is no launcher button in the header.
- The launcher renders up to four apps as a two-column, two-row icon grid. One or two registered apps are centred in the full body area below the header as a single row; three or four apps use two rows centred in that body area. Tiles are 68 px rounded squares with paper-coloured fill and a dark-ink outline. There are no app-id, OPEN, LAST USED or SELECT AN APP labels, and no BACK button. Weather uses the supplied cloudy glyph as its original 32×32 one-bit monochrome bitmap stored in PROGMEM and drawn in the current ink colour. The original source-frame proportions and transparent padding are retained; the bitmap is centred at its original size in the 68 px tile. Apps without artwork use a simple plus glyph until dedicated icon metadata is introduced. The shared clock and live Wi-Fi signal indicator remain visible and continue updating while the launcher is open.
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
- Add per-application icon metadata when a second app is introduced; the Weather icon remains a renderer-side mapping using its original 32×32 monochrome source frame.
- No dynamic loading, heap-owned app registration, FreeRTOS task model or asynchronous HTTP conversion is introduced.

## Host validation

Run both strict C++11 host suites:

```sh
sh scripts/test-application-manager.sh
sh scripts/test-application-launcher-renderer.sh
sh scripts/test-bottom-edge-swipe-gesture.sh
sh scripts/test-weather-app-asset-preparation.sh
```

Validation completed on `refinement/phase4-cleanup` (user-confirmed 2026-11-27): `sh scripts/test-application-manager.sh` → `ApplicationManager tests passed.`; `sh scripts/test-weather-app-asset-preparation.sh` → `WeatherApp state and asset preparation tests passed.`; `sh scripts/test-application-launcher-renderer.sh` → `ApplicationLauncherRenderer tests passed.`; `sh scripts/test-bottom-edge-swipe-gesture.sh` → `BottomEdgeSwipeGesture tests passed.`. The expanded coverage includes manager enumeration, launcher-controller selection/restore, cached Weather resume and offline recovery, launcher icon geometry/touch targets, and the bounded bottom-edge swipe recogniser.



## Remaining Phase 4 validation and development

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

### Clock ownership refinement — host and hardware validation (2026-11-27)

- Merged into `main` in [PR #64](https://github.com/mdrmx/esp32-gadget/pull/64) at `c2d2f77ab5590c2cba58c43685c1a109c3c586b2`.
- TimeAPI synchronisation and the one-second shared-header clock refresh are owned by the platform loop, not `WeatherApp::update()`. They continue while the boot launcher is visible and no application is active.
- All four focused strict C++11 host suites listed above passed on the final refinement branch.
- The user confirmed the physical regression passed: cold boot with the launcher left open, cold boot without Wi-Fi followed by online time recovery, repeated launcher/Weather switching, and opening/closing Wi-Fi settings while checking that the clock remains visible and updates.
- The device has no RTC; after a fresh offline boot it cannot display accurate time until TimeAPI synchronisation succeeds. This is the documented design limitation, not a regression.

Independent multi-app switching remains deferred because only Weather is registered. Runtime heap/largest-block/PSRAM profiling remains outstanding.


## Shared visual treatment (2026-11-27)

- Every shared header uses the same paper background as the screen. Clock text and the Wi-Fi glyph are dark ink, with no coloured rectangle behind either indicator. The horizontal header divider is a two-pixel ink rule and is restored by clock/Wi-Fi partial redraws.
- Weather metric cards, hourly list cards, Wi-Fi network rows/fields/buttons and launcher icon tiles use the paper background with a dark-ink outline. Partially clipped Weather/Forecast cards draw the visible outline edges during scrolling.
- All Weather view text, including loading/error status, conditions, metric labels and values, uses the dark ink colour. Forecast list/graph labels use ink as well.
- The List/Graph selector track uses the paper background and ink outline. The selected segment uses the lighter burnt-orange tone (`#F4AE52`); both labels remain ink. The selected segment is inset uniformly by one pixel inside the track: the track is 137×26 px at (179, 41), the List segment is 67×24 px at (180, 42), and the Graph segment is 68×24 px at (247, 42). `ForecastModeSelectorRenderer` derives these dimensions from the track geometry so the outer outline remains evenly visible. The user confirmed the updated selector renderer host test passed on 2026-11-27.
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
