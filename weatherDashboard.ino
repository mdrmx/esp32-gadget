#include <WiFi.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <PNGdec.h>
#include <time.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <stdlib.h>
#include "secrets.h"
#include "src/input/TouchSample.h"
#include "src/input/GestureCode.h"
#include "src/input/GestureRecognizer.h"
#include "src/input/InteractionArbiter.h"
#include "src/ui/ScrollController.h"
#include "src/hardware/DisplayManager.h"
#include "src/hardware/Cst816Reader.h"
#include "src/services/TimeService.h"
#include "src/services/LocationService.h"
#include "src/services/HttpRequestClient.h"

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
long locationTimezoneOffset = 0;
// Prefer a reverse-geocoded display name, with the IP provider's city as a
// fallback. Keep it separate from the weather provider's station name.
String locationDisplayName = "";
bool locationCoordinatesResolved = false;
const unsigned long UPDATE_INTERVAL = 600000UL;

// =====================================================
// TIME SERVICE
// =====================================================

TimeService timeService;

// Weather consumes this provider result; the service owns provider selection.
LocationService locationService;
LocationService::LocationFix currentLocationFix = {};

// Header refresh cadence and last-drawn text are Weather presentation state.
unsigned long lastClockCheckMs = 0;
String lastHeaderClock = "";

// =====================================================
// DISPLAY
// =====================================================

// Owns the physical display, render target, dimensions and off-screen buffers.
DisplayManager displayManager;

// =====================================================
// TOUCH
// =====================================================

// The raw reader owns I2C access and the recognizer interprets controller
// samples. Weather chooses view owners and retains view-specific suppression/actions.
Cst816Reader touchReader;
GestureRecognizer gestureRecognizer;
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

const uint16_t BG = 0x1082;
const uint16_t PANEL = 0x2128;
const uint16_t UI_WHITE = 0xFFFF;
const uint16_t MUTED = 0xBDF7;
const uint16_t ACCENT = 0x5DDF;
const uint16_t UI_YELLOW = 0xFEA0;
const uint16_t UI_GREEN = 0x7E8A;
const uint16_t ERROR_COLOUR = 0xF986;

// =====================================================
// WEATHER DATA
// =====================================================

String weatherCity = "";
String weatherDescription = "";
String weatherMain = "";
String weatherIcon = "03d";
String forecastError = "";
float temperature = 0.0;
float feelsLike = 0.0;
int humidity = 0;
int pressure = 0;
float windSpeedKmh = 0.0f;
int cloudCover = 0;
int visibilityMeters = 0;
float rainLastHourMm = 0.0f;
float currentUvIndex = 0.0f;
float currentRainProbability = 0.0f;
bool visibilityAvailable = false;
bool uvIndexAvailable = false;
bool rainProbabilityAvailable = false;
bool weatherAvailable = false;
bool forecastAvailable = false;
unsigned long lastUpdate = 0;

bool loadDemoWeatherData();

const int MAX_FORECAST = 48;
const int ITEMS_PER_PAGE = 3;

const int WEATHER_METRIC_COUNT = 8;
const int WEATHER_METRICS_VISIBLE = 3;
const int WEATHER_METRIC_PANEL_TOP = 47;
const int WEATHER_METRIC_PANEL_HEIGHT = 177;
const int WEATHER_METRIC_PANEL_BOTTOM =
  WEATHER_METRIC_PANEL_TOP + WEATHER_METRIC_PANEL_HEIGHT;
const int WEATHER_METRIC_CARD_HEIGHT = 54;
const int WEATHER_METRIC_ROW_STEP = 59;
// Shared elastic-scroll tuning for metric cards and hourly forecast.
// Keep the overscroll cap below half a row step, so release snaps to the boundary.
const int SCROLL_OVERSCROLL_MAX_PX = 20;
const int SCROLL_OVERSCROLL_RESISTANCE_DIVISOR = 3;
const int WEATHER_METRIC_ROW_TOP = 2;
int weatherMetricOffset = 0;

// Keep three 44 px list cards with 4 px gaps visible through y=212.
// Controls are 32 px tall, with a 2 px gap below the header and above the List
// rows. In Graph mode the same controls overlay the chart's blank upper strip.
const int FORECAST_VIEWPORT_TOP = 64;
const int FORECAST_VIEWPORT_HEIGHT = 152;
const int FORECAST_VIEWPORT_BOTTOM =
  FORECAST_VIEWPORT_TOP + FORECAST_VIEWPORT_HEIGHT;

// Keep the shared header and footer fixed while animating only the main body.
const int TAB_TRANSITION_CONTENT_TOP = 37;
const int TAB_TRANSITION_FOOTER_HEIGHT = 12;

const int FORECAST_ROW_TOP = FORECAST_VIEWPORT_TOP + 8;
const int FORECAST_ROW_STEP = 48;
const int FORECAST_CARD_HEIGHT = 44;
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
const int FORECAST_GRAPH_VERTICAL_SHIFT = 12;
// Generous movement tolerance keeps taps reliable while still allowing a
// deliberate horizontal tab swipe to cancel a control press.
const int FORECAST_CONTROL_TAP_SLOP_PX = 40;
// Limit full-viewport bitmap transfers during fast finger drags.
const unsigned long FORECAST_DRAG_RENDER_INTERVAL_MS = 50UL;

struct ForecastItem {
  String timeText;
  String description;  // OpenWeather condition group, e.g. Clouds, Rain, Clear.
  float temp;
  float feelsLike;
  float rainProbability; // 0.0 to 1.0 from OpenWeather pop.
  uint32_t timestamp;
};

ForecastItem forecast[MAX_FORECAST];
int forecastCount = 0;
int forecastOffset = 0;
bool forecastGraphView = false;
int forecastGraphOffset = 0; // First hour currently nearest the left edge.
int forecastGraphScrollPixels = 0;
uint8_t forecastControlTouch = 0; // 3=list, 4=graph.
int forecastControlStartX = 0;
int forecastControlStartY = 0;
int currentTab = 0;

// =====================================================
// TEXT HELPERS
// =====================================================

void drawText(const String& text, int x, int y, uint8_t size, uint16_t colour) {
  displayManager.target()->setTextSize(size);
  displayManager.target()->setTextColor(colour);
  displayManager.target()->setCursor(x, y);
  displayManager.target()->print(text);
}

void drawCenteredText(const String& text, int y, uint8_t size, uint16_t colour) {
  displayManager.target()->setTextSize(size);
  displayManager.target()->setTextColor(colour);

  int16_t x1, y1;
  uint16_t w, h;
  displayManager.target()->getTextBounds(text.c_str(), 0, y, &x1, &y1, &w, &h);

  int x = (displayManager.width() - (int)w) / 2;
  if (x < 4) x = 4;
  if (x + (int)w > displayManager.width() - 4) x = displayManager.width() - 4 - w;

  displayManager.target()->setCursor(x, y);
  displayManager.target()->print(text);
}

int measureTextWidth(const String& text, uint8_t size) {
  displayManager.target()->setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  displayManager.target()->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  return (int)w;
}

int measureTextHeight(const String& text, uint8_t size) {
  displayManager.target()->setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  displayManager.target()->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  return (int)h;
}

// Wrap the description to the weather column and centre each line around
// centreX. Returns the rendered block height so following data can be placed
// relative to its actual line count.
int drawWrappedCenteredText(
  const String& text,
  int centreX,
  int maxWidth,
  int topY,
  uint8_t size,
  uint16_t colour
) {
  String lines[2];
  int lineCount = 0;
  String currentLine;
  int wordStart = 0;

  while (wordStart < (int)text.length()) {
    while (wordStart < (int)text.length() && text[wordStart] == ' ') {
      wordStart++;
    }
    if (wordStart >= (int)text.length()) break;

    int wordEnd = text.indexOf(' ', wordStart);
    if (wordEnd < 0) wordEnd = text.length();
    String word = text.substring(wordStart, wordEnd);
    wordStart = wordEnd + 1;

    String candidate = currentLine.length() ? currentLine + " " + word : word;
    if (measureTextWidth(candidate, size) <= maxWidth) {
      currentLine = candidate;
    } else if (lineCount == 0 && currentLine.length() > 0) {
      lines[lineCount++] = currentLine;
      currentLine = word;
    } else if (lineCount == 1) {
      // There is no room for another line; shorten the second line neatly.
      String shortened = currentLine;
      while (shortened.length() > 0 &&
             measureTextWidth(shortened + "...", size) > maxWidth) {
        int lastSpace = shortened.lastIndexOf(' ');
        if (lastSpace >= 0) {
          shortened = shortened.substring(0, lastSpace);
        } else {
          shortened = shortened.substring(0, shortened.length() - 1);
        }
      }
      currentLine = shortened + "...";
      break;
    } else {
      currentLine = word;
    }
  }

  if (lineCount < 2 && currentLine.length() > 0) {
    lines[lineCount++] = currentLine;
  }
  if (lineCount == 0) return 0;

  displayManager.target()->setTextSize(size);
  displayManager.target()->setTextColor(colour);
  int lineHeight = measureTextHeight(lines[0], size);
  if (lineHeight <= 0) lineHeight = 8 * size;

  for (int i = 0; i < lineCount; i++) {
    int width = measureTextWidth(lines[i], size);
    drawText(lines[i], centreX - width / 2, topY + i * lineHeight, size, colour);
  }

  return lineCount * lineHeight;
}

// Draw one line centred around centreX and return its measured height.
int drawCenteredWeatherLine(
  const String& text,
  int centreX,
  int y,
  uint8_t size,
  uint16_t colour
) {
  displayManager.target()->setTextSize(size);
  int16_t x1, y1;
  uint16_t width, height;
  displayManager.target()->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &width, &height);
  drawText(text, centreX - (int)width / 2, y, size, colour);
  return height > 0 ? (int)height : 8 * size;
}

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

String getHeaderLocationText() {
  // Prefer the resolved IP location in the UI. The weather provider's "name"
  // may instead identify a nearby reporting station or larger city.
  String label = locationDisplayName.length() > 0 ? locationDisplayName : weatherCity;

  // Before either response arrives, use the configured city.
  if (label.length() == 0) {
    label = String(CITY);
    int comma = label.indexOf(',');
    if (comma > 0) {
      label = label.substring(0, comma);
    }
  }

  label.trim();
  if (label.length() > 17) {
    label = label.substring(0, 16) + ".";
  }
  return label;
}

void drawHeaderClockOnly(const String& clockText) {
  displayManager.target()->fillRect(displayManager.width() - 78, 0, 78, 36, PANEL);

  displayManager.target()->setTextSize(2);
  displayManager.target()->setTextColor(ACCENT);
  int16_t x1, y1;
  uint16_t w, h;
  displayManager.target()->getTextBounds(clockText.c_str(), 0, 10, &x1, &y1, &w, &h);
  int x = displayManager.width() - 10 - (int)w;
  if (x < displayManager.width() - 70) {
    x = displayManager.width() - 70;
  }
  drawText(clockText, x, 10, 2, ACCENT);
  lastHeaderClock = clockText;
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
    timeService.syncIfDue(WiFi.status() == WL_CONNECTED, millis());

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
  displayManager.target()->fillRect(0, 0, displayManager.width(), 36, PANEL);

  String locationLabel = getHeaderLocationText();
  drawText(locationLabel, 10, 10, 2, UI_WHITE);

  String clockText = timeService.headerClockText();
  displayManager.target()->setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  displayManager.target()->getTextBounds(clockText.c_str(), 0, 10, &x1, &y1, &w, &h);
  int x = displayManager.width() - 10 - (int)w;
  if (x < displayManager.width() - 70) {
    x = displayManager.width() - 70;
  }
  drawText(clockText, x, 10, 2, ACCENT);
  lastHeaderClock = clockText;

  displayManager.target()->drawFastHLine(0, 36, displayManager.width(), ACCENT);
}

void drawTabIndicator() {
  int y = displayManager.height() - 8;
  displayManager.target()->fillRoundRect(displayManager.width() / 2 - 24, y, 48, 4, 2, PANEL);
  displayManager.target()->fillRoundRect(displayManager.width() / 2 - 24 + (currentTab * 24), y, 24, 4, 2, UI_YELLOW);
}

// =====================================================
// LOADING AND ERROR SCREENS
// =====================================================

void drawLoading(const String& message) {
  displayManager.target()->fillScreen(BG);
  drawHeader();
  drawCenteredText("OpenWeatherMap", 83, 2, ACCENT);
  drawCenteredText(message, 119, 1, UI_WHITE);
}

void drawError(const String& message) {
  displayManager.target()->fillScreen(BG);
  drawHeader();
  drawCenteredText("WEATHER ERROR", 82, 2, ERROR_COLOUR);
  drawCenteredText(message, 117, 1, UI_WHITE);
  drawCenteredText("Will retry automatically", 143, 1, MUTED);
}

// =====================================================
// OPENWEATHER ICON PNG DOWNLOAD + DECODING
// Uses official PNG assets: https://openweathermap.org/img/wn/<code>.png
// Install the "PNGdec" library by Larry Bank from Arduino Library Manager.
// =====================================================

PNG weatherPngDecoder;
uint8_t* weatherIconPngData = nullptr;
size_t weatherIconPngLength = 0;
String cachedWeatherIconCode = "";

struct WeatherPngDrawContext {
  int x;
  int y;
  uint32_t backgroundBGR;
};

int drawWeatherPngLine(PNGDRAW* pDraw) {
  WeatherPngDrawContext* context = (WeatherPngDrawContext*)pDraw->pUser;
  if (context == nullptr || pDraw->iWidth > 100) {
    return 0;
  }

  uint16_t linePixels[100];
  weatherPngDecoder.getLineAsRGB565(
    pDraw,
    linePixels,
    PNG_RGB565_LITTLE_ENDIAN,
    context->backgroundBGR);

  displayManager.target()->draw16bitRGBBitmap(
    context->x,
    context->y + pDraw->y,
    linePixels,
    pDraw->iWidth,
    1);

  return 1;
}

bool loadOpenWeatherIconPNG() {
#if USE_DEMO_WEATHER_DATA
  return false;  // Keep demo mode completely offline.
#endif
  if (weatherIcon.length() != 3) {
    DEBUG_PRINTF("Invalid OpenWeather icon code: %s\n", weatherIcon.c_str());
    return false;
  }

  if (weatherIconPngData != nullptr && weatherIconPngLength > 0 && cachedWeatherIconCode == weatherIcon) {
    return true;
  }

String url = "https://openweathermap.org/img/wn/" + weatherIcon + "@2x.png";
  DEBUG_PRINTF("Downloading OpenWeather icon %s\n", weatherIcon.c_str());

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
  if (contentLength <= 8 || contentLength > 20000) {
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

  while (bytesRead < (size_t)contentLength && millis() - startedAt < 8000UL) {
    int available = stream->available();

    if (available > 0) {
      size_t remaining = (size_t)contentLength - bytesRead;
      size_t toRead = (size_t)available < remaining ? (size_t)available : remaining;
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

  if (bytesRead != (size_t)contentLength || newData[0] != 0x89 || newData[1] != 0x50 || newData[2] != 0x4E || newData[3] != 0x47) {
    DEBUG_PRINTF(
      "OpenWeather icon download incomplete/invalid: %u of %d bytes\n",
      (unsigned)bytesRead,
      contentLength);
    free(newData);
    return false;
  }

  if (weatherIconPngData != nullptr) {
    free(weatherIconPngData);
  }

  weatherIconPngData = newData;
  weatherIconPngLength = bytesRead;
  cachedWeatherIconCode = weatherIcon;

  DEBUG_PRINTF(
    "Cached OpenWeather icon %s (%u bytes)\n",
    cachedWeatherIconCode.c_str(),
    (unsigned)weatherIconPngLength);

  return true;
}

int drawOpenWeatherIcon(int x, int y) {
  if (!loadOpenWeatherIconPNG()) {
    return 0;
  }

  int openResult = weatherPngDecoder.openRAM(
    weatherIconPngData,
    (int)weatherIconPngLength,
    drawWeatherPngLine);

  if (openResult != PNG_SUCCESS) {
    DEBUG_PRINTF("OpenWeather PNG open failed: %d\n", openResult);
    return 0;
  }

  int iconWidth = weatherPngDecoder.getWidth();
  int iconHeight = weatherPngDecoder.getHeight();

  if (iconWidth > 100 || iconHeight > 100) {
    DEBUG_PRINTF("OpenWeather PNG dimensions unexpected: %d x %d\n", iconWidth, iconHeight);
    weatherPngDecoder.close();
    return 0;
  }

  WeatherPngDrawContext context;
  context.x = x;
  context.y = y;
  // BG is RGB565 0x1082, approximately RGB(16,16,16). PNGdec expects
  // the background as 00BBGGRR when blending transparent PNG pixels.
  context.backgroundBGR = 0x00101010UL;

  int decodeResult = weatherPngDecoder.decode(&context, PNG_FAST_PALETTE);
  if (decodeResult != PNG_SUCCESS) {
    DEBUG_PRINTF("OpenWeather PNG decode failed: %d\n", decodeResult);
  }

  weatherPngDecoder.close();
  return decodeResult == PNG_SUCCESS ? iconHeight : 0;
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

String weatherMetricLabel(int index) {
  switch (index) {
    case 0: return "HUMIDITY";
    case 1: return "PRESSURE";
    case 2: return "UV INDEX";
    case 3: return "RAIN CHANCE";
    case 4: return "WIND";
    case 5: return "CLOUD COVER";
    case 6: return "VISIBILITY";
    case 7: return "RAIN (1H)";
    default: return "";
  }
}

String weatherMetricValue(int index) {
  switch (index) {
    case 0: return String(humidity) + "%";
    case 1: return String(pressure) + " hPa";
    case 2:
      return uvIndexAvailable ? String(currentUvIndex, 1) : "--";
    case 3:
      return rainProbabilityAvailable
        ? String((int)(currentRainProbability * 100.0f + 0.5f)) + "%"
        : "--";
    case 4: return String(windSpeedKmh, 1) + " km/h";
    case 5: return String(cloudCover) + "%";
    case 6:
      return visibilityAvailable
        ? String(visibilityMeters / 1000.0f, 1) + " km"
        : "--";
    case 7: return String(rainLastHourMm, 1) + " mm";
    default: return "--";
  }
}

// Draw a metric card while respecting the vertical clipping bounds of its
// panel. viewportTop is zero for the small metrics canvas and the panel's
// screen Y when rendering directly to the LCD or full-screen transition canvas.
void drawWeatherMetricItem(
  Arduino_GFX* target,
  int index,
  int x,
  int y,
  int panelWidth,
  int viewportTop
) {
  const int localY = y - viewportTop;
  const int cardBottom = localY + WEATHER_METRIC_CARD_HEIGHT;
  const int clippedTop = max(0, localY);
  const int clippedBottom = min(WEATHER_METRIC_PANEL_HEIGHT, cardBottom);
  if (clippedTop >= clippedBottom) return;

  String label = weatherMetricLabel(index);
  String value = weatherMetricValue(index);
  const uint8_t valueSize = value.length() <= 6 ? 2 : 1;
  int16_t x1, y1;
  uint16_t w, h;

  if (localY >= 0 && cardBottom <= WEATHER_METRIC_PANEL_HEIGHT) {
    target->fillRoundRect(
      x + 2,
      viewportTop + localY,
      panelWidth - 4,
      WEATHER_METRIC_CARD_HEIGHT,
      6,
      PANEL);

    target->setTextSize(1);
    target->setTextColor(MUTED);
    target->setCursor(x + 9, viewportTop + localY + 7);
    target->print(label);

    target->setTextSize(valueSize);
    target->setTextColor(ACCENT);
    target->getTextBounds(value.c_str(), 0, 0, &x1, &y1, &w, &h);
    const int valueX = x + (panelWidth - (int)w) / 2;
    const int valueY = viewportTop + localY +
      (valueSize == 2 ? 23 : 28);
    target->setCursor(valueX, valueY);
    target->print(value);
    return;
  }

  // Partial edge cards use a clipped rectangular slice, as the forecast cards
  // do. Never send negative/off-panel Y coordinates to Arduino_GFX primitives.
  target->fillRect(
    x + 2,
    viewportTop + clippedTop,
    panelWidth - 4,
    clippedBottom - clippedTop,
    PANEL);

  const int labelLocalY = localY + 7;
  if (labelLocalY >= 0 && labelLocalY + 8 <= WEATHER_METRIC_PANEL_HEIGHT) {
    target->setTextSize(1);
    target->setTextColor(MUTED);
    target->setCursor(x + 9, viewportTop + labelLocalY);
    target->print(label);
  }

  target->setTextSize(valueSize);
  target->getTextBounds(value.c_str(), 0, 0, &x1, &y1, &w, &h);
  const int valueLocalY = localY + (valueSize == 2 ? 23 : 28);
  if (valueLocalY >= 0 &&
      valueLocalY + (int)h <= WEATHER_METRIC_PANEL_HEIGHT) {
    const int valueX = x + (panelWidth - (int)w) / 2;
    target->setTextColor(ACCENT);
    target->setCursor(valueX, viewportTop + valueLocalY);
    target->print(value);
  }
}

// Render the metrics panel into a small off-screen buffer, then transfer the
// complete panel in one bitmap operation. This avoids clearing/repainting
// individual cards directly to the LCD for each animation frame.
void drawWeatherMetricsViewport(int baseOffset, int scrollPixels) {
  int panelWidth = weatherMetricsPanelWidth();

  if (displayManager.weatherMetricsCanvas() != nullptr && !displayManager.renderingTabTransition()) {
    displayManager.weatherMetricsCanvas()->fillScreen(BG);

    int firstIndex = max(0, baseOffset - 1);
    int lastIndex = min(
      WEATHER_METRIC_COUNT,
      baseOffset + WEATHER_METRICS_VISIBLE + 1);

    for (int index = firstIndex; index < lastIndex; index++) {
      int y = WEATHER_METRIC_ROW_TOP +
              (index - baseOffset) * WEATHER_METRIC_ROW_STEP -
              scrollPixels;
      if (y + WEATHER_METRIC_CARD_HEIGHT <= 0 ||
          y >= WEATHER_METRIC_PANEL_HEIGHT) {
        continue;
      }

      drawWeatherMetricItem(
        displayManager.weatherMetricsCanvas(), index, 0, y, panelWidth, 0);
    }

    displayManager.weatherMetricsCanvas()->flush();
    return;
  }

  // Direct-render fallback if the PSRAM buffer is unavailable. Use the same
  // clipping renderer so boundary overscroll doesn't make edge cards vanish.
  displayManager.target()->fillRect(
    weatherMetricsPanelX(),
    WEATHER_METRIC_PANEL_TOP,
    panelWidth,
    WEATHER_METRIC_PANEL_HEIGHT,
    BG);

  int firstIndex = max(0, baseOffset - 1);
  int lastIndex = min(
    WEATHER_METRIC_COUNT,
    baseOffset + WEATHER_METRICS_VISIBLE + 1);
  for (int index = firstIndex; index < lastIndex; index++) {
    int localY = WEATHER_METRIC_ROW_TOP +
                 (index - baseOffset) * WEATHER_METRIC_ROW_STEP -
                 scrollPixels;
    if (localY + WEATHER_METRIC_CARD_HEIGHT <= 0 ||
        localY >= WEATHER_METRIC_PANEL_HEIGHT) {
      continue;
    }

    drawWeatherMetricItem(
      displayManager.target(),
      index,
      weatherMetricsPanelX(),
      WEATHER_METRIC_PANEL_TOP + localY,
      panelWidth,
      WEATHER_METRIC_PANEL_TOP);
  }
}

void drawWeatherMetricsPanel(int baseOffset, int scrollPixels) {
  int maxOffset = max(
    0,
    WEATHER_METRIC_COUNT - WEATHER_METRICS_VISIBLE);
  if (baseOffset < 0) baseOffset = 0;
  if (baseOffset > maxOffset) baseOffset = maxOffset;

  drawWeatherMetricsViewport(baseOffset, scrollPixels);
}

int maxWeatherMetricScrollPixels() {
  const int maxOffset = max(
    0,
    WEATHER_METRIC_COUNT - WEATHER_METRICS_VISIBLE);
  return maxOffset * WEATHER_METRIC_ROW_STEP;
}

void drawWeatherMetricsAtScrollPosition(int pixels) {
  // Snap animation also starts from the elastic position, so allow a small
  // bounded overrun here and rely on the off-screen panel to clip the cards.
  const int maxScroll = maxWeatherMetricScrollPixels();
  pixels = ScrollController::clampElasticPosition(
    pixels, maxScroll, SCROLL_OVERSCROLL_MAX_PX);

  int baseOffset = pixels / WEATHER_METRIC_ROW_STEP;
  int scrollPixels = pixels % WEATHER_METRIC_ROW_STEP;
  drawWeatherMetricsViewport(baseOffset, scrollPixels);
}

void animateWeatherMetricsSnap(int startPixels, int targetOffset) {
  const int maxOffset = max(
    0,
    WEATHER_METRIC_COUNT - WEATHER_METRICS_VISIBLE);
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
  int maxOffset = max(
    0,
    WEATHER_METRIC_COUNT - WEATHER_METRICS_VISIBLE);

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

  // Lay out the weather stack from top to bottom. Each element's measured
  // height determines where the next element begins, so a wrapped description
  // moves the temperature and feels-like value down without overlapping.
  const int contentCentreX = 96;
  const int descriptionMaxWidth = 176;

  // Adjust these gaps independently (all values are pixels).
  const int iconDescriptionGap = -7;
  const int descriptionTemperatureGap = 10;
  const int temperatureFeelsGap = 8;

  int contentY = 37;

  int iconHeight = drawOpenWeatherIcon(46, contentY);
  if (iconHeight > 0) contentY += iconHeight + iconDescriptionGap;

  int descriptionHeight = drawWrappedCenteredText(
    weatherDescription,
    contentCentreX,
    descriptionMaxWidth,
    contentY,
    2,
    MUTED);
  contentY += descriptionHeight + descriptionTemperatureGap;

  String temperatureText = String(temperature, 1) + " C";
  int temperatureHeight = drawCenteredWeatherLine(
    temperatureText,
    contentCentreX,
    contentY,
    2,
    UI_YELLOW);
  contentY += temperatureHeight + temperatureFeelsGap;

  String feelsText = "Feels " + String(feelsLike, 1) + " C";
  drawCenteredWeatherLine(
    feelsText,
    contentCentreX,
    contentY,
    1,
    MUTED);

  drawWeatherMetricsPanel(weatherMetricOffset, 0);
  drawTabIndicator();
}

// =====================================================
// FORECAST SCREEN
// =====================================================

String forecastCompactTime(int index) {
  String label = forecast[index].timeText;
  const int separator = label.lastIndexOf(' ');
  if (separator >= 0) label = label.substring(separator + 1);
  return label;
}

// Small vector weather symbols keep hourly rows compact without downloading and
// caching a separate PNG for every forecast condition.
void drawMiniWeatherIcon(
  Arduino_GFX* target,
  const String& condition,
  int x,
  int y
) {
  const bool clear = condition.indexOf("Clear") >= 0;
  const bool rain = condition.indexOf("Rain") >= 0 ||
                    condition.indexOf("Drizzle") >= 0;
  const bool snow = condition.indexOf("Snow") >= 0;
  const bool thunder = condition.indexOf("Thunder") >= 0;
  const bool mist = condition.indexOf("Mist") >= 0 ||
                    condition.indexOf("Fog") >= 0 ||
                    condition.indexOf("Haze") >= 0;

  if (clear) {
    target->fillCircle(x + 8, y + 7, 4, UI_YELLOW);
    target->drawFastHLine(x + 1, y + 7, 3, UI_YELLOW);
    target->drawFastHLine(x + 12, y + 7, 3, UI_YELLOW);
    target->drawFastVLine(x + 8, y, 3, UI_YELLOW);
    target->drawFastVLine(x + 8, y + 11, 3, UI_YELLOW);
    target->drawLine(x + 3, y + 2, x + 5, y + 4, UI_YELLOW);
    target->drawLine(x + 11, y + 10, x + 13, y + 12, UI_YELLOW);
    target->drawLine(x + 11, y + 4, x + 13, y + 2, UI_YELLOW);
    target->drawLine(x + 3, y + 12, x + 5, y + 10, UI_YELLOW);
    return;
  }

  if (mist) {
    target->drawFastHLine(x + 1, y + 4, 14, MUTED);
    target->drawFastHLine(x, y + 8, 15, UI_WHITE);
    target->drawFastHLine(x + 2, y + 12, 12, MUTED);
    return;
  }

  // The cloud is shared by cloudy, rain, snow and thunderstorm conditions.
  target->fillCircle(x + 5, y + 8, 3, UI_WHITE);
  target->fillCircle(x + 9, y + 5, 4, UI_WHITE);
  target->fillCircle(x + 13, y + 8, 3, UI_WHITE);
  target->fillRect(x + 4, y + 7, 10, 5, UI_WHITE);

  if (thunder) {
    target->drawLine(x + 9, y + 10, x + 6, y + 14, UI_YELLOW);
    target->drawLine(x + 6, y + 14, x + 9, y + 14, UI_YELLOW);
    target->drawLine(x + 9, y + 14, x + 7, y + 17, UI_YELLOW);
  } else if (snow) {
    target->drawFastVLine(x + 6, y + 13, 4, ACCENT);
    target->drawFastHLine(x + 4, y + 14, 4, ACCENT);
    target->drawFastHLine(x + 9, y + 15, 4, ACCENT);
  } else if (rain) {
    target->drawLine(x + 5, y + 13, x + 3, y + 16, ACCENT);
    target->drawLine(x + 10, y + 13, x + 8, y + 16, ACCENT);
    target->drawLine(x + 14, y + 13, x + 12, y + 16, ACCENT);
  }
}

void drawForecastItem(Arduino_GFX* target, int index, int y) {
  target->fillRoundRect(
    4, y, displayManager.width() - 8, FORECAST_CARD_HEIGHT, 4, PANEL);

  // Four concise fields fit on the small display: time, condition icon,
  // temperature, feels-like temperature, and probability of precipitation.
  target->setTextSize(1);
  target->setTextColor(UI_WHITE);
  target->setCursor(10, y + 18);
  target->print(forecastCompactTime(index));

  drawMiniWeatherIcon(target, forecast[index].description, 48, y + 13);

  String temp = String((int)round(forecast[index].temp)) + "C";
  target->setTextSize(2);
  target->setTextColor(UI_YELLOW);
  target->setCursor(72, y + 14);
  target->print(temp);

  String feelsText = "FL " +
    String((int)round(forecast[index].feelsLike)) + "C";
  target->setTextSize(1);
  target->setTextColor(MUTED);
  target->setCursor(145, y + 18);
  target->print(feelsText);

  const int rainPercent = constrain(
    (int)round(forecast[index].rainProbability * 100.0f), 0, 100);
  String rainText = "POP " + String(rainPercent) + "%";
  int16_t x1, y1;
  uint16_t w, h;
  target->getTextBounds(rainText.c_str(), 0, 0, &x1, &y1, &w, &h);
  target->setTextColor(UI_WHITE);
  target->setCursor(displayManager.width() - 10 - (int)w, y + 18);
  target->print(rainText);
}

// Draw a forecast card into the viewport canvas while explicitly clipping
// edge rows. Avoid passing negative coordinates to rounded-rectangle or text
// primitives, but keep the visible slice of each card moving smoothly.
void drawForecastItemClipped(Arduino_GFX* target, int index, int y) {
  const int cardTop = y - FORECAST_VIEWPORT_TOP;
  const int cardBottom = cardTop + FORECAST_CARD_HEIGHT;
  const int clippedTop = max(0, cardTop);
  const int clippedBottom = min(FORECAST_VIEWPORT_HEIGHT, cardBottom);
  if (clippedTop >= clippedBottom) return;

  if (cardTop >= 0 && cardBottom <= FORECAST_VIEWPORT_HEIGHT) {
    drawForecastItem(target, index, cardTop);
    return;
  }

  // Partial edge rows get a clipped body and only fields wholly inside the
  // viewport are drawn. Never pass negative/off-panel Y values to primitives.
  target->fillRect(4, clippedTop, displayManager.width() - 8,
                   clippedBottom - clippedTop, PANEL);

  const int fieldY = cardTop + 18;
  if (fieldY >= 0 && fieldY + 8 <= FORECAST_VIEWPORT_HEIGHT) {
    target->setTextSize(1);
    target->setTextColor(UI_WHITE);
    target->setCursor(10, fieldY);
    target->print(forecastCompactTime(index));

    String feelsText = "FL " +
      String((int)round(forecast[index].feelsLike)) + "C";
    target->setTextColor(MUTED);
    target->setCursor(145, fieldY);
    target->print(feelsText);

    const int rainPercent = constrain(
      (int)round(forecast[index].rainProbability * 100.0f), 0, 100);
    String rainText = "POP " + String(rainPercent) + "%";
    int16_t x1, y1;
    uint16_t w, h;
    target->getTextBounds(rainText.c_str(), 0, 0, &x1, &y1, &w, &h);
    target->setTextColor(UI_WHITE);
    target->setCursor(displayManager.width() - 10 - (int)w, fieldY);
    target->print(rainText);
  }

  if (cardTop + 13 >= 0 &&
      cardTop + 31 <= FORECAST_VIEWPORT_HEIGHT) {
    drawMiniWeatherIcon(
      target, forecast[index].description, 48, cardTop + 13);

    String temp = String((int)round(forecast[index].temp)) + "C";
    target->setTextSize(2);
    target->setTextColor(UI_YELLOW);
    target->setCursor(72, cardTop + 14);
    target->print(temp);
  }
}

void drawForecastModeSelector() {
  // The viewport canvas begins at y=64, so its flush can overwrite the bottom
  // edge of this selector. This small repaint restores it after graph frames.
  displayManager.target()->fillRoundRect(179, 41, 137, 26, 6, PANEL);
  if (forecastGraphView) {
    displayManager.target()->fillRoundRect(248, 41, 67, 24, 5, UI_WHITE);
  } else {
    displayManager.target()->fillRoundRect(181, 41, 67, 24, 5, UI_WHITE);
  }
  drawText("LIST", 201, 50, 1, forecastGraphView ? MUTED : BG);
  drawText("GRAPH", 265, 50, 1, forecastGraphView ? BG : MUTED);
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
    drawText("HOURLY", 8, 50, 1, UI_WHITE);
  }

  drawForecastModeSelector();
}

int maxForecastGraphScrollPixels() {
  return max(0, forecastCount - FORECAST_GRAPH_POINTS) *
         FORECAST_GRAPH_POINT_SPACING;
}

void syncForecastGraphOffsetFromScroll() {
  const int maxOffset = max(0, forecastCount - FORECAST_GRAPH_POINTS);
  forecastGraphOffset = constrain(
    max(0, forecastGraphScrollPixels) / FORECAST_GRAPH_POINT_SPACING,
    0,
    maxOffset);
}

void drawForecastGraph(Arduino_GFX* target, int originY) {
  if (forecastCount <= 0) return;

  const int left = 23;
  const int right = displayManager.width() - 17;
  const int plotWidth = right - left;
  const int pointSpacing = FORECAST_GRAPH_POINT_SPACING;
  const int maxScroll = maxForecastGraphScrollPixels();
  const int scroll = ScrollController::clampElasticPosition(
    forecastGraphScrollPixels,
    maxScroll,
    SCROLL_OVERSCROLL_MAX_PX);
  const int visibleScroll = max(0, scroll);

  // Use the visible hours plus their neighbours to keep a consistent vertical
  // scale while the finger moves between hourly positions.
  const int rangeStart = constrain(
    visibleScroll / pointSpacing, 0, max(0, forecastCount - 1));
  const int rangeEnd = min(
    forecastCount, rangeStart + FORECAST_GRAPH_POINTS + 1);

  float minTemp = forecast[rangeStart].temp;
  float maxTemp = forecast[rangeStart].temp;
  for (int i = rangeStart + 1; i < rangeEnd; i++) {
    minTemp = min(minTemp, forecast[i].temp);
    maxTemp = max(maxTemp, forecast[i].temp);
  }
  minTemp -= 1.0f;
  maxTemp += 1.0f;

  const int plotTop = 37 + FORECAST_GRAPH_VERTICAL_SHIFT;
  const int plotBottom = 89 + FORECAST_GRAPH_VERTICAL_SHIFT;
  const int plotHeight = plotBottom - plotTop;
  const uint16_t gridColour = PANEL;
  target->drawFastHLine(left, originY + plotTop, plotWidth, gridColour);
  target->drawFastHLine(
    left, originY + (plotTop + plotBottom) / 2, plotWidth, gridColour);
  target->drawFastHLine(left, originY + plotBottom, plotWidth, gridColour);

  // Draw one point per forecast hour at a fixed horizontal spacing. This lets
  // the chart follow the finger pixel-for-pixel rather than jumping by pages.
  const int firstIndex = max(0, visibleScroll / pointSpacing - 1);
  const int lastIndex = min(
    forecastCount, (visibleScroll + plotWidth) / pointSpacing + 2);

  int previousX = 0;
  int previousY = 0;
  bool hasPrevious = false;
  for (int index = firstIndex; index < lastIndex; index++) {
    const int x = left + index * pointSpacing - scroll;
    const float ratio = (forecast[index].temp - minTemp) /
                        (maxTemp - minTemp);
    const int y = plotBottom - (int)round(ratio * plotHeight);

    if (hasPrevious && previousX <= right && x >= left) {
      int lineStartX = previousX;
      int lineStartY = previousY;
      int lineEndX = x;
      int lineEndY = y;
      const int dx = x - previousX;
      if (dx > 0) {
        if (lineStartX < left) {
          lineStartY = previousY +
            (y - previousY) * (left - previousX) / dx;
          lineStartX = left;
        }
        if (lineEndX > right) {
          lineEndY = previousY +
            (y - previousY) * (right - previousX) / dx;
          lineEndX = right;
        }
        if (lineStartX <= lineEndX) {
          target->drawLine(
            lineStartX, originY + lineStartY,
            lineEndX, originY + lineEndY, UI_YELLOW);
        }
      }
    }

    if (x >= left && x <= right) {
      target->fillCircle(x, originY + y, 2, UI_WHITE);
    }

    // The first and sixth visible points sit close to the plot edges. Their
    // icons still fit on the display, so clip against the screen rather than
    // hiding them just because they cross the inner plot boundary.
    if (x >= 8 && x <= displayManager.width() - 8) {
      drawMiniWeatherIcon(
        target, forecast[index].description, x - 8,
        originY + 2 + FORECAST_GRAPH_VERTICAL_SHIFT);
    }

    String tempText = String((int)round(forecast[index].temp)) + "C";
    int16_t x1, y1;
    uint16_t w, h;
    target->setTextSize(1);
    target->getTextBounds(tempText.c_str(), 0, 0, &x1, &y1, &w, &h);
    if (x - (int)w / 2 >= 2 &&
        x + ((int)w + 1) / 2 <= displayManager.width() - 2) {
      target->setTextColor(UI_WHITE);
      target->setCursor(x - (int)w / 2, originY + y - 10);
      target->print(tempText);
    }

    String timeLabel = forecastCompactTime(index);
    target->getTextBounds(timeLabel.c_str(), 0, 0, &x1, &y1, &w, &h);
    if (x - (int)w / 2 >= 2 &&
        x + ((int)w + 1) / 2 <= displayManager.width() - 2) {
      target->setTextColor(MUTED);
      target->setCursor(
        x - (int)w / 2,
        originY + 99 + FORECAST_GRAPH_VERTICAL_SHIFT);
      target->print(timeLabel);
    }

    const int rainPercent = constrain(
      (int)round(forecast[index].rainProbability * 100.0f), 0, 100);
    const int barHeight = (rainPercent * 18) / 100;
    if (x >= 3 && x <= displayManager.width() - 3) {
      target->fillRect(
        x - 3, originY + 139 - barHeight, 6, max(1, barHeight), ACCENT);
    }
    String rainLabel = String(rainPercent) + "%";
    target->getTextBounds(rainLabel.c_str(), 0, 0, &x1, &y1, &w, &h);
    if (x - (int)w / 2 >= 2 &&
        x + ((int)w + 1) / 2 <= displayManager.width() - 2) {
      target->setTextColor(UI_WHITE);
      target->setCursor(x - (int)w / 2, originY + 143);
      target->print(rainLabel);
    }

    previousX = x;
    previousY = y;
    hasPrevious = true;
  }
}

// Copy only the graph body from the existing canvas. The 8-pixel source crop
// starts at screen y=72, below the selector, so controls never need repainting.
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
      drawForecastGraph(displayManager.forecastCanvas(), 0);
    }
    if (flushForecastGraphCanvasBody()) return;
    displayManager.forecastCanvas()->flush();
    drawForecastModeSelector();
    return;
  }

  // Safe no-canvas fallback. The normal hardware path uses the buffered branch.
  const int graphRedrawTop = FORECAST_CONTROL_BOTTOM + 2;
  if (graphRedrawTop >= FORECAST_VIEWPORT_BOTTOM) return;
  displayManager.target()->fillRect(
    0, graphRedrawTop, displayManager.width(),
    FORECAST_VIEWPORT_BOTTOM - graphRedrawTop, BG);
  if (forecastAvailable && forecastCount > 0) {
    drawForecastGraph(displayManager.target(), FORECAST_VIEWPORT_TOP);
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
  forecastOffset = 0;
  forecastGraphOffset = 0;
  forecastGraphScrollPixels = 0;
  forecastGraphDragCurrentScroll = 0;
  forecastGraphView = control == 4;

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
  const int maxOffset = max(0, forecastCount - ITEMS_PER_PAGE);
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
      drawForecastModeSelector();
      return;
    }

    const int firstIndex = max(0, baseOffset - 1);
    const int lastIndex = min(forecastCount, baseOffset + ITEMS_PER_PAGE + 1);
    for (int index = firstIndex; index < lastIndex; index++) {
      if (index < 0 || index >= forecastCount || index >= MAX_FORECAST) {
        continue;
      }

      const int y = FORECAST_ROW_TOP +
                    (index - baseOffset) * FORECAST_ROW_STEP -
                    scrollPixels;

      // Keep partially visible edge cards in the render so they slide
      // through the viewport instead of popping in/out during a drag.
      if (y + FORECAST_CARD_HEIGHT <= FORECAST_VIEWPORT_TOP ||
          y >= FORECAST_VIEWPORT_BOTTOM) {
        continue;
      }

      drawForecastItemClipped(displayManager.forecastCanvas(), index, y);
    }

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

  int visible = min(ITEMS_PER_PAGE, max(0, forecastCount - baseOffset));
  for (int i = 0; i < visible; i++) {
    int y = FORECAST_ROW_TOP + i * FORECAST_ROW_STEP;
    drawForecastItem(displayManager.target(), baseOffset + i, y);
  }
}

void drawForecast() {
  currentTab = 1;
  displayManager.target()->fillScreen(BG);
  drawHeader();

  if (!forecastAvailable || forecastCount == 0) {
    drawCenteredText(forecastError.length() ? forecastError : "No hourly data", 100, 1, MUTED);
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
  weatherMetricOffset = 0;
  forecastOffset = 0;
  forecastGraphOffset = 0;
  forecastGraphScrollPixels = 0;

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
  const int maxOffset = max(0, forecastCount - ITEMS_PER_PAGE);
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
  return max(0, forecastCount - ITEMS_PER_PAGE) * FORECAST_ROW_STEP;
}

void drawForecastAtScrollPosition(int pixels) {
  const int maxScroll = maxForecastScrollPixels();
  const int maxOffset = max(0, forecastCount - ITEMS_PER_PAGE);
  pixels = ScrollController::clampElasticPosition(
    pixels, maxScroll, SCROLL_OVERSCROLL_MAX_PX);

  // The overscroll cap is below one row, so negative positions remain in row
  // zero and positions beyond the bottom remain in the last valid row.
  int baseOffset = pixels < 0 ? 0 : pixels / FORECAST_ROW_STEP;
  int scrollPixels = pixels < 0
    ? pixels
    : pixels % FORECAST_ROW_STEP;

  if (baseOffset > maxOffset) {
    baseOffset = maxOffset;
    scrollPixels = pixels - maxScroll;
  }

  forecastOffset = baseOffset;
  drawForecastViewport(baseOffset, scrollPixels);
}

void animateForecastSnap(int startPixels, int targetOffset) {
  const int maxOffset = max(0, forecastCount - ITEMS_PER_PAGE);
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
      int maxOffset = max(
        0,
        WEATHER_METRIC_COUNT - WEATHER_METRICS_VISIBLE);
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
    int maxOffset = max(0, forecastCount - ITEMS_PER_PAGE);
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

void pollTouch() {
  if (!touchReader.available()) return;

  TouchSample sample;
  if (!touchReader.readSample(sample)) return;
  const GestureObservation observation = gestureRecognizer.update(sample);
  const GestureCode gesture = observation.gesture;

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

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    maybeSyncTimeFromAPI();
    return true;
  }
  drawLoading("Connecting Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPassword);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000UL) {
    delay(500);
    DEBUG_PRINT(".");
  }
  DEBUG_PRINTLN();

  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINTLN("Wi-Fi connected");
    DEBUG_PRINTLN(WiFi.localIP());
    maybeSyncTimeFromAPI();
    return true;
  }

  DEBUG_PRINTLN("Wi-Fi connection timed out");
  return false;
}

// =====================================================
// FETCH CURRENT WEATHER
// =====================================================

// Location provider orchestration and reverse geocoding live in LocationService.h.
bool fetchWeather() {
#if USE_DEMO_WEATHER_DATA
  return loadDemoWeatherData();
#endif
  if (!connectWiFi()) {
    if (weatherAvailable) drawWeather();
    else drawError("Wi-Fi unavailable");
    return false;
  }

  drawLoading("Fetching weather...");
  String url =
    "https://api.openweathermap.org/data/2.5/weather?lat=" +
    String(locationLat, 4) + "&lon=" + String(locationLon, 4) +
    "&appid=" + String(apiKey) + "&units=metric";

  HttpRequestClient http;
  if (!http.begin(
        url, 15000, 15000,
        HttpRequestClient::TlsPolicy::InsecureCompatibility)) {
    if (weatherAvailable) drawWeather();
    else drawError("HTTP init failed");
    return false;
  }

  int code = http.get();

  if (code != HTTP_CODE_OK) {
    DEBUG_PRINTF("Weather HTTP status: %d\n", code);
    http.end();
    if (weatherAvailable) drawWeather();
    else if (code == 401) drawError("Check API key");
    else if (code == 404) drawError("City not found");
    else drawError("Request failed");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();

  if (error) {
    DEBUG_PRINTF("Weather JSON error: %s\n", error.c_str());
    if (weatherAvailable) drawWeather();
    else drawError("JSON parse failed");
    return false;
  }

  weatherCity = doc["name"] | "Unknown";
  weatherDescription = doc["weather"][0]["description"] | "Unknown";
  weatherMain = doc["weather"][0]["main"] | "Unknown";
  weatherIcon = doc["weather"][0]["icon"] | "03d";
  // Keep coordinates supplied by the positioning lookup. OpenWeather can
  // return coordinates for a nearby reporting location instead.
  if (!locationCoordinatesResolved) {
    locationLat = doc["coord"]["lat"] | locationLat;
    locationLon = doc["coord"]["lon"] | locationLon;
  }
  locationTimezoneOffset = doc["timezone"] | locationTimezoneOffset;
  temperature = doc["main"]["temp"] | 0.0f;
  feelsLike = doc["main"]["feels_like"] | 0.0f;
  humidity = doc["main"]["humidity"] | 0;
  pressure = doc["main"]["pressure"] | 0;
  windSpeedKmh = (doc["wind"]["speed"] | 0.0f) * 3.6f;
  cloudCover = doc["clouds"]["all"] | 0;
  visibilityAvailable = !doc["visibility"].isNull();
  visibilityMeters = doc["visibility"] | 0;
  rainLastHourMm = doc["rain"]["1h"] | 0.0f;
  weatherAvailable = true;

  DEBUG_PRINTF(
    "%s: %.1f C, %s, humidity %d%%, pressure %d hPa\n",
    weatherCity.c_str(), temperature, weatherDescription.c_str(), humidity, pressure);
  return true;
}

// =====================================================
// FETCH ONE CALL 4.0 HOURLY FORECAST
// =====================================================

String formatForecastTime(uint32_t timestamp, long timezoneOffset) {
  time_t localTimestamp = (time_t)timestamp + (time_t)timezoneOffset;
  struct tm localTime;
  gmtime_r(&localTimestamp, &localTime);

  char clockText[8];
  snprintf(clockText, sizeof(clockText), "%02d:%02d", localTime.tm_hour, localTime.tm_min);

  if (forecastCount > 0) {
    time_t firstTimestamp = (time_t)forecast[0].timestamp + (time_t)timezoneOffset;
    struct tm firstTime;
    gmtime_r(&firstTimestamp, &firstTime);

    if (localTime.tm_yday != firstTime.tm_yday || localTime.tm_year != firstTime.tm_year) {
      const char* dayNames[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
      return String(dayNames[localTime.tm_wday]) + " " + String(clockText);
    }
  }

  return String(clockText);
}

// Deterministic UI fixture for feature development. This intentionally fills
// every metric and several forecast states without connecting to Wi-Fi.
bool loadDemoWeatherData() {
  weatherCity = "Manchester";
  weatherDescription = "scattered clouds";
  weatherMain = "Clouds";
  weatherIcon = "03d";
  locationLat = 53.4808f;
  locationLon = -2.2426f;
  locationTimezoneOffset = 0;
  temperature = 14.6f;
  feelsLike = 13.8f;
  humidity = 68;
  pressure = 1014;
  windSpeedKmh = 16.2f;
  cloudCover = 40;
  visibilityMeters = 10000;
  rainLastHourMm = 0.2f;
  currentUvIndex = 1.4f;
  currentRainProbability = 0.25f;
  visibilityAvailable = true;
  uvIndexAvailable = true;
  rainProbabilityAvailable = true;
  weatherAvailable = true;
  forecastError = "";
  forecastCount = 24;
  forecastOffset = min(forecastOffset, max(0, forecastCount - ITEMS_PER_PAGE));

  const char* descriptions[] = {
    "Clouds", "Clouds", "Rain", "Clouds", "Clear", "Clear",
    "Clouds", "Rain", "Clouds", "Clear", "Clouds", "Clouds"
  };
  const float temperatures[] = {
    14.6f, 14.1f, 13.8f, 13.2f, 12.9f, 12.5f,
    12.1f, 11.8f, 11.6f, 11.9f, 12.4f, 13.0f
  };
  const float rainProbabilities[] = {
    0.05f, 0.10f, 0.35f, 0.55f, 0.18f, 0.05f,
    0.03f, 0.72f, 0.48f, 0.12f, 0.06f, 0.02f
  };
  const time_t now = time(nullptr);
  for (int i = 0; i < forecastCount; i++) {
    int sample = i % 12;
    forecast[i].timestamp = (uint32_t)(now + (time_t)i * 3600);
    forecast[i].timeText = formatForecastTime(
      forecast[i].timestamp, locationTimezoneOffset);
    forecast[i].description = descriptions[sample];
    forecast[i].temp = temperatures[sample];
    forecast[i].feelsLike = temperatures[sample] - (sample % 3 == 0 ? 1.0f : 0.0f);
    forecast[i].rainProbability = rainProbabilities[sample];
  }
  forecastGraphOffset = constrain(
    forecastGraphOffset, 0, max(0, forecastCount - FORECAST_GRAPH_POINTS));
  forecastGraphScrollPixels =
    forecastGraphOffset * FORECAST_GRAPH_POINT_SPACING;
  forecastAvailable = true;
  DEBUG_PRINTLN("Demo mode: loaded local weather and 24 sample forecast entries; no API calls.");
  return true;
}

bool fetchOneCallForecastPage(const String& url, String& nextUrl) {
  HttpRequestClient http;
  if (!http.begin(
        url, 15000, 15000,
        HttpRequestClient::TlsPolicy::InsecureCompatibility)) {
    forecastError = "Hourly API unavailable";
    DEBUG_PRINTLN("One Call HTTP initialisation failed");
    return false;
  }

  int code = http.get();

  if (code != HTTP_CODE_OK) {
    // Capture the response body before closing HTTP. OpenWeather normally
    // includes a JSON message for rejected keys, subscriptions, and limits.
    String errorBody = http.getString();
    DEBUG_PRINTF("One Call hourly HTTP status: %d\n", code);
    if (errorBody.length() > 0) {
      if (errorBody.length() > 240) errorBody = errorBody.substring(0, 240);
      DEBUG_PRINTF("One Call error response: %s\n", errorBody.c_str());
    }
    http.end();

    if (code == 401 || code == 403) {
      forecastError = "Check One Call access";
    } else if (code == 429) {
      forecastError = "One Call rate limit";
    } else if (code < 0) {
      forecastError = "One Call network error";
    } else {
      forecastError = "One Call HTTP " + String(code);
    }
    return false;
  }
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();

  if (error) {
    DEBUG_PRINTF("One Call hourly JSON error: %s\n", error.c_str());
    forecastError = "Hourly data parse failed";
    return false;
  }

  locationTimezoneOffset = doc["timezone_offset"] | locationTimezoneOffset;
  nextUrl = doc["next"] | "";

  JsonArray list = doc["data"].as<JsonArray>();
  if (list.isNull()) {
    DEBUG_PRINTLN("One Call response did not contain a data array.");
    serializeJson(doc, Serial);
    DEBUG_PRINTLN();
    forecastError = "Unexpected One Call data";
    return false;
  }
  DEBUG_PRINTF("One Call page contains %u record(s).\n", (unsigned)list.size());
  for (JsonObject item : list) {
    if (forecastCount >= MAX_FORECAST) break;

    uint32_t timestamp = item["dt"] | 0UL;
    if (timestamp == 0) continue;

    bool duplicate = false;
    for (int i = 0; i < forecastCount; i++) {
      if (forecast[i].timestamp == timestamp) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;

    const char* desc = item["weather"][0]["main"] | "Unknown";
    float temp = item["temp"] | 0.0f;
    float feelsLikeValue = item["feels_like"] | temp;
    float precipitationProbability = item["pop"] | 0.0f;
    precipitationProbability = constrain(
      precipitationProbability, 0.0f, 1.0f);

    // The first valid hourly record is the nearest available estimate for
    // current UV index and precipitation probability.
    if (forecastCount == 0) {
      if (!item["uvi"].isNull()) {
        currentUvIndex = item["uvi"].as<float>();
        uvIndexAvailable = true;
      }
      if (!item["pop"].isNull()) {
        currentRainProbability = item["pop"].as<float>();
        rainProbabilityAvailable = true;
      }
    }

    forecast[forecastCount].timestamp = timestamp;
    forecast[forecastCount].timeText = formatForecastTime(timestamp, locationTimezoneOffset);
    forecast[forecastCount].description = String(desc);
    forecast[forecastCount].temp = temp;
    forecast[forecastCount].feelsLike = feelsLikeValue;
    forecast[forecastCount].rainProbability = precipitationProbability;
    forecastCount++;
  }

  return true;
}

bool fetchForecast() {
#if USE_DEMO_WEATHER_DATA
  return loadDemoWeatherData();
#endif
  if (!connectWiFi()) {
    forecastAvailable = false;
    forecastError = "Wi-Fi unavailable";
    return false;
  }

  forecastCount = 0;
  forecastError = "";
  currentUvIndex = 0.0f;
  currentRainProbability = 0.0f;
  uvIndexAvailable = false;
  rainProbabilityAvailable = false;

  String nextUrl =
    "https://api.openweathermap.org/data/4.0/onecall/timeline/1h?lat=" + String(locationLat, 4) + "&lon=" + String(locationLon, 4) + "&units=metric&appid=" + String(apiKey);

  // The 1-hour endpoint returns up to 20 entries per request. Follow its
  // prepared pagination URL until we have up to 48 hours or no next page.
  int pageCount = 0;
  const int MAX_HOURLY_PAGES = 3;

  while (nextUrl.length() > 0 && forecastCount < MAX_FORECAST && pageCount < MAX_HOURLY_PAGES) {
    String pageUrl = nextUrl;
    String pageNext = "";
    if (!fetchOneCallForecastPage(pageUrl, pageNext)) {
      if (forecastCount == 0) {
        forecastAvailable = false;
        DEBUG_PRINTLN("Hourly forecast unavailable. Check One Call API 4.0 subscription and API key.");
        return false;
      }
      break;
    }

    nextUrl = pageNext;
    pageCount++;
  }

  forecastAvailable = forecastCount > 0;
  forecastGraphOffset = constrain(
    forecastGraphOffset, 0, max(0, forecastCount - FORECAST_GRAPH_POINTS));
  forecastGraphScrollPixels =
    forecastGraphOffset * FORECAST_GRAPH_POINT_SPACING;
  forecastOffset = constrain(
    forecastOffset, 0, max(0, forecastCount - ITEMS_PER_PAGE));
  if (!forecastAvailable && forecastError.length() == 0) {
    forecastError = "No hourly records";
    DEBUG_PRINTLN("One Call returned without any parsable hourly records.");
  }

  DEBUG_PRINTF("Hourly forecast records received: %d across %d page(s)\n", forecastCount, pageCount);
  return forecastAvailable;
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(500);
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
#else
  drawLoading("Starting up...");
  if (connectWiFi()) {
    const char* googleGeolocationApiKey = nullptr;
#if defined(GOOGLE_GEOLOCATION_API_KEY)
    googleGeolocationApiKey = GOOGLE_GEOLOCATION_API_KEY;
#endif

    // LocationService preserves Google Wi-Fi -> IP -> configured fallback.
    const LocationService::Result locationResult = locationService.resolve(
      WiFi.status() == WL_CONNECTED,
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
  }
#endif
  fetchWeather();
  fetchForecast();

  if (weatherAvailable) drawWeather();
  else drawError("Weather unavailable");

  lastUpdate = millis();
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  pollTouch();
  maybeSyncTimeFromAPI();

  if (millis() - lastClockCheckMs >= 1000UL) {
    lastClockCheckMs = millis();
    refreshHeaderClock();
  }

  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = millis();
    DEBUG_PRINTLN("Refreshing weather and forecast...");
    bool currentOK = fetchWeather();
    fetchForecast();

    if (currentTab == 1) drawForecast();
    else if (currentOK || weatherAvailable) drawWeather();
    else drawError("Refresh failed");
  }

  delay(30);
}