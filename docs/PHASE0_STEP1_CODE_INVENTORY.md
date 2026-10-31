# Phase 0, Step 1 — Weather sketch code inventory

**Status:** Complete — static source inventory  
**Roadmap:** [Platform Refactor Roadmap](../PLATFORM_REFACTOR_ROADMAP.md)  
**Repository:** \`mdrmx/esp32-gadget\`  
**Source file:** \`weatherDashboard.ino\`  
**Source branch:** \`main\`  
**Source commit inspected:** \`55602880ff11e2f4091234091c25b5d23afd22dd\`  
**Source blob:** \`02ba63e385a36eb64d542df644126e7efea0b8b4\`  
**Review date:** 2026-10-31

---

## 1. Scope and method

This document completes the first Phase 0 task: inventory the functions, file-scope state, custom data structures and external dependencies in the existing Weather sketch.

The inventory was produced by inspecting the complete \`weatherDashboard.ino\` from the recorded \`main\` commit, including its declarations, section boundaries, function definitions and relevant call relationships.

This is a **static source inventory**, not a compile, runtime profile, hardware test or full data-flow audit. It deliberately does not infer unverified board-package versions, library versions or IDE configuration. Those remain Phase 0 work.

### Snapshot metrics

| Item | Observed |
|---|---:|
| Sketch size | 3,594 lines |
| Source characters | 119,387 |
| Function definitions | 75 |
| Forward function declarations | 1 (\`loadDemoWeatherData\`) |
| File-scope variables / constants / object declarations | 155 |
| Custom structs | 3 |
| Include directives | 13 |
| \`#define\` directives | 25 |

The 155 file-scope declarations include mutable state, constants, global hardware objects, pointers, caches and the fixed forecast array. They do not include struct members, function parameters or local variables.

## 2. Include and platform dependencies

The sketch has 13 direct include directives.

| Include | Current responsibility |
|---|---|
| \`WiFi.h\` | Wi-Fi station mode, connection status, scanning, BSSID/RSSI/channel, local IP |
| \`WiFiClientSecure.h\` | HTTPS/TLS client transport |
| \`HTTPClient.h\` | Synchronous HTTP GET and POST requests |
| \`ArduinoJson.h\` | Request/response JSON creation and parsing |
| \`Arduino_GFX_Library.h\` | SPI display, ST7789 driver, drawing primitives and canvas buffers |
| \`Wire.h\` | I²C communication with the CST816 touch controller |
| \`PNGdec.h\` | OpenWeather PNG icon decoding |
| \`time.h\` | Epoch, calendar conversion and local-time functions |
| \`sys/time.h\` | Setting the ESP32 system clock with \`settimeofday()\` |
| \`esp_heap_caps.h\` | Heap / PSRAM allocation diagnostics |
| \`esp_memory_utils.h\` | Checking whether a framebuffer pointer is in external RAM |
| \`stdlib.h\` | Standard allocation utilities, including use of \`malloc()\` / \`free()\` |
| \`secrets.h\` | Local Wi-Fi and API credential macros; ignored by Git |

### Platform APIs used in addition to the included libraries

The sketch also relies on Arduino / ESP32 core facilities, including:

- \`Serial\`, \`millis()\`, \`delay()\`.
- GPIO functions such as \`pinMode()\` and \`digitalWrite()\`.
- Arduino numeric helpers such as \`constrain()\`, \`min()\` and \`max()\`.
- I²C methods exposed through \`Wire\`.
- ESP32 Wi-Fi scan and connection APIs.
- ESP32 time and heap APIs.

### Configuration macros

The file contains 25 \`#define\` directives. They cover:

- \`USE_DEMO_WEATHER_DATA\`, currently set to \`0\`.
- Conditional \`DEBUG_PRINT\`, \`DEBUG_PRINTLN\` and \`DEBUG_PRINTF\` definitions.
- LCD SPI pins, reset, chip select, backlight, rotation and dimensions.
- Touch I²C pins, address and gesture-register address.
- CST816 raw gesture codes.

Credentials are referenced using macros supplied by the local \`secrets.h\`, including \`WIFI_SSID\`, \`WIFI_PASSWORD\` and \`OPENWEATHER_API_KEY\`. The Google Geolocation key is conditionally referenced as \`GOOGLE_GEOLOCATION_API_KEY\`. The file is intentionally not tracked.

**Finding:** there is no \`Preferences.h\` include or Preferences/NVS usage in the current sketch. Wi-Fi credentials are read from compile-time configuration rather than a user-managed saved-network store.

## 3. Custom data structures

There are three custom structs and no application classes, enums or namespaces defined in this sketch.

| Struct | Fields | Current use |
|---|---|---|
| \`TouchSample\` (line 119) | \`gesture\`, \`fingerCount\`, \`event\`, \`rawX\`, \`rawY\` | One decoded sample from the CST816 controller |
| \`ForecastItem\` (line 273) | \`timeText\`, \`description\`, \`temp\`, \`feelsLike\`, \`rainProbability\`, \`timestamp\` | One hourly forecast record |
| \`WeatherPngDrawContext\` (line 749) | \`x\`, \`y\`, \`backgroundBGR\` | Context passed to PNGdec's line callback |

### Data ownership observations

- \`TouchSample\` is a useful existing boundary between reading the controller and interpreting input, although its consumers still rely on global touch state.
- \`ForecastItem\` is stored in a global fixed-size array, \`forecast[MAX_FORECAST]\`, with \`MAX_FORECAST = 48\`. Its \`String\` members can allocate dynamically.
- \`WeatherPngDrawContext\` is a short-lived rendering callback context, not a general weather data model.

## 4. File-scope state inventory

The file has 155 file-scope declarations grouped below. The groups indicate where the state is currently located, not a recommendation that each group should become one class.

### 4.1 Configuration and location — 10 declarations (lines 33–44)

\`wifiSsid\`, \`wifiPassword\`, \`apiKey\`, \`CITY\`, \`locationLat\`, \`locationLon\`, \`locationTimezoneOffset\`, \`locationDisplayName\`, \`locationCoordinatesResolved\`, \`UPDATE_INTERVAL\`.

These combine credentials, default location, resolved location state and the Weather refresh interval.

### 4.2 Clock and time synchronisation — 10 declarations (lines 52–64)

\`TIME_ZONE_INFO\`, \`TIME_API_URL\`, \`MIN_VALID_EPOCH\`, \`timeApiValid\`, \`lastTimeApiAttemptMs\`, \`lastTimeApiSyncMs\`, \`lastClockCheckMs\`, \`lastHeaderClock\`, \`TIME_API_RETRY_INTERVAL\`, \`TIME_API_SYNC_INTERVAL\`.

This combines time-provider configuration, system-clock validity, sync scheduling and a cached header string.

### 4.3 Display and framebuffer ownership — 8 declarations (lines 82–96)

\`bus\`, \`gfx\`, \`forecastCanvas\`, \`weatherMetricsCanvas\`, \`tabTransitionCanvas\`, \`renderingTabTransition\`, \`screenW\`, \`screenH\`.

The SPI bus and ST7789 graphics object are allocated as global objects. The three canvases are allocated later in \`setup()\`. Display dimensions and rendering state are global.

### 4.4 Touch and gesture state — 50 declarations (lines 114–180)

**Controller and gesture state:**

\`touchAvailable\`, \`lastHandledGesture\`, \`previousRawGesture\`, \`lastGestureTime\`.

**Weather metric drag state:**

\`metricsTouchDragActive\`, \`metricsTouchCandidate\`, \`metricsTouchDragMoved\`, \`metricsGestureStartedInsidePanel\`, \`touchSampleWasDown\`, \`metricsDragStartX\`, \`metricsDragStartY\`, \`metricsDragStartScroll\`, \`metricsDragCurrentScroll\`, \`suppressMetricGesture\`, \`metricsGestureSuppressionStartedMs\`.

**Forecast list drag state:**

\`forecastTouchDragActive\`, \`forecastTouchCandidate\`, \`forecastGestureStartedInsideViewport\`, \`forecastTouchDragMoved\`, \`forecastDragHasRendered\`, \`lastForecastDragRenderMs\`, \`forecastDragLastRenderedScroll\`, \`forecastDragStartX\`, \`forecastDragStartY\`, \`forecastDragStartScroll\`, \`forecastDragCurrentScroll\`.

**Forecast graph touch / ownership / drag state:**

\`forecastGraphTouchOwned\`, \`forecastGraphTouchCandidate\`, \`forecastGraphQuickSwipeCandidate\`, \`forecastGraphQuickSwipeReady\`, \`suppressGraphControllerGestureAfterTouch\`, \`graphGestureQuietPeriodActive\`, \`graphGestureSuppressionStartedMs\`, \`graphGestureQuietStartedMs\`, \`forecastGraphDragActive\`, \`forecastGraphDragMoved\`, \`forecastGraphDragHasRendered\`, \`forecastGraphTouchStartedMs\`, \`lastForecastGraphDragRenderMs\`, \`forecastGraphTouchStartX\`, \`forecastGraphTouchStartY\`, \`forecastGraphTouchLastX\`, \`forecastGraphTouchLastY\`, \`forecastGraphTouchStartScroll\`, \`forecastGraphDragStartX\`, \`forecastGraphDragStartScroll\`, \`forecastGraphDragCurrentScroll\`, \`forecastGraphLastRenderedScroll\`, \`suppressForecastGesture\`, \`forecastGestureSuppressionStartedMs\`.

**Observation:** touch state is the largest single state group. Several sets represent separate interaction modes, and the touch recogniser directly manipulates weather / forecast screen state. This is a high-regression-risk extraction area and should not be simplified until its behaviour is mapped and recorded.

### 4.5 UI colours — 8 declarations (lines 186–193)

\`BG\`, \`PANEL\`, \`UI_WHITE\`, \`MUTED\`, \`ACCENT\`, \`UI_YELLOW\`, \`UI_GREEN\`, \`ERROR_COLOUR\`.

These are shared drawing constants used directly by screen renderers.

### 4.6 Weather and forecast state / layout — 65 declarations (lines 199–291)

**Weather data and availability — 21:**

\`weatherCity\`, \`weatherDescription\`, \`weatherMain\`, \`weatherIcon\`, \`forecastError\`, \`temperature\`, \`feelsLike\`, \`humidity\`, \`pressure\`, \`windSpeedKmh\`, \`cloudCover\`, \`visibilityMeters\`, \`rainLastHourMm\`, \`currentUvIndex\`, \`currentRainProbability\`, \`visibilityAvailable\`, \`uvIndexAvailable\`, \`rainProbabilityAvailable\`, \`weatherAvailable\`, \`forecastAvailable\`, \`lastUpdate\`.

**Forecast capacity — 2:**

\`MAX_FORECAST\`, \`ITEMS_PER_PAGE\`.

**Metric card layout and scroll configuration / position — 11:**

\`WEATHER_METRIC_COUNT\`, \`WEATHER_METRICS_VISIBLE\`, \`WEATHER_METRIC_PANEL_TOP\`, \`WEATHER_METRIC_PANEL_HEIGHT\`, \`WEATHER_METRIC_PANEL_BOTTOM\`, \`WEATHER_METRIC_CARD_HEIGHT\`, \`WEATHER_METRIC_ROW_STEP\`, \`SCROLL_OVERSCROLL_MAX_PX\`, \`SCROLL_OVERSCROLL_RESISTANCE_DIVISOR\`, \`WEATHER_METRIC_ROW_TOP\`, \`weatherMetricOffset\`.

**Forecast viewport, row, graph, control and animation configuration — 21:**

\`FORECAST_VIEWPORT_TOP\`, \`FORECAST_VIEWPORT_HEIGHT\`, \`FORECAST_VIEWPORT_BOTTOM\`, \`TAB_TRANSITION_CONTENT_TOP\`, \`TAB_TRANSITION_FOOTER_HEIGHT\`, \`FORECAST_ROW_TOP\`, \`FORECAST_ROW_STEP\`, \`FORECAST_CARD_HEIGHT\`, \`FORECAST_GRAPH_POINTS\`, \`FORECAST_GRAPH_POINT_SPACING\`, \`FORECAST_GRAPH_HOLD_MS\`, \`FORECAST_GRAPH_TOUCH_INTENT_PX\`, \`FORECAST_GRAPH_DRAG_START_PX\`, \`FORECAST_GRAPH_TAB_SWIPE_PX\`, \`FORECAST_CONTROL_TOP\`, \`FORECAST_CONTROL_BOTTOM\`, \`FORECAST_CONTROL_HIT_TOP\`, \`FORECAST_CONTROL_HIT_BOTTOM\`, \`FORECAST_GRAPH_VERTICAL_SHIFT\`, \`FORECAST_CONTROL_TAP_SLOP_PX\`, \`FORECAST_DRAG_RENDER_INTERVAL_MS\`.

**Forecast data, view and tab state — 10:**

\`forecast\`, \`forecastCount\`, \`forecastOffset\`, \`forecastGraphView\`, \`forecastGraphOffset\`, \`forecastGraphScrollPixels\`, \`forecastControlTouch\`, \`forecastControlStartX\`, \`forecastControlStartY\`, \`currentTab\`.

### 4.7 PNG icon decoder and cache — 4 declarations (lines 744–747)

\`weatherPngDecoder\`, \`weatherIconPngData\`, \`weatherIconPngLength\`, \`cachedWeatherIconCode\`.

The PNG byte cache is managed through a global pointer and length. The icon download allocates with \`malloc()\`, frees a previous cached buffer when replacing it, and retains the new buffer for later rendering.

### 4.8 Global-state summary

| State group | Declarations |
|---|---:|
| Configuration and location | 10 |
| Clock / time | 10 |
| Display | 8 |
| Touch / gestures | 50 |
| Colours | 8 |
| Weather / forecast / layout | 65 |
| PNG / icon cache | 4 |
| **Total** | **155** |

## 5. Function inventory

There are 75 function definitions and one forward declaration. The functions are grouped below by their current sketch section and responsibility. Line numbers refer to the inspected source commit.

### 5.1 Text and layout helpers — 6 functions (lines 297–421)

- \`drawText\` (297)
- \`drawCenteredText\` (304)
- \`measureTextWidth\` (320)
- \`measureTextHeight\` (328)
- \`drawWrappedCenteredText\` (339)
- \`drawCenteredWeatherLine\` (407)

### 5.2 Clock, time and diagnostics — 10 functions (lines 426–687)

- \`getCurrentUtcEpoch\` (426)
- \`getHeaderClockText\` (437)
- \`reportCanvasAllocation\` (457)
- \`reportHeapSummary\` (487)
- \`getHeaderLocationText\` (496)
- \`drawHeaderClockOnly\` (517)
- \`refreshHeaderClock\` (533)
- \`fetchTimeFromAPI\` (540)
- \`maybeSyncTimeFromAPI\` (655)
- \`initialiseTimeZone\` (681)

### 5.3 Header, navigation indicator and feedback — 4 functions (lines 692–734)

- \`drawHeader\` (692)
- \`drawTabIndicator\` (713)
- \`drawLoading\` (723)
- \`drawError\` (730)

### 5.4 PNG icon download and decoding — 3 functions (lines 755–912)

- \`drawWeatherPngLine\` (755)
- \`loadOpenWeatherIconPNG\` (778)
- \`drawOpenWeatherIcon\` (875)

### 5.5 Current-weather / metric-card UI — 13 functions (lines 919–1259)

- \`weatherMetricsPanelX\` (919)
- \`weatherMetricsPanelWidth\` (923)
- \`weatherMetricLabel\` (927)
- \`weatherMetricValue\` (941)
- \`drawWeatherMetricItem\` (965)
- \`drawWeatherMetricsViewport\` (1042)
- \`drawWeatherMetricsPanel\` (1102)
- \`maxWeatherMetricScrollPixels\` (1112)
- \`elasticScrollPixels\` (1122)
- \`drawWeatherMetricsAtScrollPosition\` (1138)
- \`animateWeatherMetricsSnap\` (1152)
- \`animateWeatherMetricsScroll\` (1177)
- \`drawWeather\` (1209)

### 5.6 Forecast / graph / forecast controls UI — 23 functions (lines 1264–1958)

- \`forecastCompactTime\` (1264)
- \`drawMiniWeatherIcon\` (1273)
- \`drawForecastItem\` (1329)
- \`drawForecastItemClipped\` (1369)
- \`drawForecastModeSelector\` (1423)
- \`drawForecastControls\` (1436)
- \`maxForecastGraphScrollPixels\` (1453)
- \`syncForecastGraphOffsetFromScroll\` (1458)
- \`drawForecastGraph\` (1466)
- \`flushForecastGraphCanvasBody\` (1605)
- \`redrawForecastGraphAtCurrentScroll\` (1626)
- \`drawForecastGraphAtScrollPosition\` (1651)
- \`animateForecastGraphSnap\` (1661)
- \`handleForecastControlTap\` (1684)
- \`drawForecastFooter\` (1702)
- \`drawForecastViewport\` (1710)
- \`drawForecast\` (1778)
- \`resetScrollableViewPositions\` (1806)
- \`animateTabTransition\` (1821)
- \`animateForecastScroll\` (1892)
- \`maxForecastScrollPixels\` (1905)
- \`drawForecastAtScrollPosition\` (1909)
- \`animateForecastSnap\` (1933)

### 5.7 Touch, drag and gesture dispatch — 5 functions (lines 1963–2680)

- \`readTouchSample\` (1963)
- \`touchCoordinatesToScreen\` (1992)
- \`updateTouchDrag\` (2004)
- \`handleGesture\` (2483)
- \`pollTouch\` (2577)

### 5.8 Wi-Fi connection and location resolution — 4 functions (lines 2699–3091)

- \`connectWiFi\` (2699)
- \`resolveLocationNameFromCoordinates\` (2733)
- \`resolveLocationFromWiFi\` (2793)
- \`resolveLocationFromIP\` (3029)

### 5.9 Current weather data retrieval — 1 function (lines 3092–3174)

- \`fetchWeather\` (3092)

### 5.10 Forecast data, time formatting and demo fixture — 4 functions (lines 3175–3420)

- \`formatForecastTime\` (3175)
- \`loadDemoWeatherData\` (3199)
- \`fetchOneCallForecastPage\` (3257)
- \`fetchForecast\` (3363)

### 5.11 Firmware lifecycle — 2 functions

- \`setup\` (3424)
- \`loop\` (3573)

The one forward declaration is \`bool loadDemoWeatherData();\` at line 221.

## 6. Major call relationships and ownership

The following are the most important relationships for the upcoming architecture review. This is a concise map, not a complete call graph of every helper.

### Startup path

\`setup()\` currently:

1. Starts Serial.
2. Initialises the display.
3. Allocates the forecast, metric and tab-transition canvases.
4. Reports framebuffer / heap information.
5. Initialises I²C and probes the CST816.
6. Configures the timezone.
7. In live mode, attempts Wi-Fi connection, then Google Wi-Fi geolocation with IP-geolocation fallback.
8. Fetches current weather and forecast.
9. Draws the initial Weather screen and records the refresh timestamp.

This means hardware setup, UI buffer allocation, connection setup, location lookup, data retrieval and first rendering are all orchestrated in one function.

### Main loop

\`loop()\` currently runs:

- \`pollTouch()\` on every iteration.
- \`maybeSyncTimeFromAPI()\`.
- Header clock refresh once per second.
- Weather and forecast retrieval every \`UPDATE_INTERVAL\` (600,000 ms / 10 minutes), followed by a screen redraw.
- \`delay(30)\` at the end of the loop.

Network operations called from the loop path are synchronous. This can delay subsequent touch polling and rendering while a request or connection operation waits.

### Touch and screen coupling

- \`pollTouch()\` reads a \`TouchSample\`, calls \`updateTouchDrag()\`, performs suppression / ownership checks and dispatches to \`handleGesture()\`.
- \`updateTouchDrag()\` contains screen-specific hit testing and drag state. It directly invokes weather/forecast scroll, snap, control and tab-transition routines.
- \`handleGesture()\` directly reads and changes \`currentTab\`, \`forecastOffset\` and \`weatherMetricOffset\`, then invokes screen animations.
- The metric list, forecast list and graph use separate global state and separate animation / snap functions.

There is already a shared helper, \`elasticScrollPixels()\`, used by more than one scrolling path. The rest of the behaviour is not yet represented by a general-purpose scroll controller.

### Rendering and network coupling

A significant side effect is:

\`drawWeather()\` → \`drawOpenWeatherIcon()\` → \`loadOpenWeatherIconPNG()\` → HTTPS download.

The current Weather screen renderer can therefore initiate a network request while drawing. The PNG decoder, download buffer and cached icon code are also global. This is a clear candidate for separating asset retrieval/cache updates from rendering.

Most drawing functions also use the global \`gfx\` object, global dimensions, global colours and global Weather data directly. Some forecast drawing functions accept an \`Arduino_GFX* target\`, but the overall renderer is not independent of global display state.

### Connectivity, location and data coupling

- \`connectWiFi()\` owns Wi-Fi connection setup, calls \`drawLoading()\` and may trigger time synchronisation through \`maybeSyncTimeFromAPI()\`.
- \`fetchWeather()\` calls \`connectWiFi()\`, performs the request, updates global Weather fields and calls drawing / error functions.
- \`fetchForecast()\` calls \`connectWiFi()\` and calls \`fetchOneCallForecastPage()\`.
- \`resolveLocationFromWiFi()\` and \`resolveLocationFromIP()\` update global coordinates and names, and both may call \`resolveLocationNameFromCoordinates()\`.
- Location resolution is therefore coupled to Wi-Fi APIs and, for reverse geocoding, to OpenWeather.
- Time synchronisation also checks Wi-Fi state directly.

The code does not yet have a transport-neutral network service, a separate Location Service interface or a persistent Settings / Wi-Fi profile service.

## 7. Network and external service inventory

The seven functions below directly create \`HTTPClient\` / \`WiFiClientSecure\` request paths.

| Function | Service / endpoint | Method / purpose |
|---|---|---|
| \`fetchTimeFromAPI\` | TimeAPI.io \`/api/Time/current/zone?timeZone=Europe%2FLondon\` | GET current UK time |
| \`loadOpenWeatherIconPNG\` | OpenWeather \`/img/wn/{icon}@2x.png\` | GET PNG icon asset |
| \`resolveLocationNameFromCoordinates\` | OpenWeather \`/geo/1.0/reverse\` | GET reverse geocode |
| \`resolveLocationFromWiFi\` | Google Geolocation \`/geolocation/v1/geolocate\` | POST Wi-Fi access-point observations |
| \`resolveLocationFromIP\` | ipwho.is \`/\` | GET coarse IP geolocation |
| \`fetchWeather\` | OpenWeather \`/data/2.5/weather\` | GET current weather |
| \`fetchOneCallForecastPage\` | OpenWeather \`/data/4.0/onecall/timeline/1h\` | GET hourly forecast page |

Other relevant observations:

- \`connectWiFi()\` directly uses \`WiFi.mode()\`, \`WiFi.begin()\` and \`WiFi.status()\`.
- \`resolveLocationFromWiFi()\` directly uses \`WiFi.scanNetworks()\`, BSSID, RSSI and channel information.
- \`fetchTimeFromAPI()\`, \`resolveLocationNameFromCoordinates()\`, \`resolveLocationFromWiFi()\`, \`resolveLocationFromIP()\`, \`fetchWeather()\`, \`fetchOneCallForecastPage()\` and the icon download create TLS clients. The current HTTPS code uses \`setInsecure()\`; certificate validation should remain a separate security review item and should not be silently propagated as the final platform policy.
- Request timeouts are specified in individual request functions and are not governed by a common request policy.
- Wi-Fi geolocation sends access-point BSSID / signal observations to Google's service; it does not connect to those scanned networks.
- IP geolocation is explicitly documented in the source as coarse and potentially reflecting an ISP/VPN exit point.

## 8. Display and memory-related inventory

### Display pipeline

- Global \`Arduino_DataBus*\` and \`Arduino_GFX*\` objects initialise the ESP32 SPI bus and ST7789.
- The UI uses RGB565 drawing operations.
- \`forecastCanvas\` buffers the forecast viewport.
- \`weatherMetricsCanvas\` buffers the narrow metric panel.
- \`tabTransitionCanvas\` buffers a full-screen tab transition.
- Each canvas has a null-check and fallback path. The code logs the actual framebuffer region and heap summary.

### Framebuffer size estimate

Using the current logical dimensions and RGB565 (2 bytes/pixel), the nominal requested canvas buffers are:

| Canvas | Dimensions used by setup | Approximate buffer |
|---|---:|---:|
| Forecast viewport | 320 × 152 | 97,280 bytes |
| Weather metrics | 118 × 177 | 41,772 bytes |
| Tab transition | 320 × 240 | 153,600 bytes |
| **Combined nominal request** | | **292,652 bytes (~286 KiB)** |

The 118-pixel metrics width is derived from the current layout formula at a 320-pixel screen width. These are requested pixel-buffer sizes, not measured live heap consumption. Allocation overhead, other data, fragmentation and the actual memory region are not included. The firmware's own heap diagnostics should be used to measure actual use in a later Phase 0 step.

### Other memory observations

- The forecast uses a fixed global array of 48 \`ForecastItem\` records, but each record contains Arduino \`String\` fields.
- The PNG cache allocates a buffer with \`malloc()\`, with a code-level maximum content length of 20,000 bytes.
- Display bus / graphics objects are constructed with global \`new\`; canvases use \`new\` in \`setup()\`.
- Touch state is held in many global scalars rather than in a cohesive state object.
- Heap / PSRAM diagnostic helpers already exist; preserve their value during any refactor.

## 9. Initial refactoring observations

These are evidence-based findings from the inventory, not implementation instructions for Phase 1. Their priority and exact treatment should be confirmed after the remaining Phase 0 review.

| Finding | Evidence | Architectural implication |
|---|---|---|
| Large globally owned state surface | 155 file-scope declarations across configuration, services, display, touch and Weather UI | Establish explicit ownership boundaries before moving code |
| Touch logic knows screen details | \`updateTouchDrag()\` and \`handleGesture()\` directly manipulate per-screen state and invoke render/animation routines | Separate raw input, semantic gesture interpretation, scroll state and app-specific actions incrementally |
| Scroll mechanisms are partly duplicated | Separate metric, forecast-list and graph state / animation functions; shared elastic helper already exists | Compare behaviour first; extract only the common scroll mechanics |
| Rendering has network side effects | Weather render path downloads the PNG icon through \`loadOpenWeatherIconPNG()\` | Move asset retrieval/cache work out of the render path |
| Wi-Fi ownership is distributed | Connection, scan and status checks appear in multiple functions; data functions call \`connectWiFi()\` | Introduce Connectivity Manager and backend boundary |
| Service orchestration has UI side effects | \`connectWiFi()\` draws loading UI; fetch functions draw loading/error/weather UI | Separate service results from app presentation |
| Location is coupled to providers | Wi-Fi/IP location routines write globals and call OpenWeather reverse geocoding | Introduce a location result model with source, accuracy and freshness |
| Time service is coupled to Wi-Fi and header rendering | Time functions inspect Wi-Fi state and draw/cache header clock text | Separate time synchronisation/state from header presentation |
| Application state is implicit | One \`currentTab\` variable switches two Weather views; there is no app lifecycle | Add an application manager only after Weather boundaries are clear |
| Canvas ownership is device-global | Three app-specific canvases are allocated by global state during setup | Review shared framebuffer reuse, lifetimes and budget before multi-app support |
| No persistent Wi-Fi profile layer | Credentials come from \`secrets.h\`; no Preferences/NVS use | Add persistent network profiles in Phase 2 |
| Blocking work can delay UI | Wi-Fi connection loop waits up to 20 seconds; HTTP calls are synchronous; animation routines use delays | Map and measure blocking paths before choosing asynchronous/non-blocking changes |
| TLS policy needs review | HTTPS clients currently use \`setInsecure()\` | Keep security improvement visible; don't unintentionally enshrine insecure defaults |

## 10. Repository-level observations

At the inspected commit, the repository contains the main sketch and several root-level Markdown guides. There is no \`platformio.ini\`, CMake project, library manifest, explicit board configuration file, or test directory in the repository tree inspected for this step.

This does **not** establish which local IDE or command-line build process is used. Confirm the actual workflow, board package, compiler/core version and installed libraries in the later Phase 0 build/toolchain step.

The repository tree at the time included:

- \`weatherDashboard.ino\`
- \`DEVELOPMENT_MODE.md\`
- \`GOOGLE_GEOLOCATION_SETUP.md\`
- \`HOURLY_WEATHER.md\`
- \`TIME_API_SETUP.md\`
- \`TOUCH_GESTURE_DIAGNOSTIC.md\`
- \`PLATFORM_REFACTOR_ROADMAP.md\`
- \`.gitignore\`

## 11. Questions carried forward to the remaining Phase 0 work

1. What exact Arduino IDE / Arduino CLI workflow, ESP32 board package and library versions are used to compile the current sketch?
2. Can the existing firmware be built reproducibly from a clean checkout when local secrets are supplied?
3. Which APIs are called during startup, during periodic refresh and from touch / render paths, including all timeouts and retry behaviour?
4. Which operations can block the main loop, and what are the measured worst-case delays?
5. What heap and PSRAM capacities are actually available on the exact Waveshare board configuration? Where do the three canvases allocate on-device?
6. Can the full touch interaction state machine be documented from touch-down through drag, release, suppression and controller gesture dispatch?
7. Which duplicated scroll behaviours are truly common, and which are graph/list-specific by design?
8. Which source files and build layout can be introduced without disrupting the current Arduino build workflow?
9. What current documentation is stale or inaccurate? In particular, \`weatherDashboard.ino\` currently defines \`USE_DEMO_WEATHER_DATA 0\`, while \`DEVELOPMENT_MODE.md\` says that 1 is the current default.

## 12. Step 1 completion and next step

**Step 1 is complete:** the current Weather sketch's functions, file-scope declarations, data structures, direct include dependencies, external request endpoints and major ownership/coupling relationships have been inventoried.

This does **not** complete Phase 0.

**Next:** Phase 0, Step 2 should map the runtime data flows in more detail: startup, main loop, periodic refresh, touch-down/drag/release, rendering, network requests and location/time resolution. That map should connect the symbols in this inventory to the actual execution paths and identify all blocking boundaries before any code is moved.

No firmware source was changed as part of this inventory.
