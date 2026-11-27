#include <algorithm>
#include <WiFi.h>
#include <Arduino_GFX_Library.h>
#include <PNGdec.h>
#include <time.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <stdlib.h>
#include "secrets.h"
#include "src/input/TouchSample.h"
#include "src/input/BottomEdgeSwipeGesture.h"
#include "src/input/GestureCode.h"
#include "src/input/GestureRecognizer.h"
#include "src/input/InteractionArbiter.h"
#include "src/ui/ScrollController.h"
#include "src/ui/TextRenderer.h"
#include "src/ui/ForecastModeSelectorRenderer.h"
#include "src/ui/HeaderRenderer.h"
#include "src/ui/ApplicationLauncherRenderer.h"
#include "src/ui/StatusScreenRenderer.h"
#include "src/ui/WiFiSettingsRenderer.h"
#include "src/ui/ForecastListItemRenderer.h"
#include "src/ui/ForecastListViewportRenderer.h"
#include "src/ui/ForecastGraphRenderer.h"
#include "src/ui/WeatherMetricCardRenderer.h"
#include "src/ui/WeatherMetricsViewportRenderer.h"
#include "src/ui/WeatherConditionsRenderer.h"
#include "src/hardware/DisplayManager.h"
#include "src/hardware/Cst816Reader.h"
#include "src/services/TimeService.h"
#include "src/services/ConnectivityBackend.h"
#include "src/services/ConnectivityManager.h"
#include "src/services/WiFiBackend.h"
#include "src/services/WiFiProfileStore.h"
#include "src/services/PreferencesWiFiProfileStorage.h"
#include "src/services/WiFiNetworkScanner.h"
#include "src/services/WiFiAvailabilitySnapshot.h"
#include "src/services/WiFiAvailabilityRefreshPolicy.h"
#include "src/services/WiFiSignalMeter.h"
#include "src/services/Esp32WiFiScanBackend.h"
#include "src/services/Esp32WiFiGeolocationScanBackend.h"
#include "src/services/LocationService.h"
#include "src/services/WeatherDataService.h"
#include "src/apps/ApplicationManager.h"
#include "src/apps/ApplicationLauncherController.h"
#include "src/apps/weather/WeatherApp.h"
#include "src/apps/weather/WeatherMetricCatalog.h"
#include "src/apps/weather/WeatherViewState.h"
#include "src/apps/weather/WeatherAssetCache.h"

// The service owns these models. Temporary references keep the existing
// Weather renderers readable while data ownership moves out of the sketch.
WeatherDataService weatherDataService;
WeatherCurrentData& weatherCurrentData = weatherDataService.current();
WeatherForecastData& weatherForecastData = weatherDataService.forecast();

// Owns registered application lifecycles; the sketch remains the composition root.
ApplicationManager applicationManager;
ApplicationLauncherController applicationLauncherController(applicationManager);

// Weather is the first registered application. Its shared services and
// renderers remain owned by the platform composition root below.
WeatherApp weatherApp;

// Owns the single cached provider icon buffer and its identity.
WeatherAssetCache weatherAssetCache;

// Owns persistent Weather view selection and scroll/display state.
WeatherViewState weatherViewState;

// Temporary sketch-facing references preserve the existing renderer and input
// code while ownership of these values moves into WeatherViewState.
int& currentTab = weatherViewState.currentTab;
int& weatherMetricOffset = weatherViewState.scroll.weatherMetricOffset;
int& forecastOffset = weatherViewState.scroll.forecastOffset;
int& forecastGraphOffset = weatherViewState.scroll.forecastGraphOffset;
int& forecastGraphScrollPixels = weatherViewState.scroll.forecastGraphScrollPixels;
bool& forecastGraphView = weatherViewState.forecastGraphView;

// =====================================================
// CONFIGURATION
// =====================================================

// Set to 1 for deterministic offline UI development. Live weather and
// Wi-Fi positioning (with IP geolocation fallback) are enabled by default.
#define USE_DEMO_WEATHER_DATA 0

#if USE_DEMO_WEATHER_DATA
  #define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...) do {} while (0)
  #define DEBUG_PRINTLN(...) do {} while (0)
  #define DEBUG_PRINTF(...) do {} while (0)
#endif

const char* wifiSsid = WIFI_SSID;
const char* wifiPassword = WIFI_PASSWORD;
const char* apiKey = OPENWEATHER_API_KEY;
const char* CITY = "Manchester,GB";
float locationLat = 53.4808f;
float locationLon = -2.2426f;
// Prefer a reverse-geocoded display name, with the IP provider's city as a
// fallback. Keep it separate from the weather provider's station name.
String locationDisplayName = "";
bool locationCoordinatesResolved = false;

// =====================================================
// TIME SERVICE
// =====================================================

TimeService timeService;

// Platform connectivity owns Wi-Fi association lifecycle. WeatherApp starts
// and observes connection state through injected hooks; the platform loop
// advances the manager before each WeatherApp update.
WiFiBackend wifiBackend;
ConnectivityManager connectivityManager(wifiBackend);
PreferencesWiFiProfileStorage wifiProfileStorage;
typedef WiFiProfileStore<PreferencesWiFiProfileStorage> PersistentWiFiProfileStore;
PersistentWiFiProfileStore wifiProfileStore(wifiProfileStorage);
Esp32WiFiScanBackend wifiScanBackend;
WiFiNetworkScanner wifiNetworkScanner(wifiScanBackend);
WiFiAvailabilitySnapshot wifiAvailabilitySnapshot;
WiFiAvailabilityRefreshPolicy wifiAvailabilityRefreshPolicy;
WiFiSignalMeter wifiSignalMeter;
uint32_t wifiSignalLastSampleMs = 0;

// WiFiBackend borrows these RAM strings. Keep this copy alive for the full
// device lifetime, independently from temporary Preferences read buffers.
PersistentWiFiProfileStore::Profile activeWiFiProfile = {};

// LocationService consumes a platform-neutral scan capability. The ESP32
// adapter owns the Arduino Wi-Fi scan calls; the scanner owns Google AP policy.
Esp32WiFiGeolocationScanBackend wifiGeolocationScanBackend;
WiFiGeolocationScanner wifiGeolocationScanner(wifiGeolocationScanBackend);
LocationService locationService(wifiGeolocationScanner);
LocationService::LocationFix currentLocationFix = {};

// Shared-header redraw cache and cadence are platform presentation state.
String lastHeaderClock = "";
uint32_t lastPlatformClockRefreshMs = 0;

// =====================================================
// DISPLAY
// =====================================================

// Owns the physical display, render target, dimensions and off-screen buffers.
DisplayManager displayManager;
TextRenderer textRenderer(displayManager);
WeatherConditionsRenderer weatherConditionsRenderer(textRenderer);
HeaderRenderer headerRenderer(displayManager, textRenderer);
StatusScreenRenderer statusScreenRenderer(
  displayManager, headerRenderer, textRenderer);

// =====================================================
// TOUCH
// =====================================================

// The raw reader owns I2C access and the recognizer interprets controller
// samples. Weather chooses view owners and retains view-specific suppression/actions.
Cst816Reader touchReader;
GestureRecognizer gestureRecognizer;
BottomEdgeSwipeGesture applicationLauncherGesture;
InteractionArbiter interactionArbiter;

// Duplicate-event suppression and navigation debounce remain Weather policy.
GestureCode lastHandledGesture = GestureCode::None;
unsigned long lastGestureTime = 0;

bool metricsTouchDragActive = false;
bool metricsTouchCandidate = false;
bool metricsTouchDragMoved = false;
// Origin gate for the controller's legacy swipe fallback: metric scrolling is
// allowed only when the current touch sequence began over the metrics panel.
bool metricsGestureStartedInsidePanel = false;
int metricsDragStartX = 0;
int metricsDragStartY = 0;
int metricsDragStartScroll = 0;
int metricsDragCurrentScroll = 0;
bool suppressMetricGesture = false;
unsigned long metricsGestureSuppressionStartedMs = 0;

bool forecastTouchDragActive = false;
bool forecastTouchCandidate = false;
// Gate the legacy gesture-only fallback so vertical list scrolling must start
// in the visible list viewport when coordinate tracking is unavailable.
bool forecastGestureStartedInsideViewport = false;
bool forecastTouchDragMoved = false;
bool forecastDragHasRendered = false;
unsigned long lastForecastDragRenderMs = 0;
int forecastDragLastRenderedScroll = -1;
int forecastDragStartX = 0;
int forecastDragStartY = 0;
int forecastDragStartScroll = 0;
int forecastDragCurrentScroll = 0;

// Graph-mode contacts are owned by the graph from touch-down to release so a
// lost coordinate sample cannot fall through to the CST816 tab-swipe event.
bool forecastGraphTouchOwned = false;
bool forecastGraphTouchCandidate = false;
bool forecastGraphQuickSwipeCandidate = false;
bool forecastGraphQuickSwipeReady = false;
bool suppressGraphControllerGestureAfterTouch = false;
bool graphGestureQuietPeriodActive = false;
unsigned long graphGestureSuppressionStartedMs = 0;
unsigned long graphGestureQuietStartedMs = 0;
bool forecastGraphDragActive = false;
bool forecastGraphDragMoved = false;
bool forecastGraphDragHasRendered = false;
unsigned long forecastGraphTouchStartedMs = 0;
unsigned long lastForecastGraphDragRenderMs = 0;
int forecastGraphTouchStartX = 0;
int forecastGraphTouchStartY = 0;
int forecastGraphTouchLastX = 0;
int forecastGraphTouchLastY = 0;
int forecastGraphTouchStartScroll = 0;
int forecastGraphDragStartX = 0;
int forecastGraphDragStartScroll = 0;
int forecastGraphDragCurrentScroll = 0;
int forecastGraphLastRenderedScroll = -1;
bool suppressForecastGesture = false;
unsigned long forecastGestureSuppressionStartedMs = 0;

// =====================================================
// COLOURS
// =====================================================

// Warm paper, ink and burnt-orange palette inspired by e-ink devices.
// Shared OS palette supplied for the e-ink / retro-terminal visual style.
const uint16_t BG = 0xFF9C;          // #FEF3E2 — warm off-white paper
const uint16_t PANEL = BG;              // Card surfaces stay paper-coloured.
const uint16_t BURNT_ORANGE_LIGHT = 0xF56A; // #F4AE52 — lighter burnt orange.
const uint16_t UI_INK = 0x28C1;      // #2A1A0E — dark brown ink
const uint16_t ACCENT = 0xD303;      // #D4621A — burnt orange
const uint16_t MUTED = UI_INK;        // Secondary text remains ink-coloured.
const uint16_t UI_ORANGE = ACCENT;
const uint16_t ERROR_COLOUR = ACCENT;

const StatusScreenRenderer::Colours STATUS_SCREEN_COLOURS = {
  BG, PANEL, UI_INK, UI_INK, UI_INK, UI_INK, UI_INK
};

const ApplicationLauncherRenderer::Colours APPLICATION_LAUNCHER_COLOURS = {
  BG, BG, UI_INK, UI_INK, ACCENT, UI_INK
};

ApplicationLauncherRenderer applicationLauncherRenderer(
  displayManager, textRenderer, APPLICATION_LAUNCHER_COLOURS);

const WiFiSettingsRenderer::Colours WIFI_SETTINGS_COLOURS = {
  BG, BG, UI_INK, UI_INK, ACCENT, BURNT_ORANGE_LIGHT, ACCENT
};

const ForecastModeSelectorRenderer::Colours FORECAST_MODE_SELECTOR_COLOURS = {
  BG, BURNT_ORANGE_LIGHT, UI_INK, UI_INK
};

enum class WiFiSettingsScreen : uint8_t {
  Closed = 0,
  Profiles,
  Scan,
  ProfileDetail,
  Password,
  Connecting
};

enum class WiFiConnectPhase : uint8_t {
  None = 0,
  Candidate,
  Restore
};

WiFiSettingsScreen wifiSettingsScreen = WiFiSettingsScreen::Closed;
WiFiConnectPhase wifiConnectPhase = WiFiConnectPhase::None;
bool wifiAvailabilityConnectionObserved = false;
bool wifiAvailabilityLastConnected = false;
bool wifiAvailabilityRefreshPending = false;
char wifiSelectedSsid[PersistentWiFiProfileStore::MaxSsidLength + 1] = {};
char wifiPasswordInput[PersistentWiFiProfileStore::MaxPasswordLength + 1] = {};
bool wifiKeyboardShifted = false;
bool wifiKeyboardSymbols = false;
bool wifiPasswordVisible = false;

bool wifiKeyFeedbackActive = false;
bool wifiKeyFeedbackHeld = false;
int8_t wifiHighlightedKeyRow = -1;
int8_t wifiHighlightedKeyColumn = -1;
uint32_t wifiKeyFeedbackUntilMs = 0;

PersistentWiFiProfileStore::Profile wifiSelectedProfile = {};
bool wifiSelectedProfileValid = false;
bool wifiForgetConfirmation = false;
PersistentWiFiProfileStore::Profile wifiPendingProfile = {};
bool wifiPendingProfileWasNew = false;
PersistentWiFiProfileStore::Profile wifiPreviousProfile = {};
bool wifiPreviousProfileValid = false;

WiFiSettingsRenderer::ProfileRow
    wifiProfileRows[PersistentWiFiProfileStore::MaxProfiles] = {};
int wifiListScrollPositionPx = 0;
bool wifiListTouchCandidate = false;
bool wifiListTouchDragActive = false;
int wifiListDragStartScroll = 0;
int wifiListDragCurrentScroll = 0;
String wifiUiMessage = "";

bool wifiUiTouchActive = false;
bool wifiUiTouchMoved = false;
int wifiUiTouchStartX = 0;
int wifiUiTouchStartY = 0;

bool wifiHeaderTouchActive = false;
bool wifiHeaderTouchValid = false;
int wifiHeaderTouchStartX = 0;
int wifiHeaderTouchStartY = 0;

bool applicationLauncherTouchActive = false;
bool applicationLauncherTouchMoved = false;
int applicationLauncherTouchStartX = 0;
int applicationLauncherTouchStartY = 0;
String applicationLauncherMessage = "";

bool wifiPendingWeatherRefresh = false;

WiFiSettingsRenderer wifiSettingsRenderer(
    displayManager, headerRenderer, textRenderer, WIFI_SETTINGS_COLOURS);
ForecastModeSelectorRenderer forecastModeSelectorRenderer(
    displayManager, textRenderer, FORECAST_MODE_SELECTOR_COLOURS);

const ForecastListItemRenderer::Colours FORECAST_LIST_ITEM_COLOURS = {
  BG, UI_INK, UI_INK, UI_INK, UI_INK, UI_INK
};
ForecastListItemRenderer forecastListItemRenderer(
  displayManager, FORECAST_LIST_ITEM_COLOURS);
ForecastListViewportRenderer forecastListViewportRenderer(
  forecastListItemRenderer);

const ForecastGraphRenderer::Colours FORECAST_GRAPH_COLOURS = {
  UI_INK, ACCENT, ACCENT, UI_INK, UI_INK, ACCENT
};
ForecastGraphRenderer forecastGraphRenderer(
  forecastListItemRenderer, FORECAST_GRAPH_COLOURS);

const WeatherMetricCardRenderer::Colours WEATHER_METRIC_CARD_COLOURS = {
  BG, UI_INK, UI_INK, UI_INK
};
WeatherMetricCardRenderer weatherMetricCardRenderer(
  WEATHER_METRIC_CARD_COLOURS);
WeatherMetricsViewportRenderer weatherMetricsViewportRenderer(
  weatherMetricCardRenderer);

const WeatherConditionsRenderer::Colours WEATHER_CONDITIONS_COLOURS = {
  UI_INK, UI_INK, UI_INK, UI_INK
};

// =====================================================
// WEATHER DATA
// =====================================================

// These aliases are a temporary compatibility facade for the existing Weather
// presentation code. The service owns the actual current/forecast objects.
String& weatherCity = weatherCurrentData.city;
String& weatherDescription = weatherCurrentData.description;
String& weatherIcon = weatherCurrentData.icon;
bool& weatherAvailable = weatherCurrentData.available;

float& temperature = weatherCurrentData.temperature;
float& feelsLike = weatherCurrentData.feelsLike;
int& humidity = weatherCurrentData.humidity;
int& pressure = weatherCurrentData.pressure;
float& windSpeedKmh = weatherCurrentData.windSpeedKmh;
int& cloudCover = weatherCurrentData.cloudCover;
int& visibilityMeters = weatherCurrentData.visibilityMeters;
float& rainLastHourMm = weatherCurrentData.rainLastHourMm;
bool& visibilityAvailable = weatherCurrentData.visibilityAvailable;

String& forecastError = weatherForecastData.error;
float& currentUvIndex = weatherForecastData.currentUvIndex;
float& currentRainProbability = weatherForecastData.currentRainProbability;
bool& uvIndexAvailable = weatherForecastData.uvIndexAvailable;
bool& rainProbabilityAvailable = weatherForecastData.rainProbabilityAvailable;
bool& forecastAvailable = weatherForecastData.available;
int& forecastCount = weatherForecastData.count;


bool loadDemoWeatherData();

const int ITEMS_PER_PAGE = 3;

const int WEATHER_METRIC_COUNT = WeatherMetricCatalog::Count;
const int WEATHER_METRICS_VISIBLE = 3;
const int WEATHER_METRIC_PANEL_TOP = 39;
const int WEATHER_METRIC_PANEL_HEIGHT = 177;
const int WEATHER_METRIC_PANEL_BOTTOM =
  WEATHER_METRIC_PANEL_TOP + WEATHER_METRIC_PANEL_HEIGHT;
const int WEATHER_METRIC_CARD_HEIGHT = WeatherMetricCardRenderer::CARD_HEIGHT;
const int WEATHER_METRIC_ROW_STEP = 59;
// Shared elastic-scroll tuning for metric cards and hourly forecast.
// Keep the overscroll cap below half a row step, so release snaps to the boundary.
const int SCROLL_OVERSCROLL_MAX_PX = 20;
const int SCROLL_OVERSCROLL_RESISTANCE_DIVISOR = 3;
const int WEATHER_METRIC_ROW_TOP = 2;

// The 64 px provider icon sits left of the current temperature. PNGdec reads
// the cached 2x source and the line callback downsamples it without allocating.
const int WEATHER_CONDITION_ICON_X = 61;
const int WEATHER_CONDITION_ICON_Y = 66;
const int WEATHER_CONDITION_ICON_SIZE = 70;

// Keep three 44 px list cards with 4 px gaps visible through y=212.
// The List/Graph selector occupies the area above y=70; the scrolling
// viewport starts at y=72 so rows can never overwrite the fixed controls.
const int FORECAST_VIEWPORT_TOP = 72;
const int FORECAST_VIEWPORT_HEIGHT = 144;
const int FORECAST_VIEWPORT_BOTTOM =
  FORECAST_VIEWPORT_TOP + FORECAST_VIEWPORT_HEIGHT;

// Keep the shared header and footer fixed while animating only the main body.
const int TAB_TRANSITION_CONTENT_TOP = 37;
const int TAB_TRANSITION_FOOTER_HEIGHT = 12;

// The first row begins directly below the 2 px gap under the selector.
const int FORECAST_ROW_TOP = FORECAST_VIEWPORT_TOP;

// The forecast canvas now begins 8 px lower. Preserve the chart's established
// physical graph position by applying the inverse offset within that canvas.
const int FORECAST_GRAPH_CANVAS_ORIGIN_Y = -8;
const int FORECAST_GRAPH_DISPLAY_ORIGIN_Y =
  FORECAST_VIEWPORT_TOP + FORECAST_GRAPH_CANVAS_ORIGIN_Y;
const int FORECAST_ROW_STEP = 48;
const int FORECAST_GRAPH_POINTS = 6;
const int FORECAST_GRAPH_POINT_SPACING = 56;
const unsigned long FORECAST_GRAPH_HOLD_MS = 220UL;
const int FORECAST_GRAPH_TOUCH_INTENT_PX = 8;
const int FORECAST_GRAPH_DRAG_START_PX = 4;
const int FORECAST_GRAPH_TAB_SWIPE_PX = 55;
const int FORECAST_CONTROL_TOP = 38;
const int FORECAST_CONTROL_BOTTOM = 70;
const int FORECAST_CONTROL_HIT_TOP = 36;
const int FORECAST_CONTROL_HIT_BOTTOM = 72;
// Generous movement tolerance keeps taps reliable while still allowing a
// deliberate horizontal tab swipe to cancel a control press.
const int FORECAST_CONTROL_TAP_SLOP_PX = 40;
// Limit full-viewport bitmap transfers during fast finger drags.
const unsigned long FORECAST_DRAG_RENDER_INTERVAL_MS = 50UL;

uint8_t forecastControlTouch = 0; // 3=list, 4=graph.
int forecastControlStartX = 0;
int forecastControlStartY = 0;

// Report the actual heap region of an Arduino_Canvas framebuffer.
// Arduino_GFX currently allocates its ESP32 canvas buffer with aligned_alloc(),
// which does not explicitly request PSRAM; inspect the returned pointer instead.
void reportCanvasAllocation(
  const char* label,
  Arduino_Canvas* canvas,
  size_t requestedBytes
) {
  if (canvas == nullptr) {
    DEBUG_PRINTF("Memory: %s canvas unavailable\n", label);
    return;
  }

  void* buffer = canvas->getFramebuffer();
  if (buffer == nullptr) {
    DEBUG_PRINTF(
      "Memory: %s canvas has no framebuffer (requested %u bytes)\n",
      label,
      (unsigned)requestedBytes);
    return;
  }

  const char* region = esp_ptr_external_ram(buffer)
    ? "PSRAM"
    : "not PSRAM (internal/other heap)";
  DEBUG_PRINTF(
    "Memory: %s framebuffer=%p, %u bytes, region=%s\n",
    label,
    buffer,
    (unsigned)requestedBytes,
    region);
}

void reportHeapSummary() {
  DEBUG_PRINTF(
    "Memory summary: PSRAM total=%u free=%u; internal 8-bit free=%u, largest block=%u\n",
    (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}

String getHeaderTitleText() {
  // The shared header identifies the active app, not the current location.
  return "Weather";
}

String getWeatherLocationText() {
  // Prefer the resolved IP location in the content area. The weather provider's
  // name may identify a nearby reporting station or a larger city.
  String label =
    locationDisplayName.length() > 0 ? locationDisplayName : weatherCity;

  // Before either response arrives, use the configured city.
  if (label.length() == 0) {
    label = String(CITY);
    const int comma = label.indexOf(',');
    if (comma > 0) {
      label = label.substring(0, comma);
    }
  }

  label.trim();
  return label;
}

void drawHeaderClockOnly(const String& clockText) {
  headerRenderer.drawClockOnly(clockText, BG, UI_INK, UI_INK);
  lastHeaderClock = clockText;
}

void updateWiFiSignalIndicator() {
  const uint32_t nowMs = millis();
  const bool connected = connectivityManager.isConnected();

  if (!connected) {
    wifiSignalMeter.reset();
    wifiSignalLastSampleMs = 0;
  } else if (wifiSignalLastSampleMs == 0 ||
             nowMs - wifiSignalLastSampleMs >= 1000UL) {
    wifiSignalMeter.addSample(WiFi.RSSI());
    wifiSignalLastSampleMs = nowMs;
  }

  const uint8_t strengthLevel =
      connected ? wifiSignalMeter.level() : 0;
  if (headerRenderer.setWiFiStatus(connected, strengthLevel)) {
    headerRenderer.drawWiFiIndicator(BG, UI_INK, UI_INK);
  }
}

void refreshHeaderClock() {
  String clockText = timeService.headerClockText();
  if (clockText != lastHeaderClock) {
    drawHeaderClockOnly(clockText);
  }
}

void maybeSyncTimeFromAPI() {
#if USE_DEMO_WEATHER_DATA
  return;
#else
  const TimeService::SyncResult result =
    timeService.syncIfDue(connectivityManager.isConnected(), millis());

  // The header redraw cache is presentation state. Invalidate it only after
  // the service has successfully changed the device clock.
  if (result == TimeService::SyncResult::Synced) {
    lastHeaderClock = "";
  }
#endif
}

// =====================================================

// =====================================================
// HEADER
// =====================================================

void drawHeader() {
  const String clockText = timeService.headerClockText();
  headerRenderer.draw(
    getHeaderTitleText(), clockText, BG, UI_INK, UI_INK);
  lastHeaderClock = clockText;
}

void drawTabIndicator() {
  int y = displayManager.height() - 8;
  displayManager.target()->fillRoundRect(displayManager.width() / 2 - 24, y, 48, 4, 2, BG);
  displayManager.target()->fillRoundRect(displayManager.width() / 2 - 24 + (currentTab * 24), y, 24, 4, 2, UI_ORANGE);
}

// =====================================================
// LOADING AND ERROR SCREENS
// =====================================================

void drawLoading(const String& message) {
  const String clockText = timeService.headerClockText();
  statusScreenRenderer.drawLoading(
    getHeaderTitleText(), clockText, message, STATUS_SCREEN_COLOURS);
  lastHeaderClock = clockText;
}

void drawError(const String& message) {
  const String clockText = timeService.headerClockText();
  statusScreenRenderer.drawError(
    getHeaderTitleText(), clockText, message, STATUS_SCREEN_COLOURS);
  lastHeaderClock = clockText;
}

// =====================================================
// OPENWEATHER ICON PNG DOWNLOAD + DECODING
// Uses official 2x OpenWeather PNG assets, downsampled for the compact view:
// Install the "PNGdec" library by Larry Bank from Arduino Library Manager.
// =====================================================

PNG weatherPngDecoder;

enum class WeatherPngDrawMode : uint8_t {
  FindVisibleBounds,
  DrawCropped
};

struct WeatherPngDrawContext {
  WeatherPngDrawMode mode;
  int x;
  int y;
  int cropLeft;
  int cropTop;
  int cropWidth;
  int cropHeight;
  int outputWidth;
  int outputHeight;
  int visibleLeft;
  int visibleTop;
  int visibleRight;
  int visibleBottom;
  bool hasVisiblePixels;
  uint32_t backgroundBGR;
};

uint8_t weatherPngGrayscaleSample(const PNGDRAW* pDraw, int x) {
  switch (pDraw->iBpp) {
    case 8:
      return pDraw->pPixels[x];
    case 4: {
      const uint8_t packed = pDraw->pPixels[x >> 1];
      return (x & 1) == 0 ? (uint8_t)(packed >> 4)
                          : (uint8_t)(packed & 0x0f);
    }
    case 2: {
      const uint8_t packed = pDraw->pPixels[x >> 2];
      const int shift = 6 - ((x & 3) * 2);
      return (uint8_t)((packed >> shift) & 0x03);
    }
    case 1: {
      const uint8_t packed = pDraw->pPixels[x >> 3];
      const int shift = 7 - (x & 7);
      return (uint8_t)((packed >> shift) & 0x01);
    }
    default:
      return 0;
  }
}

bool weatherPngPixelIsVisible(
  const PNGDRAW* pDraw,
  int x,
  const uint8_t* alphaMask
) {
  // PNGdec's alpha-mask helper handles per-pixel alpha and indexed palette
  // transparency. Its colour-key transparency is stored separately, so test
  // those two pixel formats against that key explicitly.
  if (weatherPngDecoder.hasAlpha() &&
      pDraw->iPixelType == PNG_PIXEL_TRUECOLOR) {
    const uint8_t* pixel = pDraw->pPixels + x * 3;
    const uint32_t colour =
      ((uint32_t)pixel[0] << 16) |
      ((uint32_t)pixel[1] << 8) |
      (uint32_t)pixel[2];
    return colour != (weatherPngDecoder.getTransparentColor() & 0xFFFFFFUL);
  }

  if (weatherPngDecoder.hasAlpha() &&
      pDraw->iPixelType == PNG_PIXEL_GRAYSCALE) {
    return weatherPngGrayscaleSample(pDraw, x) !=
      (uint8_t)weatherPngDecoder.getTransparentColor();
  }

  return (alphaMask[x >> 3] &
          (uint8_t)(0x80U >> (x & 7))) != 0;
}

int drawWeatherPngLine(PNGDRAW* pDraw) {
  if (pDraw == nullptr) return 0;
  WeatherPngDrawContext* context =
    (WeatherPngDrawContext*)pDraw->pUser;
  if (context == nullptr ||
      pDraw->iWidth <= 0 || pDraw->iWidth > 100) {
    return 0;
  }

  if (context->mode == WeatherPngDrawMode::FindVisibleBounds) {
    // PNGdec returns one bit per pixel: set bits are opaque enough to retain.
    // A low threshold keeps anti-aliased icon edges while dropping transparent
    // canvas padding. The fixed mask covers the maximum supported 100 px row.
    uint8_t alphaMask[13] = {};
    weatherPngDecoder.getAlphaMask(pDraw, alphaMask, 24);

    for (int x = 0; x < pDraw->iWidth; ++x) {
      const bool visible =
        weatherPngPixelIsVisible(pDraw, x, alphaMask);
      if (!visible) continue;

      if (!context->hasVisiblePixels) {
        context->visibleLeft = x;
        context->visibleRight = x;
        context->visibleTop = pDraw->y;
        context->visibleBottom = pDraw->y;
        context->hasVisiblePixels = true;
      } else {
        if (x < context->visibleLeft) context->visibleLeft = x;
        if (x > context->visibleRight) context->visibleRight = x;
        if (pDraw->y < context->visibleTop) {
          context->visibleTop = pDraw->y;
        }
        if (pDraw->y > context->visibleBottom) {
          context->visibleBottom = pDraw->y;
        }
      }
    }
    return 1;
  }

  Arduino_GFX* target = displayManager.target();
  if (target == nullptr ||
      context->cropWidth <= 0 || context->cropHeight <= 0 ||
      context->outputWidth <= 0 || context->outputWidth > 70 ||
      context->outputHeight <= 0 || context->outputHeight > 70) {
    return 0;
  }

  uint16_t sourceLine[100];
  uint16_t scaledLine[70];
  weatherPngDecoder.getLineAsRGB565(
    pDraw,
    sourceLine,
    PNG_RGB565_LITTLE_ENDIAN,
    context->backgroundBGR);

  // PNGdec's RGB565 conversion blends per-pixel alpha and indexed palette
  // transparency. Its truecolour/grayscale colour-key variants need this
  // explicit replacement so transparent pixels inside the crop stay paper.
  if (weatherPngDecoder.hasAlpha() &&
      (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR ||
       pDraw->iPixelType == PNG_PIXEL_GRAYSCALE)) {
    const uint8_t backgroundRed =
      (uint8_t)(context->backgroundBGR & 0xff);
    const uint8_t backgroundGreen =
      (uint8_t)((context->backgroundBGR >> 8) & 0xff);
    const uint8_t backgroundBlue =
      (uint8_t)((context->backgroundBGR >> 16) & 0xff);
    const uint16_t background565 =
      ((uint16_t)(backgroundRed & 0xf8) << 8) |
      ((uint16_t)(backgroundGreen & 0xfc) << 3) |
      ((uint16_t)backgroundBlue >> 3);
    const uint32_t transparentColour =
      weatherPngDecoder.getTransparentColor() & 0xFFFFFFUL;

    for (int sourceX = 0; sourceX < pDraw->iWidth; ++sourceX) {
      bool isTransparent = false;
      if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR) {
        const uint8_t* pixel = pDraw->pPixels + sourceX * 3;
        const uint32_t colour =
          ((uint32_t)pixel[0] << 16) |
          ((uint32_t)pixel[1] << 8) |
          (uint32_t)pixel[2];
        isTransparent = colour == transparentColour;
      } else {
        isTransparent =
          weatherPngGrayscaleSample(pDraw, sourceX) ==
          (uint8_t)transparentColour;
      }
      if (isTransparent) sourceLine[sourceX] = background565;
    }
  }

  // Sample only within the alpha-derived bounding box. The cropped bounds are
  // scaled proportionally, so the original icon aspect ratio is preserved.
  for (int outputY = 0; outputY < context->outputHeight; ++outputY) {
    const int sourceY = context->cropTop +
      outputY * context->cropHeight / context->outputHeight;
    if (sourceY != pDraw->y) continue;

    for (int outputX = 0; outputX < context->outputWidth; ++outputX) {
      const int sourceX = context->cropLeft +
        outputX * context->cropWidth / context->outputWidth;
      scaledLine[outputX] = sourceLine[sourceX];
    }

    target->draw16bitRGBBitmap(
      context->x,
      context->y + outputY,
      scaledLine,
      context->outputWidth,
      1);
  }

  return 1;
}

// This transport adapter downloads bytes only. WeatherApp calls the asset
// preparation step after data retrieval and before selecting a screen to draw.
bool downloadOpenWeatherIconPNG(
  const String& iconCode,
  uint8_t** downloadedData,
  size_t* downloadedLength
) {
  if (downloadedData == nullptr || downloadedLength == nullptr) {
    return false;
  }

  *downloadedData = nullptr;
  *downloadedLength = 0;

#if USE_DEMO_WEATHER_DATA
  (void)iconCode;
  return false;  // Keep demo mode completely offline.
#endif

  if (iconCode.length() != WeatherAssetCache::IconCodeLength) {
    DEBUG_PRINTF("Invalid OpenWeather icon code: %s\n", iconCode.c_str());
    return false;
  }

  String url =
    "https://openweathermap.org/img/wn/" + iconCode + "@2x.png";
  DEBUG_PRINTF("Downloading OpenWeather icon %s\n", iconCode.c_str());

  HttpRequestClient http;
  if (!http.begin(
        url, 10000, 10000,
        HttpRequestClient::TlsPolicy::InsecureCompatibility)) {
    DEBUG_PRINTLN("Could not initialise OpenWeather icon request");
    return false;
  }

  int status = http.get();
  if (status != HTTP_CODE_OK) {
    DEBUG_PRINTF("OpenWeather icon HTTP status: %d\n", status);
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength < WeatherAssetCache::MinPngBytes ||
      contentLength > WeatherAssetCache::MaxPngBytes) {
    DEBUG_PRINTF("Unexpected OpenWeather icon size: %d bytes\n", contentLength);
    http.end();
    return false;
  }

  uint8_t* newData = (uint8_t*)malloc((size_t)contentLength);
  if (newData == nullptr) {
    DEBUG_PRINTLN("Not enough RAM to cache OpenWeather icon");
    http.end();
    return false;
  }

  Stream* stream = http.getStreamPtr();
  if (stream == nullptr) {
    DEBUG_PRINTLN("OpenWeather icon response stream unavailable");
    http.end();
    free(newData);
    return false;
  }

  size_t bytesRead = 0;
  unsigned long startedAt = millis();

  while (bytesRead < (size_t)contentLength &&
         millis() - startedAt < 8000UL) {
    int available = stream->available();

    if (available > 0) {
      size_t remaining = (size_t)contentLength - bytesRead;
      size_t toRead =
        (size_t)available < remaining ? (size_t)available : remaining;
      int count = stream->readBytes(newData + bytesRead, toRead);
      if (count > 0) {
        bytesRead += (size_t)count;
      }
    } else if (!http.connected()) {
      break;
    } else {
      delay(1);
    }
  }

  http.end();

  if (bytesRead != (size_t)contentLength) {
    DEBUG_PRINTF(
      "OpenWeather icon download incomplete: %u of %d bytes\n",
      (unsigned)bytesRead,
      contentLength);
    free(newData);
    return false;
  }

  *downloadedData = newData;
  *downloadedLength = bytesRead;
  return true;
}

bool prepareWeatherIconAsset() {
#if USE_DEMO_WEATHER_DATA
  return false;
#endif

  if (weatherIcon.length() != WeatherAssetCache::IconCodeLength) {
    DEBUG_PRINTF("Invalid OpenWeather icon code: %s\n", weatherIcon.c_str());
    return false;
  }

  if (weatherAssetCache.contains(weatherIcon.c_str())) {
    return true;
  }

  // Do not attempt a request from the display path or without platform connectivity.
  if (!connectivityManager.isConnected()) {
    DEBUG_PRINTLN("Skipping OpenWeather icon preparation: network unavailable");
    return false;
  }

  uint8_t* downloadedData = nullptr;
  size_t downloadedLength = 0;
  if (!downloadOpenWeatherIconPNG(
        weatherIcon, &downloadedData, &downloadedLength)) {
    return false;
  }

  if (!weatherAssetCache.store(
        weatherIcon.c_str(), downloadedData, downloadedLength)) {
    DEBUG_PRINTLN("OpenWeather icon response was not a valid PNG asset");
    free(downloadedData);
    return false;
  }

  DEBUG_PRINTF(
    "Cached OpenWeather icon %s (%u bytes)\n",
    weatherIcon.c_str(),
    (unsigned)downloadedLength);
  return true;
}

// Drawing reads only an already-prepared cache entry. It never starts HTTP,
// allocates a download buffer or changes cache ownership.
int drawOpenWeatherIcon(int x, int y, int displaySize) {
  size_t iconLength = 0;
  uint8_t* iconData = weatherAssetCache.get(
    weatherIcon.c_str(), &iconLength);
  if (iconData == nullptr || iconLength == 0) {
    return 0;
  }

  int openResult = weatherPngDecoder.openRAM(
    iconData,
    (int)iconLength,
    drawWeatherPngLine);

  if (openResult != PNG_SUCCESS) {
    DEBUG_PRINTF("OpenWeather PNG open failed: %d\n", openResult);
    return 0;
  }

  int iconWidth = weatherPngDecoder.getWidth();
  int iconHeight = weatherPngDecoder.getHeight();

  if (iconWidth <= 0 || iconHeight <= 0 ||
      iconWidth > 100 || iconHeight > 100) {
    DEBUG_PRINTF("OpenWeather PNG dimensions unexpected: %d x %d\n", iconWidth, iconHeight);
    weatherPngDecoder.close();
    return 0;
  }

  if (displaySize <= 0) {
    weatherPngDecoder.close();
    return 0;
  }

  WeatherPngDrawContext context = {};
  context.mode = WeatherPngDrawMode::FindVisibleBounds;
  context.visibleLeft = iconWidth;
  context.visibleTop = iconHeight;
  context.visibleRight = -1;
  context.visibleBottom = -1;
  // Blend transparent PNG pixels against the supplied paper colour.
  // PNGdec expects 00BBGGRR, so #FEF3E2 becomes 0x00E2F3FE.
  context.backgroundBGR = 0x00E2F3FEUL;

  // First pass finds the content bounds from PNG alpha, not colour. This
  // preserves white/light icon details while removing transparent canvas
  // padding. PNGdec can decode the same open image again for the render pass.
  int scanResult = weatherPngDecoder.decode(&context, PNG_FAST_PALETTE);
  if (scanResult != PNG_SUCCESS) {
    DEBUG_PRINTF("OpenWeather PNG bounds scan failed: %d\n", scanResult);
    weatherPngDecoder.close();
    return 0;
  }

  // If the image has no opaque pixels, retain the full source canvas as a safe
  // fallback rather than drawing an invalid zero-sized crop.
  if (context.hasVisiblePixels) {
    context.cropLeft = context.visibleLeft;
    context.cropTop = context.visibleTop;
    context.cropWidth =
      context.visibleRight - context.visibleLeft + 1;
    context.cropHeight =
      context.visibleBottom - context.visibleTop + 1;
  } else {
    context.cropLeft = 0;
    context.cropTop = 0;
    context.cropWidth = iconWidth;
    context.cropHeight = iconHeight;
  }

  const int longestCropSide =
    context.cropWidth > context.cropHeight
      ? context.cropWidth
      : context.cropHeight;
  context.outputWidth =
    (context.cropWidth * displaySize + longestCropSide / 2) /
    longestCropSide;
  context.outputHeight =
    (context.cropHeight * displaySize + longestCropSide / 2) /
    longestCropSide;
  if (context.outputWidth <= 0 || context.outputHeight <= 0 ||
      context.outputWidth > 70 || context.outputHeight > 70) {
    weatherPngDecoder.close();
    return 0;
  }

  // Centre the cropped artwork inside the existing square icon area.
  context.x = x + (displaySize - context.outputWidth) / 2;
  context.y = y + (displaySize - context.outputHeight) / 2;
  context.mode = WeatherPngDrawMode::DrawCropped;

  const int drawResult =
    weatherPngDecoder.decode(&context, PNG_FAST_PALETTE);
  if (drawResult != PNG_SUCCESS) {
    DEBUG_PRINTF("OpenWeather PNG cropped draw failed: %d\n", drawResult);
  }

  weatherPngDecoder.close();
  return drawResult == PNG_SUCCESS ? context.outputHeight : 0;
}

// =====================================================
// CURRENT WEATHER SCREEN
// =====================================================

int weatherMetricsPanelX() {
  return (displayManager.width() * 60) / 100;
}

int weatherMetricsPanelWidth() {
  return displayManager.width() - weatherMetricsPanelX() - 10;
}

String weatherMetricValue(WeatherMetricCatalog::MetricId metric) {
  switch (metric) {
    case WeatherMetricCatalog::RainChance:
      return rainProbabilityAvailable
        ? String((int)(currentRainProbability * 100.0f + 0.5f)) + "%"
        : "--";
    case WeatherMetricCatalog::Wind:
      // Keep the shared size-2 card value within its 114 px inner width.
      // Decimal precision is retained for normal speeds and dropped above
      // 99.9 km/h so a three-digit value still fits without shrinking type.
      return windSpeedKmh >= 100.0f
        ? String(windSpeedKmh, 0) + " km/h"
        : String(windSpeedKmh, 1) + " km/h";
    case WeatherMetricCatalog::UvIndex:
      return uvIndexAvailable ? String(currentUvIndex, 1) : "--";
    case WeatherMetricCatalog::Humidity:
      return String(humidity) + "%";
    case WeatherMetricCatalog::Pressure:
      return String(pressure) + " hPa";
    case WeatherMetricCatalog::CloudCover:
      return String(cloudCover) + "%";
    case WeatherMetricCatalog::Visibility:
      return visibilityAvailable
        ? String(visibilityMeters / 1000.0f, 1) + " km"
        : "--";
    case WeatherMetricCatalog::RainLastHour:
      return String(rainLastHourMm, 1) + " mm";
    case WeatherMetricCatalog::Invalid:
    default:
      return "--";
  }
}

// Weather owns metric identity and formatting. The viewport renderer only
// asks for visible records and composes their cards within the supplied target.
bool readWeatherMetricItem(
  const void* context,
  int index,
  WeatherMetricsViewportRenderer::Item& item
) {
  (void)context;
  const WeatherMetricCatalog::MetricId metric =
    WeatherMetricCatalog::metricForCard(index);
  if (metric == WeatherMetricCatalog::Invalid) return false;

  item.label = WeatherMetricCatalog::label(metric);
  item.value = weatherMetricValue(metric);
  return true;
}

// The sketch keeps ownership of the selected buffer and its flush/fallback
// lifecycle. Both the PSRAM canvas and direct-display path use the same
// renderer with coordinates relative to their respective viewport origins.
void drawWeatherMetricsViewport(int baseOffset, int scrollPixels) {
  const int panelWidth = weatherMetricsPanelWidth();

  if (displayManager.weatherMetricsCanvas() != nullptr &&
      !displayManager.renderingTabTransition()) {
    const WeatherMetricsViewportRenderer::Layout layout = {
      WEATHER_METRIC_COUNT,
      WEATHER_METRICS_VISIBLE,
      WEATHER_METRIC_ROW_TOP,
      WEATHER_METRIC_ROW_STEP,
      0,
      panelWidth,
      0,
      WEATHER_METRIC_PANEL_HEIGHT,
      BG
    };

    weatherMetricsViewportRenderer.draw(
      displayManager.weatherMetricsCanvas(),
      layout,
      baseOffset,
      scrollPixels,
      nullptr,
      readWeatherMetricItem);
    displayManager.weatherMetricsCanvas()->flush();
    return;
  }

  // Direct-render fallback when the PSRAM canvas is unavailable or the
  // screen is being composed on the tab-transition target.
  const WeatherMetricsViewportRenderer::Layout layout = {
    WEATHER_METRIC_COUNT,
    WEATHER_METRICS_VISIBLE,
    WEATHER_METRIC_ROW_TOP,
    WEATHER_METRIC_ROW_STEP,
    weatherMetricsPanelX(),
    panelWidth,
    WEATHER_METRIC_PANEL_TOP,
    WEATHER_METRIC_PANEL_HEIGHT,
    BG
  };

  weatherMetricsViewportRenderer.draw(
    displayManager.target(),
    layout,
    baseOffset,
    scrollPixels,
    nullptr,
    readWeatherMetricItem);
}

void drawWeatherMetricsPanel(int baseOffset, int scrollPixels) {
  int maxOffset = WeatherScrollState::maximumOffset(WEATHER_METRIC_COUNT, WEATHER_METRICS_VISIBLE);
  if (baseOffset < 0) baseOffset = 0;
  if (baseOffset > maxOffset) baseOffset = maxOffset;

  drawWeatherMetricsViewport(baseOffset, scrollPixels);
}

int maxWeatherMetricScrollPixels() {
  return WeatherScrollState::maximumScrollPixels(
    WEATHER_METRIC_COUNT,
    WEATHER_METRICS_VISIBLE,
    WEATHER_METRIC_ROW_STEP);
}

void drawWeatherMetricsAtScrollPosition(int pixels) {
  // Keep the metric viewport mapping in the scroll-state model; the renderer
  // receives only the resolved row and within-row displacement.
  const WeatherScrollState::ViewportPosition position =
    WeatherScrollState::metricsViewportPosition(
      pixels,
      WEATHER_METRIC_COUNT,
      WEATHER_METRICS_VISIBLE,
      WEATHER_METRIC_ROW_STEP,
      SCROLL_OVERSCROLL_MAX_PX);
  drawWeatherMetricsViewport(position.baseOffset, position.scrollPixels);
}

void animateWeatherMetricsSnap(int startPixels, int targetOffset) {
  const int maxOffset = WeatherScrollState::maximumOffset(WEATHER_METRIC_COUNT, WEATHER_METRICS_VISIBLE);
  targetOffset = constrain(targetOffset, 0, maxOffset);
  int targetPixels = targetOffset * WEATHER_METRIC_ROW_STEP;

  if (startPixels != targetPixels && displayManager.weatherMetricsCanvas() != nullptr) {
    const int animationFrames = 6;
    const int animationDelayMs = 8;
    for (int frame = 1; frame <= animationFrames; frame++) {
      float progress = (float)frame / animationFrames;
      float easedProgress = progress * progress * (3.0f - 2.0f * progress);
      int pixels = startPixels +
        (int)((targetPixels - startPixels) * easedProgress);
      drawWeatherMetricsAtScrollPosition(pixels);
      delay(animationDelayMs);
    }
  } else if (startPixels != targetPixels) {
    drawWeatherMetricsAtScrollPosition(targetPixels);
  }

  weatherMetricOffset = targetOffset;
}

void animateWeatherMetricsScroll(int direction) {
  int targetOffset = weatherMetricOffset + direction;
  int maxOffset = WeatherScrollState::maximumOffset(WEATHER_METRIC_COUNT, WEATHER_METRICS_VISIBLE);

  if (targetOffset < 0 || targetOffset > maxOffset) return;

  // If memory is too constrained for the buffer, update the page in one
  // redraw rather than using a multi-frame direct-render animation.
  if (displayManager.weatherMetricsCanvas() == nullptr) {
    weatherMetricOffset = targetOffset;
    drawWeatherMetricsPanel(weatherMetricOffset, 0);
    return;
  }

  const int animationFrames = 7;
  const int animationDelayMs = 8;
  int startOffset = weatherMetricOffset;

  for (int frame = 1; frame <= animationFrames; frame++) {
    float progress = (float)frame / animationFrames;
    float easedProgress = progress * progress * (3.0f - 2.0f * progress);
    int scrollPixels = (int)(
      direction * WEATHER_METRIC_ROW_STEP * easedProgress);
    drawWeatherMetricsViewport(startOffset, scrollPixels);
    delay(animationDelayMs);
  }

  weatherMetricOffset = targetOffset;
}

void drawWeather() {
  currentTab = 0;
  displayManager.target()->fillScreen(BG);
  drawHeader();

  // The resolved location moves into the Current Conditions body. The shared
  // header now identifies the Weather app, and the 2x provider icon is drawn
  // at a compact 64 px size above the vertically stacked condition summary.
  drawOpenWeatherIcon(
    WEATHER_CONDITION_ICON_X,
    WEATHER_CONDITION_ICON_Y,
    WEATHER_CONDITION_ICON_SIZE);
  weatherConditionsRenderer.draw(
    getWeatherLocationText(),
    weatherDescription,
    String(temperature, 1) + " C",
    "Feels: " + String(feelsLike, 1) + "C",
    WEATHER_CONDITIONS_COLOURS);

  drawWeatherMetricsPanel(weatherMetricOffset, 0);
  drawTabIndicator();
}

// =====================================================
// FORECAST SCREEN
// =====================================================

// Weather supplies one forecast item at a time. The viewport renderer
// reuses its Item instance across visible rows, retaining String capacity.
bool readForecastListItem(
  const void* context,
  int index,
  ForecastListItemRenderer::Item& item
) {
  if (context == nullptr || index < 0 ||
      index >= WeatherForecastData::MaxRecords) {
    return false;
  }

  const WeatherForecastData* data =
    static_cast<const WeatherForecastData*>(context);
  if (index >= data->count) return false;

  const ForecastItem& source = data->items[index];
  const int separator = source.timeText.lastIndexOf(' ');
  item.time = separator >= 0
    ? source.timeText.substring(separator + 1)
    : source.timeText;
  item.description = source.description;
  item.temperature = source.temp;
  item.feelsLike = source.feelsLike;
  item.rainProbability = source.rainProbability;
  return true;
}

void drawForecastModeSelector() {
  forecastModeSelectorRenderer.draw(
    forecastGraphView
      ? ForecastModeSelectorRenderer::Mode::Graph
      : ForecastModeSelectorRenderer::Mode::List);
}

void drawForecastControls() {
  // Graph paging is gesture-based; the List/Graph selector stays in the same
  // position, with the same geometry in both modes.
  displayManager.target()->fillRect(
    0,
    FORECAST_CONTROL_TOP - 1,
    displayManager.width(),
    FORECAST_CONTROL_BOTTOM - (FORECAST_CONTROL_TOP - 1),
    BG);

  if (!forecastGraphView) {
    textRenderer.drawText("HOURLY", 8, 50, 1, UI_INK);
  }

  drawForecastModeSelector();
}

int maxForecastGraphScrollPixels() {
  return WeatherScrollState::maximumScrollPixels(
    forecastCount,
    FORECAST_GRAPH_POINTS,
    FORECAST_GRAPH_POINT_SPACING);
}

void syncForecastGraphOffsetFromScroll() {
  forecastGraphOffset =
    WeatherScrollState::forecastGraphOffsetForPixels(
      forecastGraphScrollPixels,
      forecastCount,
      FORECAST_GRAPH_POINTS,
      FORECAST_GRAPH_POINT_SPACING);
}

bool readForecastGraphItem(
  const void* context,
  int index,
  ForecastGraphRenderer::Item& item
) {
  if (context == nullptr || index < 0 ||
      index >= WeatherForecastData::MaxRecords) {
    return false;
  }
  const WeatherForecastData* data =
    static_cast<const WeatherForecastData*>(context);
  if (index >= data->count) return false;

  const ForecastItem& source = data->items[index];
  item.timeText = &source.timeText;
  item.description = &source.description;
  item.temperature = source.temp;
  item.rainProbability = source.rainProbability;
  return true;
}

void drawForecastGraph(Arduino_GFX* target, int originY) {
  if (target == nullptr || forecastCount <= 0) return;

  // Weather owns the current pixel offset and elastic-scroll policy. The
  // renderer receives the resulting position and owns only chart composition.
  const int maxScroll = maxForecastGraphScrollPixels();
  const int scroll = ScrollController::clampElasticPosition(
    forecastGraphScrollPixels,
    maxScroll,
    SCROLL_OVERSCROLL_MAX_PX);

  forecastGraphRenderer.draw(
    target,
    displayManager.width(),
    originY,
    forecastCount,
    scroll,
    &weatherForecastData,
    readForecastGraphItem);
}

// Copy only the graph body from the existing canvas. The canvas begins at
// screen y=72, below the selector, so the full viewport crop is safe.
bool flushForecastGraphCanvasBody() {
  if (displayManager.forecastCanvas() == nullptr) return false;
  uint16_t* framebuffer = displayManager.forecastCanvas()->getFramebuffer();
  if (framebuffer == nullptr) return false;

  const int graphRedrawTop = FORECAST_CONTROL_BOTTOM + 2;
  const int sourceTop = graphRedrawTop - FORECAST_VIEWPORT_TOP;
  const int bodyHeight = FORECAST_VIEWPORT_BOTTOM - graphRedrawTop;
  if (sourceTop < 0 || bodyHeight <= 0) return false;

  displayManager.target()->draw16bitRGBBitmap(
    0,
    graphRedrawTop,
    framebuffer + sourceTop * displayManager.width(),
    displayManager.width(),
    bodyHeight);
  return true;
}

// Render graph motion off-screen and copy one cropped bitmap to the LCD. This
// avoids flicker from dozens of direct draw calls and avoids repainting controls.
void redrawForecastGraphAtCurrentScroll() {
  if (!forecastGraphView) return;

  if (displayManager.forecastCanvas() != nullptr && !displayManager.renderingTabTransition()) {
    displayManager.forecastCanvas()->fillScreen(BG);
    if (forecastAvailable && forecastCount > 0) {
      drawForecastGraph(
        displayManager.forecastCanvas(),
        FORECAST_GRAPH_CANVAS_ORIGIN_Y);
    }
    if (flushForecastGraphCanvasBody()) return;
    displayManager.forecastCanvas()->flush();
    forecastModeSelectorRenderer.draw(ForecastModeSelectorRenderer::Mode::Graph);
    return;
  }

  // Safe no-canvas fallback. The normal hardware path uses the buffered branch.
  const int graphRedrawTop = FORECAST_CONTROL_BOTTOM + 2;
  if (graphRedrawTop >= FORECAST_VIEWPORT_BOTTOM) return;
  displayManager.target()->fillRect(
    0, graphRedrawTop, displayManager.width(),
    FORECAST_VIEWPORT_BOTTOM - graphRedrawTop, BG);
  if (forecastAvailable && forecastCount > 0) {
    drawForecastGraph(
      displayManager.target(),
      FORECAST_GRAPH_DISPLAY_ORIGIN_Y);
  }
}

void drawForecastGraphAtScrollPosition(int pixels) {
  const int maxScroll = maxForecastGraphScrollPixels();
  forecastGraphScrollPixels = ScrollController::clampElasticPosition(
    pixels, maxScroll, SCROLL_OVERSCROLL_MAX_PX);
  syncForecastGraphOffsetFromScroll();
  redrawForecastGraphAtCurrentScroll();
}

void animateForecastGraphSnap(int startPixels, int targetPixels) {
  const int maxScroll = maxForecastGraphScrollPixels();
  targetPixels = constrain(targetPixels, 0, maxScroll);

  if (startPixels != targetPixels) {
    const int animationFrames = 6;
    const int animationDelayMs = 8;
    for (int frame = 1; frame <= animationFrames; frame++) {
      const float progress = (float)frame / animationFrames;
      const float easedProgress =
        progress * progress * (3.0f - 2.0f * progress);
      const int pixels = startPixels +
        (int)((targetPixels - startPixels) * easedProgress);
      drawForecastGraphAtScrollPosition(pixels);
      delay(animationDelayMs);
    }
  }

  forecastGraphScrollPixels = targetPixels;
  syncForecastGraphOffsetFromScroll();
  redrawForecastGraphAtCurrentScroll();
}

void handleForecastControlTap(uint8_t control) {
  if (control != 3 && control != 4) return;

  // List and Graph are separate views of the hourly forecast. Switching
  // between them always starts at the first hour rather than carrying across
  // the other view's scroll position.
  weatherViewState.selectForecastMode(control == 4);
  forecastGraphDragCurrentScroll = 0;

  drawForecast();
  // Prevent a selector tap from being reinterpreted as a controller swipe.
  forecastGestureSuppressionStartedMs = millis();
  suppressForecastGesture = true;
}

void drawForecastFooter() {
  // Keep the footer minimal: only the current tab indicator is shown.
  drawTabIndicator();
}

// Render the moving region off-screen, then flush it to the LCD as one RGB565
// bitmap. This avoids the many separate SPI drawing operations that caused
// visible tearing/flicker during the earlier direct-to-display animation.
void drawForecastViewport(int baseOffset, int scrollPixels) {
  // Keep every renderer entry point safe, including touch-drag frames and
  // redraws immediately after a forecast refresh changes the record count.
  const int maxOffset = WeatherScrollState::maximumOffset(forecastCount, ITEMS_PER_PAGE);
  baseOffset = constrain(baseOffset, 0, maxOffset);
  // Negative scrollPixels is allowed only for a small resisted pull-down at
  // the first row; the item renderer clips the partially visible edge card.
  scrollPixels = constrain(
    scrollPixels,
    -SCROLL_OVERSCROLL_MAX_PX,
    FORECAST_ROW_STEP - 1);

  if (displayManager.forecastCanvas() != nullptr && !displayManager.renderingTabTransition()) {
    displayManager.forecastCanvas()->fillScreen(BG);
    if (forecastGraphView) {
      drawForecastGraph(displayManager.forecastCanvas(), 0);
      if (flushForecastGraphCanvasBody()) return;
      displayManager.forecastCanvas()->flush();
      forecastModeSelectorRenderer.draw(ForecastModeSelectorRenderer::Mode::Graph);
      return;
    }

    const ForecastListViewportRenderer::Layout layout = {
      forecastCount,
      ITEMS_PER_PAGE,
      FORECAST_ROW_TOP,
      FORECAST_ROW_STEP,
      FORECAST_VIEWPORT_TOP,
      FORECAST_VIEWPORT_HEIGHT
    };
    forecastListViewportRenderer.drawClippedViewport(
      displayManager.forecastCanvas(),
      layout,
      baseOffset,
      scrollPixels,
      &weatherForecastData,
      readForecastListItem);

    displayManager.forecastCanvas()->flush();
    return;
  }

  // Safe fallback if the canvas buffer could not be allocated: render the
  // three settled rows directly if the canvas buffer is unavailable.
  displayManager.target()->fillRect(
    0,
    FORECAST_VIEWPORT_TOP,
    displayManager.width(),
    FORECAST_VIEWPORT_HEIGHT,
    BG);

  if (forecastGraphView) {
    drawForecastGraph(displayManager.target(), FORECAST_VIEWPORT_TOP);
    return;
  }

  const ForecastListViewportRenderer::Layout layout = {
    forecastCount,
    ITEMS_PER_PAGE,
    FORECAST_ROW_TOP,
    FORECAST_ROW_STEP,
    FORECAST_VIEWPORT_TOP,
    FORECAST_VIEWPORT_HEIGHT
  };
  forecastListViewportRenderer.drawSettledFallback(
    displayManager.target(),
    layout,
    baseOffset,
    &weatherForecastData,
    readForecastListItem);
}

void drawForecast() {
  currentTab = 1;
  displayManager.target()->fillScreen(BG);
  drawHeader();

  if (!forecastAvailable || forecastCount == 0) {
    textRenderer.drawCenteredText(forecastError.length() ? forecastError : "No hourly data", 100, 1, MUTED);
    drawForecastControls();
    drawForecastFooter();
    return;
  }

  // Draw the viewport first so the graph controls can safely overlay its blank
  // top strip without being overwritten by the canvas flush.
  displayManager.target()->fillRect(
    0,
    FORECAST_VIEWPORT_TOP,
    displayManager.width(),
    FORECAST_VIEWPORT_HEIGHT,
    BG);
  drawForecastViewport(forecastOffset, 0);
  drawForecastControls();
  drawForecastFooter();
}

// Reset every scroll position whenever the user changes the main tab.
// This keeps each screen predictable when revisiting it, rather than restoring
// the previous scroll position from the last visit.
void resetScrollableViewPositions() {
  weatherViewState.resetScrollablePositions();

  // Keep the transient pixel positions in sync with their canonical offsets.
  metricsDragCurrentScroll = 0;
  forecastDragCurrentScroll = 0;
  forecastGraphDragCurrentScroll = 0;
}

// Render the destination tab off-screen, then slide it over the current tab.
// The viewport renderers bypass their own canvases during composition so they
// cannot flush partial content to the physical display mid-transition.
void animateTabTransition(int targetTab, int direction) {
  if (targetTab == currentTab) return;

  // Reset outgoing and incoming content before composing the destination frame.
  resetScrollableViewPositions();

  if (displayManager.tabTransitionCanvas() == nullptr) {
    if (targetTab == 0) drawWeather();
    else drawForecast();
    return;
  }

  Arduino_GFX* liveDisplay = displayManager.physicalDisplay();
  displayManager.beginTabTransitionRender();

  if (targetTab == 0) drawWeather();
  else drawForecast();

  displayManager.endTabTransitionRender();

  uint16_t* frameBuffer = displayManager.tabTransitionCanvas()->getFramebuffer();
  if (frameBuffer == nullptr) {
    // The composed frame is unavailable: redraw directly rather than leave
    // the previous tab visible.
    if (targetTab == 0) drawWeather();
    else drawForecast();
    return;
  }

  const int contentHeight = displayManager.height() -
    TAB_TRANSITION_CONTENT_TOP -
    TAB_TRANSITION_FOOTER_HEIGHT;
  const uint16_t* contentFrame = frameBuffer +
    ((size_t)TAB_TRANSITION_CONTENT_TOP * (size_t)displayManager.width());

  const int animationFrames = 8;
  const int animationDelayMs = 9;

  for (int frame = 1; frame < animationFrames; frame++) {
    float progress = (float)frame / animationFrames;
    float easedProgress = progress * progress * (3.0f - 2.0f * progress);
    int travel = (int)(displayManager.width() * easedProgress);
    int xOffset = direction > 0 ? displayManager.width() - travel : -displayManager.width() + travel;

    // Slide only the central content. Header, divider and footer remain fixed
    // while the incoming page covers the outgoing page from one side.
    liveDisplay->draw16bitRGBBitmap(
      xOffset,
      TAB_TRANSITION_CONTENT_TOP,
      (uint16_t*)contentFrame,
      displayManager.width(),
      contentHeight);
    delay(animationDelayMs);
  }

  // Finish at an exact alignment. The compact footer contains only the tab
  // indicator, so there is no changing range label to repaint.
  liveDisplay->draw16bitRGBBitmap(
    0,
    TAB_TRANSITION_CONTENT_TOP,
    (uint16_t*)contentFrame,
    displayManager.width(),
    contentHeight);
  drawTabIndicator();
}

// Scroll by one hourly record and redraw the complete forecast viewport
// through its off-screen canvas. The single bitmap flush prevents the tearing
// caused by clearing the LCD and painting each card in separate operations.
void animateForecastScroll(int direction) {
  const int maxOffset = WeatherScrollState::maximumOffset(forecastCount, ITEMS_PER_PAGE);
  const int targetOffset = forecastOffset + direction;
  if (targetOffset < 0 || targetOffset > maxOffset) return;

  forecastOffset = targetOffset;
  drawForecastViewport(forecastOffset, 0);
  drawForecastFooter();
}

// Convert a continuous pixel scroll position into the forecast's row offset
// plus the within-row displacement. Rendering still uses the same buffered
// viewport and a single bitmap flush.
int maxForecastScrollPixels() {
  return WeatherScrollState::maximumScrollPixels(
    forecastCount,
    ITEMS_PER_PAGE,
    FORECAST_ROW_STEP);
}

void drawForecastAtScrollPosition(int pixels) {
  // The scroll model resolves edge displacement and row bounds before the
  // viewport renderer receives its two-dimensional position.
  const WeatherScrollState::ViewportPosition position =
    WeatherScrollState::forecastListViewportPosition(
      pixels,
      forecastCount,
      ITEMS_PER_PAGE,
      FORECAST_ROW_STEP,
      SCROLL_OVERSCROLL_MAX_PX);

  forecastOffset = position.baseOffset;
  drawForecastViewport(position.baseOffset, position.scrollPixels);
}

void animateForecastSnap(int startPixels, int targetOffset) {
  const int maxOffset = WeatherScrollState::maximumOffset(forecastCount, ITEMS_PER_PAGE);
  targetOffset = constrain(targetOffset, 0, maxOffset);
  const int targetPixels = targetOffset * FORECAST_ROW_STEP;

  if (startPixels != targetPixels && displayManager.forecastCanvas() != nullptr) {
    const int animationFrames = 6;
    const int animationDelayMs = 8;
    for (int frame = 1; frame <= animationFrames; frame++) {
      const float progress = (float)frame / animationFrames;
      const float easedProgress =
        progress * progress * (3.0f - 2.0f * progress);
      const int pixels = startPixels +
        (int)((targetPixels - startPixels) * easedProgress);
      drawForecastAtScrollPosition(pixels);
      delay(animationDelayMs);
    }
  } else if (startPixels != targetPixels) {
    drawForecastAtScrollPosition(targetPixels);
  }

  forecastOffset = targetOffset;
  drawForecastViewport(forecastOffset, 0);
  drawForecastFooter();
}

// Raw I2C register access and six-byte packet decoding live in
// src/hardware/Cst816Reader.h. Keep all interpretation below in Weather.

// The CST816 coordinates are reported in the panel's portrait-native frame.
// This board is mounted in landscape. The observed gesture mapping corresponds
// to screenX = rawY, screenY = reversed rawX for the 320x240 logical display.
void touchCoordinatesToScreen(
  const TouchSample& sample,
  int& screenX,
  int& screenY
) {
  screenX = (int)sample.rawY;
  screenY = displayManager.height() - 1 - (int)sample.rawX;

  screenX = constrain(screenX, 0, displayManager.width() - 1);
  screenY = constrain(screenY, 0, displayManager.height() - 1);
}

void updateTouchDrag(const TouchSample& sample, const GestureObservation& observation) {
  const bool touchPresent = observation.touchPresent;
  int touchX = 0;
  int touchY = 0;
  if (touchPresent) {
    touchCoordinatesToScreen(sample, touchX, touchY);
  }

  // One diagnostic per touch lets us validate the landscape coordinate mapping
  // without flooding the Serial Monitor with every movement sample.
  if (observation.touchStarted) {
    DEBUG_PRINTF(
      "Touch coordinates raw=(%u,%u), screen=(%d,%d), tab=%d\n",
      (unsigned)sample.rawX,
      (unsigned)sample.rawY,
      touchX,
      touchY,
      currentTab);
  }

  // Don't claim a touch on the metric cards until its intent is clear. This
  // keeps a horizontal tab swipe starting over the cards from being consumed
  // as a vertical drag.
  const bool touchJustStarted = observation.touchStarted;

  // A new contact starts a new gesture sequence. Clear suppression left by a
  // previous drag so a legitimate horizontal swipe can reach tab navigation.
  // The stale completed gesture is not carried forward via lastHandledGesture.
  if (touchJustStarted) {
    interactionArbiter.beginContact();

    // The CST816 can emit a completed vertical gesture even when coordinate-
    // tracked dragging never activated. Remember where this contact began so
    // the legacy gesture fallback cannot scroll cards from other screen areas.
    metricsGestureStartedInsidePanel =
      currentTab == 0 &&
      touchX >= weatherMetricsPanelX() &&
      touchX < displayManager.width() &&
      touchY >= WEATHER_METRIC_PANEL_TOP &&
      touchY < WEATHER_METRIC_PANEL_BOTTOM;
    forecastGestureStartedInsideViewport =
      currentTab == 1 &&
      !forecastGraphView &&
      touchX >= 8 &&
      touchX < displayManager.width() - 8 &&
      touchY >= FORECAST_VIEWPORT_TOP &&
      touchY < FORECAST_VIEWPORT_BOTTOM;

    suppressMetricGesture = false;
    suppressForecastGesture = false;

    // A new physical contact begins a new gesture sequence. Do not let the
    // previous graph-release suppression latch consume this entire swipe.
    // Any gesture code already present at touch-down is treated as stale and
    // marked handled; a fresh event can be handled after the register clears.
    if (suppressGraphControllerGestureAfterTouch) {
      suppressGraphControllerGestureAfterTouch = false;
      graphGestureQuietPeriodActive = false;
      graphGestureSuppressionStartedMs = 0;
      graphGestureQuietStartedMs = 0;
      lastHandledGesture = observation.gesture;
      DEBUG_PRINTLN("Graph gesture suppression cleared at new touch-down");
    } else {
      lastHandledGesture = GestureCode::None;
    }

    // Only the List/Graph selector remains in the control strip.
    forecastControlTouch = 0;
    if (currentTab == 1 &&
        touchY >= FORECAST_CONTROL_HIT_TOP &&
        touchY < FORECAST_CONTROL_HIT_BOTTOM) {
      if (touchX >= 176 && touchX < 247) {
        forecastControlTouch = 3;
      } else if (touchX >= 247 && touchX < 320) {
        forecastControlTouch = 4;
      }
      if (forecastControlTouch != 0) {
        forecastControlStartX = touchX;
        forecastControlStartY = touchY;
        interactionArbiter.claim(InteractionOwner::ForecastControl);
      }
    }

    // A touch beginning in the graph body belongs to the graph until release.
    // This blocks the controller's coarse swipe event if coordinate tracking drops.
    forecastGraphTouchOwned =
      currentTab == 1 &&
      forecastGraphView &&
      touchX >= 0 &&
      touchX < displayManager.width() &&
      touchY >= FORECAST_CONTROL_BOTTOM + 2 &&
      touchY < FORECAST_VIEWPORT_BOTTOM;
    if (forecastGraphTouchOwned) {
      interactionArbiter.claim(InteractionOwner::ForecastGraph);
    }
    forecastGraphTouchCandidate = forecastGraphTouchOwned;
    forecastGraphQuickSwipeCandidate = false;
    forecastGraphQuickSwipeReady = false;
    forecastGraphTouchStartedMs = millis();
    forecastGraphTouchStartX = touchX;
    forecastGraphTouchStartY = touchY;
    forecastGraphTouchLastX = touchX;
    forecastGraphTouchLastY = touchY;
    forecastGraphTouchStartScroll = forecastGraphScrollPixels;
    forecastGraphDragActive = false;
    forecastGraphDragMoved = false;
    forecastGraphDragHasRendered = false;
    forecastGraphLastRenderedScroll = -1;
    forecastGraphDragStartX = touchX;
    forecastGraphDragStartScroll = forecastGraphScrollPixels;
    forecastGraphDragCurrentScroll = forecastGraphScrollPixels;
  }

  if (!metricsTouchDragActive &&
      !metricsTouchCandidate &&
      touchJustStarted &&
      currentTab == 0 &&
      displayManager.weatherMetricsCanvas() != nullptr) {
    bool insideMetricsPanel =
      touchX >= weatherMetricsPanelX() &&
      touchY >= WEATHER_METRIC_PANEL_TOP &&
      touchY < WEATHER_METRIC_PANEL_BOTTOM;

    if (insideMetricsPanel) {
      metricsTouchCandidate = true;
      metricsTouchDragMoved = false;
      metricsDragStartX = touchX;
      metricsDragStartY = touchY;
      metricsDragStartScroll =
        weatherMetricOffset * WEATHER_METRIC_ROW_STEP;
      metricsDragCurrentScroll = metricsDragStartScroll;
      DEBUG_PRINTF(
        "Metric touch candidate at screen (%d,%d), scroll=%d\n",
        touchX,
        touchY,
        metricsDragStartScroll);
    }
  }

  // A control tap is cancelled as soon as the finger moves significantly.
  // Such a movement remains available to the existing tab-swipe handler.
  if (forecastControlTouch != 0 && touchPresent &&
      (abs(touchX - forecastControlStartX) > FORECAST_CONTROL_TAP_SLOP_PX ||
       abs(touchY - forecastControlStartY) > FORECAST_CONTROL_TAP_SLOP_PX)) {
    forecastControlTouch = 0;
    interactionArbiter.release(InteractionOwner::ForecastControl);
  }

  // Forecast list dragging starts only in List mode and inside the list
  // viewport. Graph paging uses explicit buttons, not horizontal drag gestures.
  if (!forecastGraphView &&
      !forecastTouchDragActive &&
      !forecastTouchCandidate &&
      forecastControlTouch == 0 &&
      touchJustStarted &&
      currentTab == 1 &&
      displayManager.forecastCanvas() != nullptr &&
      forecastAvailable &&
      forecastCount > ITEMS_PER_PAGE &&
      touchX >= 8 &&
      touchX < displayManager.width() - 8 &&
      touchY >= FORECAST_VIEWPORT_TOP &&
      touchY < FORECAST_VIEWPORT_BOTTOM) {
    forecastTouchCandidate = true;
    forecastTouchDragMoved = false;
    forecastDragHasRendered = false;
    lastForecastDragRenderMs = 0;
    forecastDragLastRenderedScroll = -1;
    forecastDragStartX = touchX;
    forecastDragStartY = touchY;
    forecastDragStartScroll = forecastOffset * FORECAST_ROW_STEP;
    forecastDragCurrentScroll = forecastDragStartScroll;
    DEBUG_PRINTF(
      "Forecast touch candidate at screen (%d,%d), scroll=%d\n",
      touchX,
      touchY,
      forecastDragStartScroll);
  }

  if (metricsTouchCandidate && touchPresent) {
    int deltaX = touchX - metricsDragStartX;
    int deltaY = touchY - metricsDragStartY;
    int absX = abs(deltaX);
    int absY = abs(deltaY);
    const int intentThresholdPx = 8;

    if (absX >= intentThresholdPx && absX > absY) {
      // Horizontal intent: release the candidate so normal gesture handling
      // can change tabs when the CST816 reports its completed swipe.
      metricsTouchCandidate = false;
      DEBUG_PRINTF(
        "Metric touch resolved as horizontal swipe (dx=%d, dy=%d)\n",
        deltaX,
        deltaY);
    } else if (absY >= intentThresholdPx && absY >= absX) {
      // Vertical intent: take ownership of the touch and track the finger.
      metricsTouchCandidate = false;
      metricsTouchDragActive = true;
      interactionArbiter.claim(InteractionOwner::MetricPanel);
      DEBUG_PRINTF(
        "Metric vertical drag started (dx=%d, dy=%d)\n",
        deltaX,
        deltaY);
    }
  }

  if (metricsTouchDragActive && touchPresent) {
    int nextScroll = ScrollController::elasticPosition(
      ScrollController::applyDragDelta(
        metricsDragStartScroll,
        metricsDragStartY,
        touchY),
      maxWeatherMetricScrollPixels(),
      SCROLL_OVERSCROLL_MAX_PX,
      SCROLL_OVERSCROLL_RESISTANCE_DIVISOR);

    if (nextScroll != metricsDragCurrentScroll) {
      if (abs(nextScroll - metricsDragStartScroll) >= 3) {
        metricsTouchDragMoved = true;
      }
      metricsDragCurrentScroll = nextScroll;
      drawWeatherMetricsAtScrollPosition(metricsDragCurrentScroll);
    }
  }

  if (metricsTouchDragActive &&
      (!touchPresent || sample.event == 1)) {
    const int targetOffset =
      ScrollController::snapToStep(
        metricsDragCurrentScroll,
        WEATHER_METRIC_ROW_STEP,
        maxWeatherMetricScrollPixels()) /
      WEATHER_METRIC_ROW_STEP;
    DEBUG_PRINTF(
      "Metric drag released at scroll=%d; snapping to offset=%d; moved=%d\n",
      metricsDragCurrentScroll,
      targetOffset,
      metricsTouchDragMoved ? 1 : 0);
    animateWeatherMetricsSnap(metricsDragCurrentScroll, targetOffset);
    metricsTouchDragActive = false;
    // The CST816 can publish its completed gesture after touch-up. Ignore that
    // duplicate for a short grace period so it cannot trigger a second scroll.
    metricsGestureSuppressionStartedMs = millis();
    suppressMetricGesture = true;
  }

  // Forecast gestures use the same axis-intent arbitration as the metric
  // panel, but scroll the wider hourly list by tracking the finger's Y.
  if (forecastTouchCandidate && touchPresent) {
    const int deltaX = touchX - forecastDragStartX;
    const int deltaY = touchY - forecastDragStartY;
    const int absX = abs(deltaX);
    const int absY = abs(deltaY);
    const int intentThresholdPx = 8;

    if (absX >= intentThresholdPx && absX > absY) {
      // Leave horizontal intent to the existing tab-switch gesture handler.
      forecastTouchCandidate = false;
      DEBUG_PRINTF(
        "Forecast touch resolved as horizontal swipe (dx=%d, dy=%d)\n",
        deltaX,
        deltaY);
    } else if (absY >= intentThresholdPx && absY >= absX) {
      forecastTouchCandidate = false;
      forecastTouchDragActive = true;
      interactionArbiter.claim(InteractionOwner::ForecastList);
      DEBUG_PRINTF(
        "Forecast vertical drag started (dx=%d, dy=%d)\n",
        deltaX,
        deltaY);
    }
  }

  // Remember the last valid coordinates while a graph-originated contact is down.
  // If CST816 tracking drops on release, use the last confirmed movement rather
  // than trusting its coarse gesture code to decide whether to switch tabs.
  if (forecastGraphTouchOwned && touchPresent) {
    forecastGraphTouchLastX = touchX;
    forecastGraphTouchLastY = touchY;
  }

  if (forecastGraphTouchCandidate && touchPresent &&
      !forecastGraphDragActive) {
    const int deltaX = touchX - forecastGraphTouchStartX;
    const int deltaY = touchY - forecastGraphTouchStartY;
    const int absX = abs(deltaX);
    const int absY = abs(deltaY);
    const unsigned long heldMs = millis() - forecastGraphTouchStartedMs;

    // Only a large, clearly tracked horizontal movement before the hold delay
    // can become a tab swipe. Small movement/jitter no longer cancels graph drag.
    if (heldMs < FORECAST_GRAPH_HOLD_MS &&
        absX >= FORECAST_GRAPH_TAB_SWIPE_PX &&
        absX > absY) {
      forecastGraphTouchCandidate = false;
      forecastGraphQuickSwipeCandidate = true;
      forecastGraphQuickSwipeReady = true;
    } else if (heldMs >= FORECAST_GRAPH_HOLD_MS &&
               absX >= FORECAST_GRAPH_DRAG_START_PX &&
               absX > absY) {
      forecastGraphTouchCandidate = false;
      forecastGraphQuickSwipeCandidate = false;
      forecastGraphQuickSwipeReady = false;
      forecastGraphDragActive = true;
      forecastGraphDragMoved = false;
      forecastGraphDragHasRendered = false;
      forecastGraphLastRenderedScroll = -1;
      forecastGraphDragStartX = forecastGraphTouchStartX;
      forecastGraphDragStartScroll = forecastGraphTouchStartScroll;
      forecastGraphDragCurrentScroll = forecastGraphTouchStartScroll;
      DEBUG_PRINTF(
        "Graph horizontal hold-drag started at x=%d, scroll=%d\n",
        forecastGraphDragStartX,
        forecastGraphDragStartScroll);
    } else if (heldMs < FORECAST_GRAPH_HOLD_MS &&
               absY >= 20 && absY > absX) {
      // Strong vertical intent cancels chart panning, but the contact remains
      // owned so a controller dropout cannot turn it into a tab switch.
      forecastGraphTouchCandidate = false;
      forecastGraphQuickSwipeCandidate = false;
    }
  }

  if (forecastGraphQuickSwipeCandidate && touchPresent) {
    const int deltaX = touchX - forecastGraphTouchStartX;
    const int deltaY = touchY - forecastGraphTouchStartY;
    const unsigned long heldMs = millis() - forecastGraphTouchStartedMs;
    forecastGraphQuickSwipeReady =
      heldMs < FORECAST_GRAPH_HOLD_MS &&
      abs(deltaX) >= FORECAST_GRAPH_TAB_SWIPE_PX &&
      abs(deltaX) > abs(deltaY);
    if (heldMs >= FORECAST_GRAPH_HOLD_MS) {
      forecastGraphQuickSwipeCandidate = false;
      forecastGraphQuickSwipeReady = false;
    }
  }

  if (forecastGraphDragActive && touchPresent) {
    const int nextScroll = ScrollController::elasticPosition(
      ScrollController::applyDragDelta(
        forecastGraphDragStartScroll,
        forecastGraphDragStartX,
        touchX),
      maxForecastGraphScrollPixels(),
      SCROLL_OVERSCROLL_MAX_PX,
      SCROLL_OVERSCROLL_RESISTANCE_DIVISOR);

    if (nextScroll != forecastGraphDragCurrentScroll) {
      forecastGraphDragMoved =
        forecastGraphDragMoved ||
        abs(nextScroll - forecastGraphDragStartScroll) >= 3;
      forecastGraphDragCurrentScroll = nextScroll;
    }

    const unsigned long nowMs = millis();
    const bool positionNeedsRender =
      forecastGraphLastRenderedScroll != forecastGraphDragCurrentScroll;
    const bool renderDue =
      !forecastGraphDragHasRendered ||
      nowMs - lastForecastGraphDragRenderMs >= FORECAST_DRAG_RENDER_INTERVAL_MS;
    if (positionNeedsRender && renderDue) {
      drawForecastGraphAtScrollPosition(forecastGraphDragCurrentScroll);
      lastForecastGraphDragRenderMs = nowMs;
      forecastGraphDragHasRendered = true;
      forecastGraphLastRenderedScroll = forecastGraphDragCurrentScroll;
    }
  }

  if (forecastTouchDragActive && touchPresent) {
    const int nextScroll = ScrollController::elasticPosition(
      ScrollController::applyDragDelta(
        forecastDragStartScroll,
        forecastDragStartY,
        touchY),
      maxForecastScrollPixels(),
      SCROLL_OVERSCROLL_MAX_PX,
      SCROLL_OVERSCROLL_RESISTANCE_DIVISOR);

    // Track every changed position, but check the redraw deadline on every
    // touch sample—even when the finger has stopped moving. This ensures a
    // throttled position is eventually painted while the finger is stationary.
    if (nextScroll != forecastDragCurrentScroll) {
      forecastTouchDragMoved =
        forecastTouchDragMoved ||
        abs(nextScroll - forecastDragStartScroll) >= 3;
      forecastDragCurrentScroll = nextScroll;
    }

    const unsigned long nowMs = millis();
    const bool positionNeedsRender =
      forecastDragLastRenderedScroll != forecastDragCurrentScroll;
    const bool renderDue =
      !forecastDragHasRendered ||
      nowMs - lastForecastDragRenderMs >= FORECAST_DRAG_RENDER_INTERVAL_MS;
    if (positionNeedsRender && renderDue) {
      drawForecastAtScrollPosition(forecastDragCurrentScroll);
      lastForecastDragRenderMs = nowMs;
      forecastDragHasRendered = true;
      forecastDragLastRenderedScroll = forecastDragCurrentScroll;
    }
  }

  if (forecastTouchDragActive &&
      (!touchPresent || sample.event == 1)) {
    const int targetOffset =
      ScrollController::snapToStep(
        forecastDragCurrentScroll,
        FORECAST_ROW_STEP,
        maxForecastScrollPixels()) /
      FORECAST_ROW_STEP;
    DEBUG_PRINTF(
      "Forecast drag released at scroll=%d; snapping to offset=%d; moved=%d\n",
      forecastDragCurrentScroll,
      targetOffset,
      forecastTouchDragMoved ? 1 : 0);

    // Animate back from elastic overscroll to the nearest valid forecast row.
    animateForecastSnap(forecastDragCurrentScroll, targetOffset);
    forecastTouchDragActive = false;
    forecastGestureSuppressionStartedMs = millis();
    suppressForecastGesture = true;
  }

  if (forecastGraphDragActive &&
      (!touchPresent || sample.event == 1)) {
    const int maxScroll = maxForecastGraphScrollPixels();
    const int targetScroll = ScrollController::snapToStep(
      forecastGraphDragCurrentScroll,
      FORECAST_GRAPH_POINT_SPACING,
      maxScroll);

    DEBUG_PRINTF(
      "Graph drag released at scroll=%d; snapping to %d; moved=%d\n",
      forecastGraphDragCurrentScroll,
      targetScroll,
      forecastGraphDragMoved ? 1 : 0);

    animateForecastGraphSnap(forecastGraphDragCurrentScroll, targetScroll);
    forecastGraphDragActive = false;
    forecastGraphTouchCandidate = false;
    forecastGraphQuickSwipeCandidate = false;
    forecastGraphQuickSwipeReady = false;
    forecastGestureSuppressionStartedMs = millis();
    suppressForecastGesture = true;
  }

  if (forecastGraphTouchOwned && (!touchPresent || sample.event == 1)) {
    const int deltaX = forecastGraphTouchLastX - forecastGraphTouchStartX;
    const int deltaY = forecastGraphTouchLastY - forecastGraphTouchStartY;
    const bool confirmedQuickSwipe =
      !forecastGraphDragMoved &&
      !forecastGraphDragActive &&
      forecastGraphQuickSwipeReady &&
      abs(deltaX) >= FORECAST_GRAPH_TAB_SWIPE_PX &&
      abs(deltaX) > abs(deltaY) &&
      millis() - forecastGraphTouchStartedMs < FORECAST_GRAPH_HOLD_MS;

    forecastGraphTouchOwned = false;
    forecastGraphTouchCandidate = false;
    forecastGraphQuickSwipeCandidate = false;
    forecastGraphQuickSwipeReady = false;

    if (confirmedQuickSwipe && currentTab == 1 && deltaX > 0) {
      // Match the existing physical swipe mapping: a quick rightward swipe
      // returns from Forecast to Weather.
      animateTabTransition(0, -1);
    }

    // Keep suppressing the controller gesture until the gesture register has
    // returned to zero for a quiet interval. Tracking loss alone must not switch tabs.
    suppressGraphControllerGestureAfterTouch = true;
    graphGestureQuietPeriodActive = false;
    graphGestureSuppressionStartedMs = millis();
    graphGestureQuietStartedMs = 0;
    forecastGestureSuppressionStartedMs = millis();
    suppressForecastGesture = true;
  }

  if (forecastGraphTouchCandidate && (!touchPresent || sample.event == 1)) {
    forecastGraphTouchCandidate = false;
  }

  if (forecastTouchCandidate && (!touchPresent || sample.event == 1)) {
    forecastTouchCandidate = false;
  }

  // A tap that never crossed the intent threshold wasn't a drag; discard the
  // candidate but allow any final controller gesture to reach navigation.
  if (metricsTouchCandidate && (!touchPresent || sample.event == 1)) {
    metricsTouchCandidate = false;
  }

  if (forecastControlTouch != 0 &&
      (!touchPresent || sample.event == 1)) {
    const uint8_t control = forecastControlTouch;
    forecastControlTouch = 0;
    handleForecastControlTap(control);
  }

  if (!touchPresent) {
    interactionArbiter.releaseContact();
  }
}

// =====================================================
// WI-FI NETWORK MANAGEMENT UI
// =====================================================

String wifiConnectionStatusText() {
  if (connectivityManager.isConnected()) {
    return String("Connected: ") + WiFi.SSID();
  }
  if (connectivityManager.isConnecting()) {
    return "Connecting to Wi-Fi...";
  }
  if (wifiSsid != nullptr && wifiSsid[0] != '\0') {
    return String("Saved: ") + wifiSsid + " (offline)";
  }
  return "No network connected";
}

WiFiSettingsRenderer::Availability getWiFiProfileAvailability(
    const char* ssid,
    bool activeProfile) {
  if (activeProfile && connectivityManager.isConnected()) {
    return WiFiSettingsRenderer::Availability::Available;
  }

  // The badges use the last successfully completed scan, not the live
  // scanner state. A background refresh must not temporarily blank or change
  // values that are already displayed.
  if (!wifiAvailabilitySnapshot.hasSnapshot()) {
    return WiFiSettingsRenderer::Availability::Unknown;
  }

  return wifiAvailabilitySnapshot.contains(ssid)
      ? WiFiSettingsRenderer::Availability::Available
      : WiFiSettingsRenderer::Availability::Unavailable;
}

uint8_t wifiSettingsListItemCount() {
  if (wifiSettingsScreen == WiFiSettingsScreen::Profiles) {
    return wifiProfileStore.count();
  }
  if (wifiSettingsScreen == WiFiSettingsScreen::Scan) {
    return wifiNetworkScanner.count();
  }
  return 0;
}

int maxWiFiListScrollPixels() {
  const int maximumFirstIndex =
      static_cast<int>(wifiSettingsListItemCount()) -
      static_cast<int>(WiFiSettingsRenderer::ListRowsVisible);
  return (maximumFirstIndex > 0 ? maximumFirstIndex : 0) *
      WiFiSettingsRenderer::ListRowHeight;
}

void drawWiFiProfilesScreen() {
  PersistentWiFiProfileStore::Profile preferred = {};
  const bool hasPreferred = wifiProfileStore.getPreferred(preferred);
  const uint8_t count = wifiProfileStore.count();

  memset(wifiProfileRows, 0, sizeof(wifiProfileRows));
  for (uint8_t i = 0; i < count; ++i) {
    PersistentWiFiProfileStore::Profile profile = {};
    if (!wifiProfileStore.getAt(i, profile)) continue;
    strncpy(wifiProfileRows[i].ssid, profile.ssid,
            sizeof(wifiProfileRows[i].ssid) - 1);
    wifiProfileRows[i].preferred =
        hasPreferred && strcmp(preferred.ssid, profile.ssid) == 0;
    wifiProfileRows[i].active =
        wifiSsid != nullptr && strcmp(wifiSsid, profile.ssid) == 0;
    wifiProfileRows[i].availability =
        getWiFiProfileAvailability(profile.ssid, wifiProfileRows[i].active);
  }

  String message = wifiUiMessage;
  if (!wifiProfileStore.isLoaded()) {
    message = "Saved profiles unavailable";
  } else if (message.length() == 0 && count == 0) {
    message = "Scan to add a network";
  }

  wifiListScrollPositionPx = ScrollController::clampElasticPosition(
      wifiListScrollPositionPx,
      maxWiFiListScrollPixels(),
      SCROLL_OVERSCROLL_MAX_PX);

  wifiSettingsRenderer.drawProfiles(
      getHeaderTitleText(), timeService.headerClockText(),
      wifiProfileRows, count, wifiListScrollPositionPx,
      wifiConnectionStatusText(), message);
}

void drawWiFiScanScreen() {
  WiFiScanResult results[WiFiNetworkScanner::MaxResults] = {};
  const uint8_t count = wifiNetworkScanner.count();
  for (uint8_t i = 0; i < count; ++i) {
    wifiNetworkScanner.getAt(i, results[i]);
  }

  wifiListScrollPositionPx = ScrollController::clampElasticPosition(
      wifiListScrollPositionPx,
      maxWiFiListScrollPixels(),
      SCROLL_OVERSCROLL_MAX_PX);

  wifiSettingsRenderer.drawScan(
      getHeaderTitleText(), timeService.headerClockText(),
      results, count, wifiListScrollPositionPx,
      wifiNetworkScanner.state() == WiFiNetworkScanner::State::Scanning,
      wifiNetworkScanner.state() == WiFiNetworkScanner::State::Failed,
      wifiConnectionStatusText(), wifiUiMessage);
}

void drawWiFiProfileDetailScreen() {
  PersistentWiFiProfileStore::Profile preferred = {};
  const bool isPreferred =
      wifiProfileStore.getPreferred(preferred) &&
      strcmp(preferred.ssid, wifiSelectedProfile.ssid) == 0;
  const bool isActive =
      wifiSsid != nullptr &&
      strcmp(wifiSsid, wifiSelectedProfile.ssid) == 0;
  const bool connected =
      connectivityManager.isConnected() &&
      isActive;

  wifiSettingsRenderer.drawProfileDetail(
      getHeaderTitleText(), timeService.headerClockText(),
      wifiSelectedProfile.ssid, isPreferred, isActive, connected,
      wifiForgetConfirmation, wifiUiMessage);
}

void drawWiFiPasswordScreen() {
  wifiSettingsRenderer.drawPassword(
      getHeaderTitleText(), timeService.headerClockText(),
      wifiSelectedSsid, wifiPasswordInput,
      wifiKeyboardShifted, wifiKeyboardSymbols,
      wifiPasswordVisible,
      wifiKeyFeedbackActive ? wifiHighlightedKeyRow : -1,
      wifiKeyFeedbackActive ? wifiHighlightedKeyColumn : -1,
      wifiUiMessage);
}

void drawWiFiConnectingScreen() {
  wifiSettingsRenderer.drawConnecting(
      getHeaderTitleText(), timeService.headerClockText(),
      wifiPendingProfile.ssid,
      wifiConnectPhase == WiFiConnectPhase::Restore);
}

// Compose the Wi-Fi modal into the existing full-screen staging canvas.
// Rendering all controls into RAM first and flushing once avoids visible tearing
// from the many small SPI drawing operations in each settings screen.
void drawWiFiSettingsScreenContents() {
  switch (wifiSettingsScreen) {
    case WiFiSettingsScreen::Profiles:
      drawWiFiProfilesScreen();
      break;
    case WiFiSettingsScreen::Scan:
      drawWiFiScanScreen();
      break;
    case WiFiSettingsScreen::ProfileDetail:
      drawWiFiProfileDetailScreen();
      break;
    case WiFiSettingsScreen::Password:
      drawWiFiPasswordScreen();
      break;
    case WiFiSettingsScreen::Connecting:
      drawWiFiConnectingScreen();
      break;
    case WiFiSettingsScreen::Closed:
    default:
      break;
  }
}

void drawWiFiSettingsScreen() {
  if (wifiSettingsScreen == WiFiSettingsScreen::Closed) return;

  Arduino_Canvas* frameCanvas = displayManager.tabTransitionCanvas();
  if (frameCanvas == nullptr || frameCanvas->getFramebuffer() == nullptr) {
    // Preserve a functional direct-render fallback on boards where the
    // full-screen PSRAM canvas could not be allocated.
    drawWiFiSettingsScreenContents();
    return;
  }

  displayManager.beginTabTransitionRender();
  drawWiFiSettingsScreenContents();
  displayManager.endTabTransitionRender();

  // Arduino_Canvas is a full-screen 320x240 staging buffer (origin 0,0).
  // Its flush writes one RGB565 bitmap rather than issuing per-widget writes.
  frameCanvas->flush();
}

// Draw either Wi-Fi list at a finger-following or animation position. Saved
// profiles and scan results share the same row geometry and scroll policy.
void drawWiFiListAtScrollPosition(int pixels) {
  wifiListScrollPositionPx = ScrollController::clampElasticPosition(
      pixels,
      maxWiFiListScrollPixels(),
      SCROLL_OVERSCROLL_MAX_PX);
  if (wifiSettingsScreen == WiFiSettingsScreen::Profiles ||
      wifiSettingsScreen == WiFiSettingsScreen::Scan) {
    drawWiFiSettingsScreen();
  }
}

// Match the metric-card settle animation for either Wi-Fi list: six eased
// frames at 8 ms intervals. Without the staging canvas, settle in one frame.
void animateWiFiListSnap(int startPixels, int targetPixels) {
  targetPixels = constrain(targetPixels, 0, maxWiFiListScrollPixels());

  if (startPixels != targetPixels &&
      displayManager.tabTransitionCanvas() != nullptr &&
      displayManager.tabTransitionCanvas()->getFramebuffer() != nullptr) {
    const int animationFrames = 6;
    const int animationDelayMs = 8;
    for (int frame = 1; frame <= animationFrames; ++frame) {
      const float progress =
          static_cast<float>(frame) / animationFrames;
      const float easedProgress =
          progress * progress * (3.0f - 2.0f * progress);
      const int pixels = startPixels +
          static_cast<int>((targetPixels - startPixels) * easedProgress);
      drawWiFiListAtScrollPosition(pixels);
      delay(animationDelayMs);
    }
  } else if (startPixels != targetPixels) {
    drawWiFiListAtScrollPosition(targetPixels);
  }

  wifiListScrollPositionPx = targetPixels;
  if (wifiSettingsScreen == WiFiSettingsScreen::Profiles ||
      wifiSettingsScreen == WiFiSettingsScreen::Scan) {
    drawWiFiSettingsScreen();
  }
}

bool startWiFiSettingsScan() {
  if (wifiNetworkScanner.state() == WiFiNetworkScanner::State::Scanning) {
    return false;
  }

  const bool started = wifiNetworkScanner.start();
  // Record failed starts as attempts too, so a driver error is retried on a
  // bounded cadence rather than on every pass through loop().
  wifiAvailabilityRefreshPolicy.noteAttempt(millis());
  // This attempt is now after the pending link-state edge, even when the
  // driver rejects it; the normal five-second failed-scan retry takes over.
  wifiAvailabilityRefreshPending = false;
  return started;
}

void openWiFiSettings() {
  wifiUiMessage = "";
  wifiForgetConfirmation = false;
  wifiPasswordVisible = false;
  wifiKeyFeedbackActive = false;
  wifiKeyFeedbackHeld = false;
  wifiHighlightedKeyRow = -1;
  wifiHighlightedKeyColumn = -1;

  // Populate saved-profile availability immediately; later scans refresh
  // the badges periodically while the Profiles screen remains visible.
  startWiFiSettingsScan();

  wifiListScrollPositionPx = 0;
  wifiSettingsScreen = WiFiSettingsScreen::Profiles;
  drawWiFiSettingsScreen();
}

void drawApplicationLauncherScreen();

void restoreWeatherScreenAfterWiFiSettings() {
  if (applicationLauncherController.isOpen()) {
    // The launcher remains the foreground screen while the platform Wi-Fi
    // manager is open. Do not reveal or refresh Weather behind it.
    wifiPendingWeatherRefresh = false;
    drawApplicationLauncherScreen();
    return;
  }

  if (wifiPendingWeatherRefresh && weatherApp.isReady()) {
    wifiPendingWeatherRefresh = false;
    weatherApp.requestRefresh();
    drawLoading("Refreshing weather...");
    return;
  }
  wifiPendingWeatherRefresh = false;

  switch (weatherApp.state()) {
    case WeatherApp::State::Ready:
      if (currentTab == 1) drawForecast();
      else drawWeather();
      break;
    case WeatherApp::State::Connecting:
    case WeatherApp::State::RefreshConnecting:
      drawLoading("Connecting Wi-Fi...");
      break;
    case WeatherApp::State::ResolvingLocation:
      drawLoading("Finding your location...");
      break;
    case WeatherApp::State::FetchingCurrentWeather:
    case WeatherApp::State::RefreshingCurrentWeather:
      drawLoading("Fetching current weather...");
      break;
    case WeatherApp::State::FetchingForecast:
    case WeatherApp::State::RefreshingForecast:
      drawLoading("Fetching hourly forecast...");
      break;
    case WeatherApp::State::PreparingStartupAssets:
    case WeatherApp::State::PreparingRefreshAssets:
      drawLoading("Preparing weather display...");
      break;
    case WeatherApp::State::Inactive:
    default:
      drawLoading("Loading weather...");
      break;
  }
}

void closeWiFiSettings() {
  // ESP32 scan cancellation is not exposed by the Arduino API. Let an in-flight
  // scan finish in the background; updateWiFiSettings() will copy/release its
  // bounded results before another scan can be started.
  wifiSettingsScreen = WiFiSettingsScreen::Closed;
  wifiUiTouchActive = false;
  wifiHeaderTouchActive = false;
  wifiListTouchCandidate = false;
  wifiListTouchDragActive = false;
  wifiPasswordVisible = false;
  wifiKeyFeedbackActive = false;
  wifiKeyFeedbackHeld = false;
  wifiHighlightedKeyRow = -1;
  wifiHighlightedKeyColumn = -1;
  restoreWeatherScreenAfterWiFiSettings();
}

void drawApplicationLauncherScreen() {
  applicationLauncherRenderer.draw(
      applicationManager,
      applicationLauncherController.returnApplicationId(),
      applicationLauncherMessage);

  // The launcher is a platform screen, but it uses the same persistent clock
  // and Wi-Fi indicator as Weather. drawClockOnly() also redraws the signal.
  drawHeaderClockOnly(timeService.headerClockText());
}

bool openApplicationLauncher() {
  if (!applicationLauncherController.open()) {
    return false;
  }
  applicationLauncherMessage = "";
  applicationLauncherTouchActive = false;
  applicationLauncherTouchMoved = false;
  drawApplicationLauncherScreen();
  return true;
}

bool beginWiFiProfileConnection(
    const PersistentWiFiProfileStore::Profile& profile,
    bool profileWasNew) {
  if (connectivityManager.isConnected()) {
    const String connectedSsid = WiFi.SSID();
    if (connectedSsid == profile.ssid) {
      const PersistentWiFiProfileStore::Status preferenceStatus =
          wifiProfileStore.setPreferred(profile.ssid);
      wifiUiMessage =
          preferenceStatus == PersistentWiFiProfileStore::Status::Success
              ? "Already connected; preferred network saved"
              : "Connected; could not save preference";
      wifiSettingsScreen = WiFiSettingsScreen::Profiles;
      wifiPendingWeatherRefresh = true;
      drawWiFiSettingsScreen();
      return true;
    }
  }

  wifiPendingProfile = profile;
  wifiPendingProfileWasNew = profileWasNew;
  wifiPreviousProfileValid =
      wifiProfileStore.getPreferred(wifiPreviousProfile);
  if (wifiPreviousProfileValid &&
      strcmp(wifiPreviousProfile.ssid, profile.ssid) == 0) {
    // The attempted network is already preferred; retry it only once.
    wifiPreviousProfileValid = false;
  }

  activeWiFiProfile = profile;
  wifiSsid = activeWiFiProfile.ssid;
  wifiPassword = activeWiFiProfile.password;

  connectivityManager.disconnect();
  if (!wifiBackend.configure(wifiSsid, wifiPassword)) {
    wifiUiMessage = "Could not configure this network";
    wifiSettingsScreen = WiFiSettingsScreen::Profiles;
    drawWiFiSettingsScreen();
    return false;
  }

  wifiConnectPhase = WiFiConnectPhase::Candidate;
  connectivityManager.connect(millis());
  wifiSettingsScreen = WiFiSettingsScreen::Connecting;
  drawWiFiSettingsScreen();
  return true;
}

void selectSavedWiFiProfile(uint8_t index) {
  if (!wifiProfileStore.getAt(index, wifiSelectedProfile)) {
    wifiUiMessage = "Saved profile is unavailable";
    drawWiFiSettingsScreen();
    return;
  }
  wifiSelectedProfileValid = true;
  wifiForgetConfirmation = false;
  wifiUiMessage = "";
  wifiSettingsScreen = WiFiSettingsScreen::ProfileDetail;
  drawWiFiSettingsScreen();
}

void startWiFiNetworkScan() {
  wifiListScrollPositionPx = 0;
  wifiListTouchCandidate = false;
  wifiListTouchDragActive = false;
  wifiUiMessage = "";
  wifiSettingsScreen = WiFiSettingsScreen::Scan;
  startWiFiSettingsScan();
  drawWiFiSettingsScreen();
}

void selectScannedWiFiNetwork(uint8_t index) {
  WiFiScanResult network = {};
  if (!wifiNetworkScanner.getAt(index, network)) return;

  PersistentWiFiProfileStore::Profile saved = {};
  if (wifiProfileStore.getBySsid(network.ssid, saved)) {
    wifiUiMessage = "";
    beginWiFiProfileConnection(saved, false);
    return;
  }

  if (!wifiProfileStore.isLoaded()) {
    wifiUiMessage = "Saved profile storage unavailable";
    drawWiFiSettingsScreen();
    return;
  }

  if (!network.supported) {
    wifiUiMessage = "This security type is not supported";
    drawWiFiSettingsScreen();
    return;
  }

  strncpy(wifiSelectedSsid, network.ssid, sizeof(wifiSelectedSsid) - 1);
  wifiSelectedSsid[sizeof(wifiSelectedSsid) - 1] = '\0';
  memset(wifiPasswordInput, 0, sizeof(wifiPasswordInput));
  wifiKeyboardShifted = false;
  wifiKeyboardSymbols = false;
  wifiPasswordVisible = false;
  wifiUiMessage = "";

  if (network.open) {
    const PersistentWiFiProfileStore::Status saveStatus =
        wifiProfileStore.save(network.ssid, nullptr, false);
    if (saveStatus != PersistentWiFiProfileStore::Status::Success) {
      wifiUiMessage =
          saveStatus == PersistentWiFiProfileStore::Status::Full
              ? "Saved profile limit reached"
              : "Could not save this network";
      drawWiFiSettingsScreen();
      return;
    }

    PersistentWiFiProfileStore::Profile profile = {};
    if (wifiProfileStore.getBySsid(network.ssid, profile)) {
      beginWiFiProfileConnection(profile, true);
    }
    return;
  }

  wifiSettingsScreen = WiFiSettingsScreen::Password;
  drawWiFiSettingsScreen();
}

void submitWiFiPassword() {
  const size_t passwordLength = strlen(wifiPasswordInput);
  if (passwordLength < 8 || passwordLength > 63) {
    wifiUiMessage = "Password must be 8-63 characters";
    drawWiFiSettingsScreen();
    return;
  }

  if (!wifiProfileStore.isLoaded()) {
    wifiUiMessage = "Saved profile storage unavailable";
    drawWiFiSettingsScreen();
    return;
  }

  const PersistentWiFiProfileStore::Status saveStatus =
      wifiProfileStore.save(wifiSelectedSsid, wifiPasswordInput, false);
  if (saveStatus != PersistentWiFiProfileStore::Status::Success) {
    wifiUiMessage =
        saveStatus == PersistentWiFiProfileStore::Status::Full
            ? "Saved profile limit reached"
            : "Could not save this network";
    drawWiFiSettingsScreen();
    return;
  }

  PersistentWiFiProfileStore::Profile profile = {};
  if (!wifiProfileStore.getBySsid(wifiSelectedSsid, profile)) {
    wifiUiMessage = "Could not read the saved profile";
    drawWiFiSettingsScreen();
    return;
  }

  beginWiFiProfileConnection(profile, true);
}

void forgetSelectedWiFiProfile() {
  if (!wifiSelectedProfileValid) return;

  PersistentWiFiProfileStore::Profile preferred = {};
  const bool wasPreferred =
      wifiProfileStore.getPreferred(preferred) &&
      strcmp(preferred.ssid, wifiSelectedProfile.ssid) == 0;

  const PersistentWiFiProfileStore::Status removeStatus =
      wifiProfileStore.remove(wifiSelectedProfile.ssid);
  if (removeStatus != PersistentWiFiProfileStore::Status::Success) {
    wifiUiMessage = "Could not forget this network";
    wifiForgetConfirmation = false;
    drawWiFiSettingsScreen();
    return;
  }

  wifiSelectedProfileValid = false;
  wifiForgetConfirmation = false;
  wifiUiMessage = "Network forgotten";

  if (wasPreferred) {
    PersistentWiFiProfileStore::Profile nextPreferred = {};
    if (wifiProfileStore.getPreferred(nextPreferred)) {
      beginWiFiProfileConnection(nextPreferred, false);
      return;
    }

    connectivityManager.disconnect();
    wifiSsid = nullptr;
    wifiPassword = nullptr;
    memset(&activeWiFiProfile, 0, sizeof(activeWiFiProfile));
    wifiConnectPhase = WiFiConnectPhase::None;
    wifiPendingWeatherRefresh = true;
  }

  wifiSettingsScreen = WiFiSettingsScreen::Profiles;
  drawWiFiSettingsScreen();
}

void restorePreviousWiFiProfile() {
  if (!wifiPreviousProfileValid) return;

  activeWiFiProfile = wifiPreviousProfile;
  wifiSsid = activeWiFiProfile.ssid;
  wifiPassword = activeWiFiProfile.password;
  connectivityManager.disconnect();

  wifiConnectPhase = WiFiConnectPhase::Restore;
  wifiBackend.configure(wifiSsid, wifiPassword);
  connectivityManager.connect(millis());
  wifiSettingsScreen = WiFiSettingsScreen::Connecting;
  drawWiFiSettingsScreen();
}

void finishWiFiConnection(bool restored) {
  if (!restored) {
    const PersistentWiFiProfileStore::Status preferredStatus =
        wifiProfileStore.setPreferred(wifiPendingProfile.ssid);
    wifiUiMessage =
        preferredStatus == PersistentWiFiProfileStore::Status::Success
            ? String("Connected: ") + wifiPendingProfile.ssid
            : "Connected, but preference could not be saved";
  } else {
    wifiUiMessage =
        String("Connection failed; restored ") + wifiPreviousProfile.ssid;
  }

  wifiConnectPhase = WiFiConnectPhase::None;
  wifiSettingsScreen = WiFiSettingsScreen::Profiles;
  wifiPendingWeatherRefresh = true;
  drawWiFiSettingsScreen();
}

void failWiFiConnection(bool restoring) {
  if (!restoring && wifiPreviousProfileValid) {
    restorePreviousWiFiProfile();
    return;
  }

  if (!restoring && wifiPendingProfileWasNew &&
      !wifiPreviousProfileValid) {
    // The store automatically makes its first profile preferred. If that
    // first connection fails, remove the unusable candidate rather than
    // leaving it as the boot-time preferred network.
    wifiProfileStore.remove(wifiPendingProfile.ssid);
    wifiSsid = nullptr;
    wifiPassword = nullptr;
    memset(&activeWiFiProfile, 0, sizeof(activeWiFiProfile));
    wifiUiMessage = "Connection failed; check the password";
  } else if (restoring) {
    wifiUiMessage = "Connection failed; previous network is offline";
    wifiPendingWeatherRefresh = true;
  } else {
    // Preserve an existing saved profile so the user can retry it later.
    activeWiFiProfile = wifiPendingProfile;
    wifiSsid = activeWiFiProfile.ssid;
    wifiPassword = activeWiFiProfile.password;
    wifiUiMessage = "Connection failed; check network availability";
  }

  wifiConnectPhase = WiFiConnectPhase::None;
  wifiSettingsScreen = WiFiSettingsScreen::Profiles;
  drawWiFiSettingsScreen();
}

void updateWiFiSettings() {
  const bool connected = connectivityManager.isConnected();
  const bool connectionAvailabilityChanged =
      wifiAvailabilityConnectionObserved &&
      connected != wifiAvailabilityLastConnected;
  wifiAvailabilityLastConnected = connected;
  wifiAvailabilityConnectionObserved = true;
  if (connectionAvailabilityChanged) {
    wifiAvailabilityRefreshPending = true;
  }

  const WiFiNetworkScanner::State previousScanState =
      wifiNetworkScanner.state();
  const uint8_t previousScanCount = wifiNetworkScanner.count();
  wifiNetworkScanner.update();

  const bool scanCompleted =
      previousScanState == WiFiNetworkScanner::State::Scanning &&
      wifiNetworkScanner.state() == WiFiNetworkScanner::State::Ready;
  if (scanCompleted) {
    // Commit only completed results. The snapshot remains intact through
    // active and failed scans, so periodic checks never flash CHECKING/UNKNOWN.
    wifiAvailabilitySnapshot.capture(wifiNetworkScanner);
  }

  // Refresh every 30 seconds while Saved Profiles is visible. A real
  // connected/disconnected edge bypasses that cadence and starts a scan now,
  // so an access point disappearing or returning is reflected promptly.
  // Failed scans retry after 5 seconds without discarding the last good set.
  if (wifiAvailabilityRefreshPolicy.shouldStart(
          millis(),
          wifiSettingsScreen == WiFiSettingsScreen::Profiles,
          wifiNetworkScanner.state() == WiFiNetworkScanner::State::Scanning,
          wifiNetworkScanner.state() == WiFiNetworkScanner::State::Failed,
          wifiAvailabilityRefreshPending)) {
    startWiFiSettingsScan();
  }

  bool redrawProfiles = false;
  bool redrawScanPage = false;
  if (wifiSettingsScreen == WiFiSettingsScreen::Profiles) {
    // No redraw when a background scan merely starts or remains in progress.
    // Redraw on connectivity edges (connection-status text) and when a
    // completed scan replaces the availability snapshot.
    redrawProfiles = connectionAvailabilityChanged || scanCompleted;
  } else if (wifiSettingsScreen == WiFiSettingsScreen::Scan) {
    // The explicit Available Networks page retains its visible scan status.
    redrawScanPage =
        wifiNetworkScanner.state() != previousScanState ||
        wifiNetworkScanner.count() != previousScanCount;
  }

  if (redrawProfiles || redrawScanPage) {
    if (wifiNetworkScanner.state() != WiFiNetworkScanner::State::Scanning &&
        wifiUiMessage == "Please wait for the availability scan") {
      wifiUiMessage = "";
    }
    drawWiFiSettingsScreen();
  }

  if (wifiKeyFeedbackActive && !wifiKeyFeedbackHeld &&
      static_cast<int32_t>(millis() - wifiKeyFeedbackUntilMs) >= 0) {
    wifiKeyFeedbackActive = false;
    wifiHighlightedKeyRow = -1;
    wifiHighlightedKeyColumn = -1;
    if (wifiSettingsScreen == WiFiSettingsScreen::Password) {
      drawWiFiSettingsScreen();
    }
  }

  if (wifiConnectPhase == WiFiConnectPhase::None) {
    return;
  }

  if (connectivityManager.isConnected()) {
    finishWiFiConnection(wifiConnectPhase == WiFiConnectPhase::Restore);
    return;
  }

  const ConnectivityManager::State state = connectivityManager.state();
  if (state == ConnectivityManager::State::Failed ||
      state == ConnectivityManager::State::TimedOut) {
    failWiFiConnection(wifiConnectPhase == WiFiConnectPhase::Restore);
  }
}

void handleWiFiPasswordKey(char key) {
  const size_t length = strlen(wifiPasswordInput);
  if (length >= PersistentWiFiProfileStore::MaxPasswordLength) return;
  wifiPasswordInput[length] = key;
  wifiPasswordInput[length + 1] = '\0';
  wifiUiMessage = "";
  drawWiFiSettingsScreen();
}

bool findWiFiPasswordKeyAt(int x, int y, int& row, int& column) {
  if (y >= WiFiSettingsRenderer::KeyboardFunctionTop &&
      y < WiFiSettingsRenderer::KeyboardFunctionTop + 22 &&
      x >= 8 && x < 312) {
    row = 4;
    if (x < 72) column = 0;
    else if (x < 138) column = 1;
    else if (x < 202) column = 2;
    else column = 3;
    return true;
  }

  if (y < WiFiSettingsRenderer::KeyboardY ||
      y >= WiFiSettingsRenderer::KeyboardY +
               4 * (WiFiSettingsRenderer::KeyboardKeyHeight +
                    WiFiSettingsRenderer::KeyboardGapY)) {
    return false;
  }

  const int relativeY = y - WiFiSettingsRenderer::KeyboardY;
  const int rowStep = WiFiSettingsRenderer::KeyboardKeyHeight +
                      WiFiSettingsRenderer::KeyboardGapY;
  const int withinRowY = relativeY % rowStep;
  if (withinRowY >= WiFiSettingsRenderer::KeyboardKeyHeight) return false;

  const int relativeX = x - WiFiSettingsRenderer::KeyboardX;
  if (relativeX < 0) return false;
  const int columnStep = WiFiSettingsRenderer::KeyboardKeyWidth +
                         WiFiSettingsRenderer::KeyboardGapX;
  if (relativeX % columnStep >= WiFiSettingsRenderer::KeyboardKeyWidth) {
    return false;
  }

  row = relativeY / rowStep;
  column = relativeX / columnStep;
  if (column >= 10) return false;
  if (row == 2 && column == 9) return true;

  static const char* alphaRows[] = {
    "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm.,-"
  };
  static const char* symbolRows[] = {
    "!@#$%^&*()", "-_=+[]{};:", "'\"<>?/\\|", "~`^%&+=*"
  };
  return (wifiKeyboardSymbols
      ? symbolRows[row] : alphaRows[row])[column] != '\0';
}

void handleWiFiSettingsTap(int x, int y) {
  if (wifiSettingsScreen == WiFiSettingsScreen::Profiles) {
    if (y >= WiFiSettingsRenderer::ListRowsTop &&
        y < WiFiSettingsRenderer::ListRowsTop +
                WiFiSettingsRenderer::ListRowsVisible *
                    WiFiSettingsRenderer::ListRowHeight) {
      const int contentY =
          y - WiFiSettingsRenderer::ListRowsTop +
          wifiListScrollPositionPx;
      if (contentY < 0) return;
      const uint8_t index = static_cast<uint8_t>(
          contentY / WiFiSettingsRenderer::ListRowHeight);
      if (index < wifiProfileStore.count()) {
        if (wifiNetworkScanner.state() ==
            WiFiNetworkScanner::State::Scanning) {
          wifiUiMessage = "Please wait for the availability scan";
          drawWiFiSettingsScreen();
        } else {
          selectSavedWiFiProfile(index);
        }
      }
      return;
    }

    if (y >= WiFiSettingsRenderer::FooterTop &&
        y < WiFiSettingsRenderer::FooterTop +
                WiFiSettingsRenderer::FooterHeight) {
      if (x < 160) startWiFiNetworkScan();
      else closeWiFiSettings();
    }
    return;
  }

  if (wifiSettingsScreen == WiFiSettingsScreen::Scan) {
    if (y >= WiFiSettingsRenderer::ListRowsTop &&
        y < WiFiSettingsRenderer::ListRowsTop +
                WiFiSettingsRenderer::ListRowsVisible *
                    WiFiSettingsRenderer::ListRowHeight) {
      const int contentY =
          y - WiFiSettingsRenderer::ListRowsTop +
          wifiListScrollPositionPx;
      if (contentY >= 0) {
        const int index =
            contentY / WiFiSettingsRenderer::ListRowHeight;
        if (index < wifiNetworkScanner.count()) {
          selectScannedWiFiNetwork(static_cast<uint8_t>(index));
        }
      }
      return;
    }

    if (y >= WiFiSettingsRenderer::FooterTop &&
        y < WiFiSettingsRenderer::FooterTop +
                WiFiSettingsRenderer::FooterHeight) {
      if (x >= 8 && x < 153) {
        wifiListScrollPositionPx = 0;
        wifiListTouchCandidate = false;
        wifiListTouchDragActive = false;
        wifiSettingsScreen = WiFiSettingsScreen::Profiles;
        wifiUiMessage = "";
        drawWiFiSettingsScreen();
      } else if (x >= 167 && x < 312) {
        startWiFiNetworkScan();
      }
    }
    return;
  }

  if (wifiSettingsScreen == WiFiSettingsScreen::ProfileDetail) {
    if (y >= WiFiSettingsRenderer::DetailActionTop &&
        y < WiFiSettingsRenderer::DetailActionTop +
                WiFiSettingsRenderer::DetailActionHeight) {
      if (x < 160) {
        if (wifiForgetConfirmation) {
          wifiForgetConfirmation = false;
          wifiUiMessage = "";
        } else {
          wifiUiMessage = "";
          beginWiFiProfileConnection(wifiSelectedProfile, false);
        }
      } else if (wifiForgetConfirmation) {
        forgetSelectedWiFiProfile();
      } else {
        wifiForgetConfirmation = true;
        wifiUiMessage = "Confirm forgetting this network";
      }
      drawWiFiSettingsScreen();
      return;
    }

    if (y >= WiFiSettingsRenderer::DetailBackTop) {
      wifiSettingsScreen = WiFiSettingsScreen::Profiles;
      wifiForgetConfirmation = false;
      wifiUiMessage = "";
      drawWiFiSettingsScreen();
    }
    return;
  }

  if (wifiSettingsScreen == WiFiSettingsScreen::Password) {
    if (y >= WiFiSettingsRenderer::PasswordFieldTop &&
        y < WiFiSettingsRenderer::PasswordFieldTop + 21 &&
        x >= 220 && x < 312) {
      wifiPasswordVisible = !wifiPasswordVisible;
      drawWiFiSettingsScreen();
      return;
    }

    if (y >= WiFiSettingsRenderer::KeyboardY &&
        y < WiFiSettingsRenderer::KeyboardY +
                4 * (WiFiSettingsRenderer::KeyboardKeyHeight +
                     WiFiSettingsRenderer::KeyboardGapY)) {
      const uint8_t row = static_cast<uint8_t>(
          (y - WiFiSettingsRenderer::KeyboardY) /
          (WiFiSettingsRenderer::KeyboardKeyHeight +
           WiFiSettingsRenderer::KeyboardGapY));
      const int withinRowY =
          (y - WiFiSettingsRenderer::KeyboardY) %
          (WiFiSettingsRenderer::KeyboardKeyHeight +
           WiFiSettingsRenderer::KeyboardGapY);
      const int relativeX = x - WiFiSettingsRenderer::KeyboardX;
      if (withinRowY < WiFiSettingsRenderer::KeyboardKeyHeight &&
          relativeX >= 0) {
        const uint8_t column = static_cast<uint8_t>(
            relativeX /
            (WiFiSettingsRenderer::KeyboardKeyWidth +
             WiFiSettingsRenderer::KeyboardGapX));
        const int withinKeyX =
            relativeX %
            (WiFiSettingsRenderer::KeyboardKeyWidth +
             WiFiSettingsRenderer::KeyboardGapX);
        if (column < 10 &&
            withinKeyX < WiFiSettingsRenderer::KeyboardKeyWidth) {
          if (row == 2 && column == 9) {
            const size_t length = strlen(wifiPasswordInput);
            if (length > 0) wifiPasswordInput[length - 1] = '\0';
            wifiUiMessage = "";
            drawWiFiSettingsScreen();
            return;
          }

          static const char* alphaRows[] = {
            "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm.,-"
          };
          static const char* symbolRows[] = {
            "!@#$%^&*()", "-_=+[]{};:", "'\"<>?/\\|", "~`^%&+=*"
          };
          const char key = (wifiKeyboardSymbols
              ? symbolRows[row] : alphaRows[row])[column];
          if (key != '\0') {
            char selected = key;
            if (!wifiKeyboardSymbols && wifiKeyboardShifted &&
                row > 0 && key >= 'a' && key <= 'z') {
              selected = static_cast<char>(key - ('a' - 'A'));
            }
            handleWiFiPasswordKey(selected);
          }
        }
      }
      return;
    }

    if (y >= WiFiSettingsRenderer::KeyboardFunctionTop &&
        y < WiFiSettingsRenderer::KeyboardFunctionTop + 22) {
      if (x < 72) {
        wifiKeyboardShifted = !wifiKeyboardShifted;
      } else if (x < 138) {
        handleWiFiPasswordKey(' ');
        return;
      } else if (x < 202) {
        const size_t length = strlen(wifiPasswordInput);
        if (length > 0) wifiPasswordInput[length - 1] = '\0';
      } else {
        wifiKeyboardSymbols = !wifiKeyboardSymbols;
      }
      wifiUiMessage = "";
      drawWiFiSettingsScreen();
      return;
    }

    if (y >= WiFiSettingsRenderer::PasswordFooterTop &&
        y < WiFiSettingsRenderer::PasswordFooterTop + 23) {
      if (x < 160) {
        wifiPasswordVisible = false;
        wifiSettingsScreen = WiFiSettingsScreen::Scan;
        wifiUiMessage = "";
        drawWiFiSettingsScreen();
      } else {
        submitWiFiPassword();
      }
    }
  }
}

bool processWiFiSettingsTouch(
    const TouchSample& sample,
    const GestureObservation& observation) {
  int x = 0;
  int y = 0;
  if (observation.touchPresent) {
    touchCoordinatesToScreen(sample, x, y);
  }

  if (wifiSettingsScreen != WiFiSettingsScreen::Closed) {
    if (observation.touchStarted) {
      wifiUiTouchActive = true;
      wifiUiTouchMoved = false;
      wifiUiTouchStartX = x;
      wifiUiTouchStartY = y;
      const bool onScrollableWiFiList =
          wifiSettingsScreen == WiFiSettingsScreen::Profiles ||
          wifiSettingsScreen == WiFiSettingsScreen::Scan;
      wifiListTouchCandidate =
          onScrollableWiFiList &&
          wifiSettingsListItemCount() >
              WiFiSettingsRenderer::ListRowsVisible &&
          x >= 8 && x < displayManager.width() - 8 &&
          y >= WiFiSettingsRenderer::ListRowsTop &&
          y < WiFiSettingsRenderer::ListRowsTop +
                  WiFiSettingsRenderer::ListRowsVisible *
                      WiFiSettingsRenderer::ListRowHeight;
      wifiListTouchDragActive = false;
      wifiListDragStartScroll = wifiListScrollPositionPx;
      wifiListDragCurrentScroll = wifiListScrollPositionPx;

      const bool hadKeyFeedback = wifiKeyFeedbackActive;
      wifiKeyFeedbackActive = false;
      wifiKeyFeedbackHeld = false;
      wifiHighlightedKeyRow = -1;
      wifiHighlightedKeyColumn = -1;
      bool hasNewKeyFeedback = false;
      if (wifiSettingsScreen == WiFiSettingsScreen::Password) {
        int row = 0;
        int column = 0;
        if (findWiFiPasswordKeyAt(x, y, row, column)) {
          wifiKeyFeedbackActive = true;
          wifiKeyFeedbackHeld = true;
          wifiHighlightedKeyRow = static_cast<int8_t>(row);
          wifiHighlightedKeyColumn = static_cast<int8_t>(column);
          hasNewKeyFeedback = true;
        }
      }
      if (hasNewKeyFeedback ||
          (hadKeyFeedback &&
           wifiSettingsScreen == WiFiSettingsScreen::Password)) {
        drawWiFiSettingsScreen();
      }
    }

    if (wifiUiTouchActive && observation.touchPresent &&
        wifiListTouchCandidate) {
      const int deltaX = x - wifiUiTouchStartX;
      const int deltaY = y - wifiUiTouchStartY;
      const int absX = abs(deltaX);
      const int absY = abs(deltaY);
      const int intentThresholdPx = 8;

      if (absY >= intentThresholdPx && absY >= absX) {
        wifiListTouchCandidate = false;
        wifiListTouchDragActive = true;
        wifiUiTouchMoved = true;
      } else if (absX >= intentThresholdPx && absX > absY) {
        // Horizontal movement is not a list selection; keep it from becoming
        // an accidental tap while leaving the modal's vertical list policy clear.
        wifiListTouchCandidate = false;
        wifiUiTouchMoved = true;
      }
    }

    if (wifiUiTouchActive && observation.touchPresent &&
        wifiListTouchDragActive) {
      const int nextScroll = ScrollController::elasticPosition(
          ScrollController::applyDragDelta(
              wifiListDragStartScroll, wifiUiTouchStartY, y),
          maxWiFiListScrollPixels(),
          SCROLL_OVERSCROLL_MAX_PX,
          SCROLL_OVERSCROLL_RESISTANCE_DIVISOR);
      if (nextScroll != wifiListDragCurrentScroll) {
        wifiListDragCurrentScroll = nextScroll;
        wifiListScrollPositionPx = nextScroll;
        // Redraw fractional row movement as well as whole-row transitions so
        // the contents track the finger instead of stepping between rows.
        drawWiFiSettingsScreen();
      }
    }

    if (wifiUiTouchActive && observation.touchPresent &&
        (abs(x - wifiUiTouchStartX) > 14 ||
         abs(y - wifiUiTouchStartY) > 14)) {
      wifiUiTouchMoved = true;
    }

    if (wifiUiTouchActive && observation.touchReleased) {
      if (wifiKeyFeedbackHeld) {
        wifiKeyFeedbackHeld = false;
        wifiKeyFeedbackUntilMs = millis() + 180UL;
      }

      if (wifiListTouchDragActive) {
        const int targetScroll = ScrollController::snapToStep(
            wifiListDragCurrentScroll,
            WiFiSettingsRenderer::ListRowHeight,
            maxWiFiListScrollPixels());
        animateWiFiListSnap(
            wifiListDragCurrentScroll, targetScroll);
      }

      wifiListTouchCandidate = false;
      wifiListTouchDragActive = false;

      if (!wifiUiTouchMoved) {
        const bool headerWiFiTapped =
            wifiUiTouchStartX >= HeaderRenderer::WiFiHitAreaLeft &&
            wifiUiTouchStartX < HeaderRenderer::WiFiHitAreaRight &&
            wifiUiTouchStartY >= HeaderRenderer::WiFiHitAreaTop &&
            wifiUiTouchStartY < HeaderRenderer::WiFiHitAreaBottom;
        if (headerWiFiTapped) {
          closeWiFiSettings();
        } else {
          handleWiFiSettingsTap(wifiUiTouchStartX, wifiUiTouchStartY);
        }
      }
      wifiUiTouchActive = false;
      wifiUiTouchMoved = false;
    }
    return true;
  }

  if (observation.touchStarted &&
      x >= HeaderRenderer::WiFiHitAreaLeft &&
      x < HeaderRenderer::WiFiHitAreaRight &&
      y >= HeaderRenderer::WiFiHitAreaTop &&
      y < HeaderRenderer::WiFiHitAreaBottom) {
    wifiHeaderTouchActive = true;
    wifiHeaderTouchValid = true;
    wifiHeaderTouchStartX = x;
    wifiHeaderTouchStartY = y;
  }

  if (wifiHeaderTouchActive && observation.touchPresent &&
      (abs(x - wifiHeaderTouchStartX) > 14 ||
       abs(y - wifiHeaderTouchStartY) > 14)) {
    wifiHeaderTouchValid = false;
  }

  if (wifiHeaderTouchActive && observation.touchReleased) {
    if (wifiHeaderTouchValid) openWiFiSettings();
    wifiHeaderTouchActive = false;
    wifiHeaderTouchValid = false;
    return true;
  }

  return wifiHeaderTouchActive;
}

// Advance scan and connection work from loop(), never from a drawing or touch
// callback. Weather's staged fetch/render workflow is paused while this modal
// settings surface is visible; the device loop and clock continue to run.
bool weatherAppPauseWorkflow() {
  return wifiSettingsScreen != WiFiSettingsScreen::Closed;
}

// =====================================================
// NAVIGATION
// =====================================================

void handleGesture(GestureCode gesture) {
  unsigned long now = millis();
  if (now - lastGestureTime < 250UL) return;
  lastGestureTime = now;

  DEBUG_PRINTF(
    "Navigation gesture accepted: raw=0x%02X; tab=%d; offset=%d\n",
    static_cast<uint8_t>(gesture), currentTab, forecastOffset);

  // These are provisional mappings. Use the raw transition logs to verify
  // physical directions on this particular panel before changing the codes.
  if (gesture == GestureCode::ScreenRight) {
    if (currentTab == 0) {
      forecastOffset = 0;
      animateTabTransition(1, 1);
      if (forecastAvailable && forecastCount > 0) {
        DEBUG_PRINTLN("Action: mapped horizontal-left gesture opened forecast");
      } else {
        // Show the forecast screen even when retrieval failed, so the API
        // error message is visible instead of only a generic unavailable log.
        DEBUG_PRINTF("Action: forecast unavailable; reason: %s\n", forecastError.length() ? forecastError.c_str() : "No hourly records");
      }
    } else {
      DEBUG_PRINTLN("Action: mapped horizontal-left gesture; already on forecast");
    }
    return;
  }

  if (gesture == GestureCode::ScreenLeft) {
    if (currentTab == 1) {
      animateTabTransition(0, -1);
      DEBUG_PRINTLN("Action: mapped horizontal-right gesture returned to weather");
    } else {
      DEBUG_PRINTLN("Action: mapped horizontal-right gesture; already on weather");
    }
    return;
  }

  if (currentTab == 0) {
    if (gesture == GestureCode::ScreenUp) {
      int maxOffset = WeatherScrollState::maximumOffset(WEATHER_METRIC_COUNT, WEATHER_METRICS_VISIBLE);
      if (weatherMetricOffset < maxOffset) {
        animateWeatherMetricsScroll(1);
        DEBUG_PRINTF(
          "Action: scrolled current-weather metrics forward; offset=%d\n",
          weatherMetricOffset);
      } else {
        DEBUG_PRINTLN("Action: already at last current-weather metric");
      }
    } else if (gesture == GestureCode::ScreenDown) {
      if (weatherMetricOffset > 0) {
        animateWeatherMetricsScroll(-1);
        DEBUG_PRINTF(
          "Action: scrolled current-weather metrics backward; offset=%d\n",
          weatherMetricOffset);
      } else {
        DEBUG_PRINTLN("Action: already at first current-weather metric");
      }
    } else {
      DEBUG_PRINTLN("Action: vertical gesture ignored on current-weather screen");
    }
    return;
  }

  if (currentTab == 1 && forecastGraphView) {
    // Graph movement is handled by coordinate-tracked hold-and-drag. Vertical
    // controller gestures are ignored here; horizontal gestures above still switch tabs.
    DEBUG_PRINTLN("Ignoring legacy vertical gesture in graph mode");
    return;
  }

  if (gesture == GestureCode::ScreenUp) {
    int maxOffset = WeatherScrollState::maximumOffset(forecastCount, ITEMS_PER_PAGE);
    if (forecastOffset < maxOffset) {
      animateForecastScroll(1);
      DEBUG_PRINTF("Action: mapped vertical-up gesture advanced one entry; offset=%d\n", forecastOffset);
    } else {
      DEBUG_PRINTLN("Action: already at end of forecast");
    }
    return;
  }

  if (gesture == GestureCode::ScreenDown) {
    if (forecastOffset > 0) {
      animateForecastScroll(-1);
      DEBUG_PRINTF("Action: mapped vertical-down gesture moved back one entry; offset=%d\n", forecastOffset);
    } else {
      DEBUG_PRINTLN("Action: already at beginning of forecast");
    }
  }
}


bool processApplicationLauncherTouch(
    const TouchSample& sample,
    const GestureObservation& observation) {
  if (!applicationLauncherController.isOpen()) {
    return false;
  }

  int x = 0;
  int y = 0;
  if (observation.touchPresent) {
    touchCoordinatesToScreen(sample, x, y);
  }

  if (observation.touchStarted) {
    applicationLauncherTouchActive = true;
    applicationLauncherTouchMoved = false;
    applicationLauncherTouchStartX = x;
    applicationLauncherTouchStartY = y;
  }

  if (applicationLauncherTouchActive &&
      observation.touchPresent &&
      (abs(x - applicationLauncherTouchStartX) > 14 ||
       abs(y - applicationLauncherTouchStartY) > 14)) {
    applicationLauncherTouchMoved = true;
  }

  if (applicationLauncherTouchActive && observation.touchReleased) {
    bool redrawLauncher = false;
    if (!applicationLauncherTouchMoved) {
      uint8_t applicationIndex = 0;
      if (applicationLauncherRenderer.applicationAt(
              applicationLauncherTouchStartX,
              applicationLauncherTouchStartY,
              applicationIndex,
              applicationManager)) {
        if (!applicationLauncherController.select(applicationIndex)) {
          applicationLauncherMessage = "APP FAILED TO START";
          redrawLauncher = true;
        } else {
          applicationLauncherMessage = "";
        }
      }
    }

    applicationLauncherTouchActive = false;
    applicationLauncherTouchMoved = false;
    // Ignore touches outside app tiles. There is no BACK or prompt region.
    // Re-enter the previous app by selecting its icon again.
    if (applicationLauncherController.isOpen() && redrawLauncher) {
      drawApplicationLauncherScreen();
    }
  }

  return true;
}

void pollTouch() {
  if (!touchReader.available()) return;

  TouchSample sample;
  if (!touchReader.readSample(sample)) return;
  const GestureObservation observation = gestureRecognizer.update(sample);
  const GestureCode gesture = observation.gesture;

  // The bottom-edge swipe is a platform gesture, so it takes precedence over
  // app gestures and modal touches. Use the release sample's final coordinates
  // even though the recognizer marks that sample as no longer touching.
  int launcherGestureX = 0;
  int launcherGestureY = 0;
  if (observation.touchStarted || observation.touchPresent ||
      observation.touchReleased) {
    touchCoordinatesToScreen(sample, launcherGestureX, launcherGestureY);
  }

  bool launcherGestureRecognised = false;
  const bool wifiSettingsOverlayOpen =
      wifiSettingsScreen != WiFiSettingsScreen::Closed;
  if (!applicationLauncherController.isOpen() || wifiSettingsOverlayOpen) {
    launcherGestureRecognised = applicationLauncherGesture.update(
        observation.touchStarted,
        observation.touchPresent,
        observation.touchReleased,
        launcherGestureX,
        launcherGestureY,
        millis(),
        displayManager.width(),
        displayManager.height());
  } else {
    applicationLauncherGesture.reset();
  }

  const bool consumeBottomEdgeTouch =
      applicationLauncherGesture.consumeTouchSample();
  if (launcherGestureRecognised) {
    if (wifiSettingsOverlayOpen) {
      closeWiFiSettings();
    }
    if (!applicationLauncherController.isOpen()) {
      openApplicationLauncher();
    }
    return;
  }
  if (consumeBottomEdgeTouch) {
    return;
  }

  // Wi-Fi settings owns its header hit target even when the modal is
  // closed. Dispatch it before the launcher captures all remaining contacts;
  // otherwise a launcher-screen tap on the signal icon is swallowed as a
  // harmless background touch.
  if (processWiFiSettingsTouch(sample, observation)) {
    return;
  }

  // The launcher is platform-level input: it remains usable while the active
  // application is stopped and the manager has no app receiving updates.
  if (processApplicationLauncherTouch(sample, observation)) {
    return;
  }

  // Weather-specific gestures run only while Weather is the active app.
  const char* activeApplicationId = applicationManager.activeId();
  if (activeApplicationId == nullptr ||
      strcmp(activeApplicationId, "weather") != 0) {
    return;
  }

  // Always log raw transitions in demo mode, including the return to zero.
  if (observation.rawGestureChanged) {
    DEBUG_PRINTF(
      "Touch register transition: 0x%02X -> 0x%02X\n",
      observation.previousRawGesture,
      observation.rawGesture);
  }

  updateTouchDrag(sample, observation);

  // A drag on the metric cards is driven by coordinates. Suppress the
  // controller's completed swipe code so the same motion is not applied twice.
  const unsigned long nowMs = millis();
  const bool metricReleaseGestureSuppressed =
    suppressMetricGesture &&
    (nowMs - metricsGestureSuppressionStartedMs < 300UL);
  const bool forecastReleaseGestureSuppressed =
    suppressForecastGesture &&
    (nowMs - forecastGestureSuppressionStartedMs < 300UL);
  const bool releaseGestureSuppressed =
    metricReleaseGestureSuppressed || forecastReleaseGestureSuppressed;

  // A graph-originated contact owns its controller gesture even if finger
  // tracking briefly disappears. Clear this latch only after a sustained zero.
  bool graphControllerGestureSuppressed = false;
  if (suppressGraphControllerGestureAfterTouch) {
    if (gesture == GestureCode::None) {
      if (!graphGestureQuietPeriodActive) {
        graphGestureQuietPeriodActive = true;
        graphGestureQuietStartedMs = nowMs;
      }
      if (nowMs - graphGestureSuppressionStartedMs >= 450UL &&
          nowMs - graphGestureQuietStartedMs >= 250UL) {
        suppressGraphControllerGestureAfterTouch = false;
        graphGestureQuietPeriodActive = false;
      } else {
        graphControllerGestureSuppressed = true;
      }
    } else {
      graphGestureQuietPeriodActive = false;
      graphGestureQuietStartedMs = 0;
      graphControllerGestureSuppressed = true;
      lastHandledGesture = gesture;
    }
  }

  if (suppressMetricGesture && !metricReleaseGestureSuppressed) {
    suppressMetricGesture = false;
  }
  if (suppressForecastGesture && !forecastReleaseGestureSuppressed) {
    suppressForecastGesture = false;
  }

  if (interactionArbiter.suppressesControllerGesture() ||
      graphControllerGestureSuppressed ||
      releaseGestureSuppressed) {
    if (gesture != GestureCode::None) lastHandledGesture = gesture;
    else lastHandledGesture = GestureCode::None;
    return;
  }

  if (gesture == GestureCode::None) {
    lastHandledGesture = GestureCode::None;
    return;
  }

  if (gesture == GestureCode::Unknown) return;
  if (gesture == lastHandledGesture) return;

  lastHandledGesture = gesture;
  DEBUG_PRINTF(
    "Touch raw gesture: 0x%02X\n",
    static_cast<uint8_t>(gesture));

  // On the current-weather screen, vertical scrolling must originate inside
  // the metric-card panel. This also gates the old gesture-only fallback used
  // when the buffered canvas is unavailable. Horizontal tab swipes are intact.
  if (currentTab == 0 &&
      (gesture == GestureCode::ScreenUp || gesture == GestureCode::ScreenDown) &&
      !metricsGestureStartedInsidePanel) {
    DEBUG_PRINTLN(
      "Ignoring vertical gesture: touch started outside the metrics panel");
    metricsGestureStartedInsidePanel = false;
    return;
  }

  if (currentTab == 1 &&
      (gesture == GestureCode::ScreenUp || gesture == GestureCode::ScreenDown) &&
      forecastGraphView) {
    DEBUG_PRINTLN(
      "Ignoring vertical gesture in graph mode; use a held horizontal drag");
    forecastGestureStartedInsideViewport = false;
    metricsGestureStartedInsidePanel = false;
    return;
  }

  if (currentTab == 1 &&
      (gesture == GestureCode::ScreenUp || gesture == GestureCode::ScreenDown) &&
      !forecastGestureStartedInsideViewport) {
    DEBUG_PRINTLN(
      "Ignoring vertical forecast gesture started outside the list viewport");
    forecastGestureStartedInsideViewport = false;
    metricsGestureStartedInsidePanel = false;
    return;
  }

  handleGesture(gesture);
  forecastGestureStartedInsideViewport = false;
  metricsGestureStartedInsidePanel = false;
}

// =====================================================
// WI-FI
// =====================================================

// Load the preferred saved profile before the platform launcher starts its
// background connection attempt. On a genuinely empty NVS store, migrate the legacy
// secrets.h values once. Corrupt/unknown data is left untouched and falls back
// to the compile-time values rather than silently overwriting saved records.
void initialiseWiFiProfiles() {
#if !USE_DEMO_WEATHER_DATA
  const PersistentWiFiProfileStore::Status loadStatus =
    wifiProfileStore.begin();

  if (loadStatus == PersistentWiFiProfileStore::Status::NotFound) {
    const PersistentWiFiProfileStore::Status migrationStatus =
      wifiProfileStore.save(WIFI_SSID, WIFI_PASSWORD, true);
    if (migrationStatus != PersistentWiFiProfileStore::Status::Success) {
      Serial.println(
        "[WIFI] Could not migrate compile-time profile; using secrets.h fallback.");
      return;
    }
    Serial.println("[WIFI] Imported the compile-time profile into device storage.");
  } else if (loadStatus != PersistentWiFiProfileStore::Status::Success) {
    Serial.println(
      "[WIFI] Saved profiles unavailable; using secrets.h fallback.");
    return;
  }

  PersistentWiFiProfileStore::Profile preferred = {};
  if (wifiProfileStore.getPreferred(preferred)) {
    activeWiFiProfile = preferred;
    wifiSsid = activeWiFiProfile.ssid;
    wifiPassword = activeWiFiProfile.password;
    Serial.println("[WIFI] Loaded the saved preferred network profile.");
  } else {
    // An intentionally empty but valid store must not resurrect the legacy
    // profile after its final saved network has been forgotten.
    wifiSsid = nullptr;
    wifiPassword = nullptr;
    Serial.println("[WIFI] No saved preferred network profile is configured.");
  }
#endif
}

// Start Wi-Fi association without waiting. ConnectivityManager is polled by
// loop(), and WeatherApp observes its state through the callbacks below.
bool startWiFiConnection(uint32_t nowMs) {
#if USE_DEMO_WEATHER_DATA
  (void)nowMs;
  return true;
#else
  if (connectivityManager.isConnected()) {
    return true;
  }

  if (!wifiBackend.configure(wifiSsid, wifiPassword)) {
    DEBUG_PRINTLN("Wi-Fi profile is not configured");
    return false;
  }

  const bool started = connectivityManager.connect(nowMs);
  if (!started && !connectivityManager.isConnecting() &&
      !connectivityManager.isConnected()) {
    DEBUG_PRINTLN("Wi-Fi connection could not be started");
  }
  return started || connectivityManager.isConnecting() ||
         connectivityManager.isConnected();
#endif
}

bool weatherAppConnectionPending() {
#if USE_DEMO_WEATHER_DATA
  return false;
#else
  return connectivityManager.isConnecting();
#endif
}

bool weatherAppHasConnection() {
#if USE_DEMO_WEATHER_DATA
  return true;
#else
  return connectivityManager.isConnected();
#endif
}

// =====================================================
// WEATHER DATA SERVICE ADAPTERS
// =====================================================

// ConnectivityManager and WiFiBackend own association lifecycle; these
// adapters only check connection state and retain Weather status-message
// selection in the sketch. StatusScreenRenderer owns presentation composition;
// WeatherDataService receives only the selected location and API key.
bool fetchWeather() {
#if USE_DEMO_WEATHER_DATA
  return loadDemoWeatherData();
#endif
  if (!weatherAppHasConnection()) {
    if (weatherAvailable) drawWeather();
    else drawError("Wi-Fi unavailable");
    return false;
  }

  drawLoading("Fetching weather...");
  const WeatherDataService::FetchResult result =
    weatherDataService.fetchCurrentWeather(
      locationLat, locationLon, apiKey, locationCoordinatesResolved);

  if (!result.success) {
    DEBUG_PRINTF(
      "Weather request failed (HTTP %d): %s\n",
      result.httpStatus, result.detail.c_str());

    if (weatherAvailable) {
      drawWeather();
    } else if (result.errorCode ==
               WeatherDataService::ErrorCode::RequestInitializationFailed) {
      drawError("HTTP init failed");
    } else if (result.errorCode ==
               WeatherDataService::ErrorCode::Unauthorized) {
      drawError("Check API key");
    } else if (result.errorCode ==
               WeatherDataService::ErrorCode::NotFound) {
      drawError("City not found");
    } else if (result.errorCode ==
               WeatherDataService::ErrorCode::JsonParseFailed) {
      drawError("JSON parse failed");
    } else {
      drawError("Request failed");
    }
    return false;
  }

  // OpenWeather's reporting-station coordinates are only a fallback when the
  // platform LocationService did not obtain a provider fix.
  if (!locationCoordinatesResolved && result.hasLocationCoordinates) {
    locationLat = result.suggestedLatitude;
    locationLon = result.suggestedLongitude;
  }

  DEBUG_PRINTF(
    "%s: %.1f C, %s, humidity %d%%, pressure %d hPa\n",
    weatherCity.c_str(), temperature, weatherDescription.c_str(),
    humidity, pressure);
  return true;
}

// Deterministic UI fixture for feature development. The service owns the data;
// the sketch applies the fixture's location and clamps view-specific offsets.
bool loadDemoWeatherData() {
  const bool loaded = weatherDataService.loadDemoData(time(nullptr));
  if (!loaded) return false;

  locationLat = 53.4808f;
  locationLon = -2.2426f;
  forecastOffset = (std::min)(
    forecastOffset,
    WeatherScrollState::maximumOffset(forecastCount, ITEMS_PER_PAGE));
  forecastGraphOffset = (std::min)(
    (std::max)(forecastGraphOffset, 0),
    WeatherScrollState::maximumOffset(forecastCount, FORECAST_GRAPH_POINTS));
  forecastGraphScrollPixels =
    forecastGraphOffset * FORECAST_GRAPH_POINT_SPACING;
  return true;
}

bool fetchForecast() {
#if USE_DEMO_WEATHER_DATA
  return loadDemoWeatherData();
#endif
  if (!weatherAppHasConnection()) {
    weatherDataService.setForecastUnavailable("Wi-Fi unavailable");
    return false;
  }

  const WeatherDataService::FetchResult result =
    weatherDataService.fetchHourlyForecast(locationLat, locationLon, apiKey);

  // View offsets and graph pixels are Weather presentation state, not data
  // service state. Clamp them only after a response has produced usable data,
  // matching the original early-failure behaviour.
  if (result.success || result.pagesFetched > 0) {
    forecastGraphOffset = (std::min)(
      (std::max)(forecastGraphOffset, 0),
      WeatherScrollState::maximumOffset(forecastCount, FORECAST_GRAPH_POINTS));
    forecastGraphScrollPixels =
      forecastGraphOffset * FORECAST_GRAPH_POINT_SPACING;
    forecastOffset = (std::min)(
      (std::max)(forecastOffset, 0),
      WeatherScrollState::maximumOffset(forecastCount, ITEMS_PER_PAGE));
  }

  if (!result.success) {
    DEBUG_PRINTF(
      "Hourly forecast unavailable (%d pages): %s\n",
      result.pagesFetched, result.detail.c_str());
    return false;
  }

  if (result.partial) {
    DEBUG_PRINTF(
      "Hourly forecast retained %d record(s) after a later page failed: %s\n",
      forecastCount, result.detail.c_str());
  } else {
    DEBUG_PRINTF(
      "Hourly forecast records received: %d across %d page(s)\n",
      forecastCount, result.pagesFetched);
  }

  return forecastAvailable;
}

// =====================================================
// WEATHER APPLICATION ADAPTERS
// =====================================================

// WeatherApp's hooks keep its lifecycle independent from Arduino APIs while
// these adapters connect it to connectivity, location, data and rendering.
// Shared touch sampling and TimeService remain platform-loop responsibilities.
void weatherAppProcessInput() {
  // Touch sampling is promoted to loop() so the platform launcher remains
  // available even while ApplicationManager has no active application.
}

uint32_t weatherAppMillisNow() {
  return (uint32_t)millis();
}

bool weatherAppConnectionPendingForApp() {
  return weatherAppConnectionPending();
}

bool weatherAppIsConnected() {
  return weatherAppHasConnection();
}

void weatherAppResolveLocation() {
#if !USE_DEMO_WEATHER_DATA
  // Avoid an additional IP-geolocation timeout if Wi-Fi association failed.
  if (!weatherAppHasConnection()) {
    currentLocationFix =
      LocationService::configuredFallback(locationLat, locationLon);
    locationCoordinatesResolved = false;
    locationDisplayName = "";
    return;
  }

  const char* googleGeolocationApiKey = nullptr;
#if defined(GOOGLE_GEOLOCATION_API_KEY)
  googleGeolocationApiKey = GOOGLE_GEOLOCATION_API_KEY;
#endif

  // LocationService preserves Google Wi-Fi -> IP -> configured fallback.
  const LocationService::Result locationResult = locationService.resolve(
    weatherAppHasConnection(),
    googleGeolocationApiKey,
    apiKey,
    locationLat,
    locationLon);

  currentLocationFix = locationResult.fix;
  locationLat = currentLocationFix.latitude;
  locationLon = currentLocationFix.longitude;
  locationDisplayName = locationResult.displayName;
  locationCoordinatesResolved =
    currentLocationFix.valid &&
    currentLocationFix.source !=
      LocationService::Source::ConfiguredFallback;

  Serial.printf(
    "[LOCATION] Selected source: %s (valid=%d, observed=%lu ms)\n",
    LocationService::sourceName(currentLocationFix.source),
    currentLocationFix.valid,
    (unsigned long)currentLocationFix.observedAtMs);
  if (currentLocationFix.hasAccuracyRadius) {
    Serial.printf(
      "[LOCATION] Selected accuracy radius: about %.0f m\n",
      (double)currentLocationFix.accuracyRadiusMeters);
  }
#endif
}

void weatherAppDrawLoading(const char* message) {
#if USE_DEMO_WEATHER_DATA
  (void)message;
  drawLoading("Loading demo weather...");
#else
  drawLoading(message != nullptr ? String(message) : String("Loading..."));
#endif
}

bool weatherAppFetchCurrentWeather() {
  return fetchWeather();
}

void weatherAppFetchForecast() {
  (void)fetchForecast();
}

void weatherAppPrepareWeatherAssets() {
  if (weatherAvailable) {
    (void)prepareWeatherIconAsset();
  }
}

bool weatherAppWeatherAvailable() {
  return weatherAvailable;
}

WeatherApp::View weatherAppActiveView() {
  return currentTab == 1
    ? WeatherApp::View::Forecast
    : WeatherApp::View::Weather;
}

void weatherAppDrawWeather() {
  drawWeather();
}

void weatherAppDrawForecast() {
  drawForecast();
}

void weatherAppDrawError(const char* message) {
  drawError(message != nullptr ? String(message) : String("Unknown error"));
}

void weatherAppRefreshStarted() {
  DEBUG_PRINTLN("Refreshing weather and forecast...");
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  initialiseWiFiProfiles();
  DEBUG_PRINTLN();
  Serial.println("Waveshare Weather Dashboard");

  if (!displayManager.begin()) {
    Serial.println("Display initialisation failed!");
    while (true) delay(1000);
  }

  // Buffer only the scrolling forecast viewport, not the entire 320x240 UI.
  // This keeps memory use modest while letting each animation frame flush
  // as one rectangular bitmap to the ST7789.
  if (!displayManager.createForecastCanvas(
        displayManager.width(),
        FORECAST_VIEWPORT_HEIGHT,
        0,
        FORECAST_VIEWPORT_TOP)) {
    DEBUG_PRINTLN("Forecast canvas unavailable; animated scrolling disabled.");
  } else {
    DEBUG_PRINTF(
      "Forecast canvas ready: %d x %d\n",
      displayManager.width(),
      FORECAST_VIEWPORT_HEIGHT);
  }

  // Buffer just the narrow metrics column (about 42 KB at 118x177 RGB565).
  if (!displayManager.createWeatherMetricsCanvas(
        weatherMetricsPanelWidth(),
        WEATHER_METRIC_PANEL_HEIGHT,
        weatherMetricsPanelX(),
        WEATHER_METRIC_PANEL_TOP)) {
    DEBUG_PRINTLN("Weather-metrics canvas unavailable; scrolling animation disabled.");
  } else {
    DEBUG_PRINTF(
      "Weather-metrics canvas ready: %d x %d\n",
      weatherMetricsPanelWidth(),
      WEATHER_METRIC_PANEL_HEIGHT);
  }

  // Full-screen RGB565 buffer used only when changing tabs. This is the
  // largest canvas, so allocate it last and keep instant-switch fallback.
  if (!displayManager.createTabTransitionCanvas(
        displayManager.width(),
        displayManager.height())) {
    DEBUG_PRINTLN("Tab-transition canvas unavailable; tab changes will be instant.");
  } else {
    DEBUG_PRINTF(
      "Tab-transition canvas ready: %d x %d\n",
      displayManager.width(),
      displayManager.height());
  }

  // Determine the actual heap region for every canvas, rather than assuming
  // aligned_alloc() placed them in PSRAM. These checks inspect live pointers.
  reportCanvasAllocation(
    "forecast",
    displayManager.forecastCanvas(),
    (size_t)displayManager.width() * FORECAST_VIEWPORT_HEIGHT * sizeof(uint16_t));
  reportCanvasAllocation(
    "weather metrics",
    displayManager.weatherMetricsCanvas(),
    (size_t)weatherMetricsPanelWidth() *
      WEATHER_METRIC_PANEL_HEIGHT * sizeof(uint16_t));
  reportCanvasAllocation(
    "tab transition",
    displayManager.tabTransitionCanvas(),
    (size_t)displayManager.width() * displayManager.height() * sizeof(uint16_t));
  reportHeapSummary();

  DEBUG_PRINTF("Logical display size: %d x %d\n", displayManager.width(), displayManager.height());
  touchReader.begin();
  DEBUG_PRINTLN(
    touchReader.available()
      ? "CST816 detected at 0x15"
      : "Touch controller not detected");

  timeService.begin();
  currentLocationFix =
    LocationService::configuredFallback(locationLat, locationLon);

#if USE_DEMO_WEATHER_DATA
  // Set a repeatable local clock so the header and forecast time labels render
  // during offline UI development. The clock advances normally after setup.
  timeService.setDemoTime(2026, 10, 28, 18, 0, 0);
  DEBUG_PRINTLN("DEMO MODE ENABLED: network and API calls are disabled.");
#endif
  WeatherApp::Hooks weatherHooks;
  weatherHooks.processInput = weatherAppProcessInput;
  weatherHooks.millisNow = weatherAppMillisNow;
  weatherHooks.connectionPending = weatherAppConnectionPendingForApp;
  weatherHooks.isConnected = weatherAppIsConnected;
  weatherHooks.resolveLocation = weatherAppResolveLocation;
  weatherHooks.fetchCurrentWeather = weatherAppFetchCurrentWeather;
  weatherHooks.fetchForecast = weatherAppFetchForecast;
  weatherHooks.prepareWeatherAssets = weatherAppPrepareWeatherAssets;
  weatherHooks.weatherAvailable = weatherAppWeatherAvailable;
  weatherHooks.activeView = weatherAppActiveView;
  weatherHooks.drawWeather = weatherAppDrawWeather;
  weatherHooks.drawForecast = weatherAppDrawForecast;
  weatherHooks.drawLoading = weatherAppDrawLoading;
  weatherHooks.drawError = weatherAppDrawError;
  weatherHooks.refreshStarted = weatherAppRefreshStarted;
  weatherHooks.pauseWorkflow = weatherAppPauseWorkflow;

  if (!weatherApp.configure(weatherHooks)) {
    Serial.println("Weather application failed to configure: incomplete hooks or already active.");
    while (true) delay(1000);
  }

  if (!applicationManager.registerApplication(weatherApp)) {
    Serial.println("Weather application registration failed.");
    while (true) delay(1000);
  }

  // Boot into the platform launcher with no application active. Draw the
  // launcher first, then start the preferred Wi-Fi association asynchronously
  // from the platform layer; ConnectivityManager keeps it advancing in loop().
  if (!openApplicationLauncher()) {
    Serial.println("Application launcher failed to open.");
    while (true) delay(1000);
  }
  startWiFiConnection(millis());
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  // Do not start a manager-owned association retry while the asynchronous
  // user-facing network scan is still using the Wi-Fi radio.
  const uint32_t nowMs = millis();
  connectivityManager.update(
      nowMs,
      wifiNetworkScanner.state() != WiFiNetworkScanner::State::Scanning);

  updateWiFiSignalIndicator();
  updateWiFiSettings();
  pollTouch();

  // Time synchronisation and the shared header clock belong to the platform,
  // not the currently selected application. This must run while the boot
  // launcher is open (when no application is active) as well as in every app.
  maybeSyncTimeFromAPI();
  if (nowMs - lastPlatformClockRefreshMs >= 1000UL) {
    lastPlatformClockRefreshMs = nowMs;
    refreshHeaderClock();
  }

  applicationManager.update();
  delay(30);
}
