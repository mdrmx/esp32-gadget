#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <PNGdec.h>
#include <time.h>
#include <sys/time.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <stdlib.h>
#include "secrets.h"

// =====================================================
// CONFIGURATION
// =====================================================

// Development switch: 1 uses deterministic local sample data and makes NO
// weather, time, or icon HTTP requests. Set to 0 for live API-backed weather.
#define USE_DEMO_WEATHER_DATA 1

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
const unsigned long UPDATE_INTERVAL = 600000UL;

// =====================================================
// CLOCK API CONFIGURATION
// =====================================================

// Fetch Europe/London local date/time from the HTTP API, convert it using
// the same UK timezone/DST rules, then set the ESP32 system clock.
const char* TIME_ZONE_INFO = "GMT0BST,M3.5.0/1,M10.5.0/2";
const char* TIME_API_URL =
  "https://timeapi.io/api/Time/current/zone?timeZone=Europe%2FLondon";
const time_t MIN_VALID_EPOCH = 1704067200; // 2024-01-01

bool timeApiValid = false;
unsigned long lastTimeApiAttemptMs = 0;
unsigned long lastTimeApiSyncMs = 0;
unsigned long lastClockCheckMs = 0;
String lastHeaderClock = "";

const unsigned long TIME_API_RETRY_INTERVAL = 5UL * 60UL * 1000UL;
const unsigned long TIME_API_SYNC_INTERVAL = 6UL * 60UL * 60UL * 1000UL;

// =====================================================
// DISPLAY
// =====================================================

#define LCD_SCLK 39
#define LCD_MOSI 38
#define LCD_MISO 40
#define LCD_DC 42
#define LCD_RST -1
#define LCD_CS 45
#define LCD_BL 1

#define LCD_ROTATION 1
#define LCD_WIDTH 240
#define LCD_HEIGHT 320

Arduino_DataBus* bus = new Arduino_ESP32SPI(
  LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO);

Arduino_GFX* gfx = new Arduino_ST7789(
  bus, LCD_RST, LCD_ROTATION, true, LCD_WIDTH, LCD_HEIGHT);

// Buffered forecast viewport used to render each scroll frame off-screen,
// then send it to the ST7789 as one bitmap transfer.
Arduino_Canvas* forecastCanvas = nullptr;
Arduino_Canvas* weatherMetricsCanvas = nullptr;
Arduino_Canvas* tabTransitionCanvas = nullptr;
bool renderingTabTransition = false;

int screenW = 320;
int screenH = 240;

// =====================================================
// TOUCH
// =====================================================

#define TOUCH_SDA 48
#define TOUCH_SCL 47
#define TOUCH_ADDR 0x15
#define REG_GESTURE 0x01

// Values are kept as configurable raw codes. The diagnostic logs below
// report each transition so physical swipe directions can be verified.
#define GESTURE_SCREEN_LEFT 0x01
#define GESTURE_SCREEN_RIGHT 0x02
#define GESTURE_SCREEN_DOWN 0x03
#define GESTURE_SCREEN_UP 0x04

bool touchAvailable = false;
uint8_t lastHandledGesture = 0;
uint8_t previousRawGesture = 0;
unsigned long lastGestureTime = 0;

struct TouchSample {
  uint8_t gesture;
  uint8_t fingerCount;
  uint8_t event;
  uint16_t rawX;
  uint16_t rawY;
};

bool metricsTouchDragActive = false;
bool metricsTouchDragMoved = false;
int metricsDragStartY = 0;
int metricsDragStartScroll = 0;
int metricsDragCurrentScroll = 0;
bool suppressMetricGesture = false;
unsigned long metricsGestureSuppressionStartedMs = 0;

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
const int WEATHER_METRIC_PANEL_HEIGHT = 152;
const int WEATHER_METRIC_PANEL_BOTTOM =
  WEATHER_METRIC_PANEL_TOP + WEATHER_METRIC_PANEL_HEIGHT;
const int WEATHER_METRIC_CARD_HEIGHT = 46;
const int WEATHER_METRIC_ROW_STEP = 50;
const int WEATHER_METRIC_ROW_TOP = 2;
int weatherMetricOffset = 0;

// Keep the title above and the range/navigation footer below the scrolling
// viewport. Three taller cards use the available height evenly.
const int FORECAST_VIEWPORT_TOP = 60;
const int FORECAST_VIEWPORT_HEIGHT = 147;
const int FORECAST_VIEWPORT_BOTTOM =
  FORECAST_VIEWPORT_TOP + FORECAST_VIEWPORT_HEIGHT;

// Keep the shared header and footer fixed while animating only the main body.
const int TAB_TRANSITION_CONTENT_TOP = 37;
const int TAB_TRANSITION_FOOTER_HEIGHT = 29;

const int FORECAST_ROW_TOP = FORECAST_VIEWPORT_TOP + 3;
const int FORECAST_ROW_STEP = 49;
const int FORECAST_CARD_HEIGHT = 43;

struct ForecastItem {
  String timeText;
  String description;
  float temp;
  uint32_t timestamp;
};

ForecastItem forecast[MAX_FORECAST];
int forecastCount = 0;
int forecastOffset = 0;
int currentTab = 0;

// =====================================================
// TEXT HELPERS
// =====================================================

void drawText(const String& text, int x, int y, uint8_t size, uint16_t colour) {
  gfx->setTextSize(size);
  gfx->setTextColor(colour);
  gfx->setCursor(x, y);
  gfx->print(text);
}

void drawCenteredText(const String& text, int y, uint8_t size, uint16_t colour) {
  gfx->setTextSize(size);
  gfx->setTextColor(colour);

  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(text.c_str(), 0, y, &x1, &y1, &w, &h);

  int x = (screenW - (int)w) / 2;
  if (x < 4) x = 4;
  if (x + (int)w > screenW - 4) x = screenW - 4 - w;

  gfx->setCursor(x, y);
  gfx->print(text);
}

int measureTextWidth(const String& text, uint8_t size) {
  gfx->setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  return (int)w;
}

int measureTextHeight(const String& text, uint8_t size) {
  gfx->setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
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

  gfx->setTextSize(size);
  gfx->setTextColor(colour);
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
  gfx->setTextSize(size);
  int16_t x1, y1;
  uint16_t width, height;
  gfx->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &width, &height);
  drawText(text, centreX - (int)width / 2, y, size, colour);
  return height > 0 ? (int)height : 8 * size;
}

// =====================================================
// CLOCK HELPERS
// =====================================================

bool getCurrentUtcEpoch(time_t& epoch) {
  // After the first successful API sync, the ESP32 system clock advances
  // locally. No repeated time API requests are needed to draw the header.
  time_t systemEpoch = time(nullptr);
  if (systemEpoch >= MIN_VALID_EPOCH) {
    epoch = systemEpoch;
    return true;
  }
  return false;
}

String getHeaderClockText() {
  time_t epoch = 0;
  if (!getCurrentUtcEpoch(epoch)) {
    return "--:--";
  }

  struct tm localTime;
  if (localtime_r(&epoch, &localTime) == nullptr) {
    return "--:--";
  }

  char clockText[6];
  snprintf(clockText, sizeof(clockText), "%02d:%02d",
           localTime.tm_hour, localTime.tm_min);
  return String(clockText);
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
  String label = weatherCity;

  // Before the current-weather response arrives, use the configured city.
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
  gfx->fillRect(screenW - 78, 0, 78, 36, PANEL);

  gfx->setTextSize(2);
  gfx->setTextColor(ACCENT);
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(clockText.c_str(), 0, 10, &x1, &y1, &w, &h);
  int x = screenW - 10 - (int)w;
  if (x < screenW - 70) {
    x = screenW - 70;
  }
  drawText(clockText, x, 10, 2, ACCENT);
  lastHeaderClock = clockText;
}

void refreshHeaderClock() {
  String clockText = getHeaderClockText();
  if (clockText != lastHeaderClock) {
    drawHeaderClockOnly(clockText);
  }
}

bool fetchTimeFromAPI() {
#if USE_DEMO_WEATHER_DATA
  DEBUG_PRINTLN("Demo mode: skipping TimeAPI.io request.");
  return false;
#endif
  lastTimeApiAttemptMs = millis();

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("Time API skipped: Wi-Fi is not connected.");
    return false;
  }

  DEBUG_PRINTLN("Requesting current date/time from TimeAPI.io...");

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (!http.begin(client, TIME_API_URL)) {
    DEBUG_PRINTLN("Could not initialise time API request.");
    return false;
  }

  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  int status = http.GET();

  if (status != HTTP_CODE_OK) {
    DEBUG_PRINTF("Time API HTTP status: %d\n", status);
    String errorBody = http.getString();
    if (errorBody.length() > 0) {
      if (errorBody.length() > 180) {
        errorBody = errorBody.substring(0, 180);
      }
      DEBUG_PRINTF("Time API response: %s\n", errorBody.c_str());
    }
    http.end();
    return false;
  }

  // Read the complete response first. Some HTTP responses arrive chunked;
  // parsing the raw stream directly can fail before the JSON body is consumed.
  String responseBody = http.getString();
  http.end();

  DEBUG_PRINTF("Time API response length: %u bytes\n", (unsigned)responseBody.length());

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, responseBody);

  if (error) {
    DEBUG_PRINTF("Time API JSON error: %s\n", error.c_str());
    String preview = responseBody;
    if (preview.length() > 240) {
      preview = preview.substring(0, 240);
    }
    DEBUG_PRINTF("Time API body preview: [%s]\n", preview.c_str());
    return false;
  }

  int year = doc["year"] | 0;
  int month = doc["month"] | 0;
  int day = doc["day"] | 0;
  int hour = doc["hour"] | -1;
  int minute = doc["minute"] | -1;
  int second = doc["seconds"] | (doc["second"] | 0);

  if (year < 2024 || year > 2099 ||
      month < 1 || month > 12 ||
      day < 1 || day > 31 ||
      hour < 0 || hour > 23 ||
      minute < 0 || minute > 59 ||
      second < 0 || second > 60) {
    DEBUG_PRINTLN("Time API response contained invalid date/time fields.");
    serializeJson(doc, Serial);
    DEBUG_PRINTLN();
    return false;
  }

  // The API returns Europe/London wall-clock fields. mktime() converts
  // them to Unix UTC epoch using the UK timezone and DST rules configured
  // during setup. This avoids assuming the UK is always UTC+0 or UTC+1.
  struct tm localTime = {};
  localTime.tm_year = year - 1900;
  localTime.tm_mon = month - 1;
  localTime.tm_mday = day;
  localTime.tm_hour = hour;
  localTime.tm_min = minute;
  localTime.tm_sec = second;
  localTime.tm_isdst = -1;

  time_t epoch = mktime(&localTime);
  if (epoch < MIN_VALID_EPOCH) {
    DEBUG_PRINTLN("Time API date could not be converted to a valid epoch.");
    return false;
  }

  struct timeval newSystemTime;
  newSystemTime.tv_sec = epoch;
  newSystemTime.tv_usec = 0;

  if (settimeofday(&newSystemTime, nullptr) != 0) {
    DEBUG_PRINTLN("Failed to set ESP32 system time from TimeAPI.io.");
    return false;
  }

  timeApiValid = true;
  lastTimeApiSyncMs = millis();
  lastHeaderClock = "";

  DEBUG_PRINTF("System clock set from API: %04d-%02d-%02d %02d:%02d:%02d Europe/London\n",
                year, month, day, hour, minute, second);
  return true;
}

void maybeSyncTimeFromAPI() {
#if USE_DEMO_WEATHER_DATA
  return;
#endif
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long nowMs = millis();
  bool syncDue = false;

  if (timeApiValid) {
    syncDue = nowMs - lastTimeApiSyncMs >= TIME_API_SYNC_INTERVAL;
  } else {
    syncDue = lastTimeApiAttemptMs == 0 ||
              nowMs - lastTimeApiAttemptMs >= TIME_API_RETRY_INTERVAL;
  }

  // If a periodic refresh has failed, retry no more than once per five minutes.
  if (syncDue &&
      (lastTimeApiAttemptMs == 0 ||
       nowMs - lastTimeApiAttemptMs >= TIME_API_RETRY_INTERVAL)) {
    fetchTimeFromAPI();
  }
}

void initialiseTimeZone() {
  setenv("TZ", TIME_ZONE_INFO, 1);
  tzset();
}

// =====================================================

// =====================================================
// HEADER
// =====================================================

void drawHeader() {
  gfx->fillRect(0, 0, screenW, 36, PANEL);

  String locationLabel = getHeaderLocationText();
  drawText(locationLabel, 10, 10, 2, UI_WHITE);

  String clockText = getHeaderClockText();
  gfx->setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(clockText.c_str(), 0, 10, &x1, &y1, &w, &h);
  int x = screenW - 10 - (int)w;
  if (x < screenW - 70) {
    x = screenW - 70;
  }
  drawText(clockText, x, 10, 2, ACCENT);
  lastHeaderClock = clockText;

  gfx->drawFastHLine(0, 36, screenW, ACCENT);
}

void drawTabIndicator() {
  int y = screenH - 8;
  gfx->fillRoundRect(screenW / 2 - 24, y, 48, 4, 2, PANEL);
  gfx->fillRoundRect(screenW / 2 - 24 + (currentTab * 24), y, 24, 4, 2, UI_YELLOW);
}

// =====================================================
// LOADING AND ERROR SCREENS
// =====================================================

void drawLoading(const String& message) {
  gfx->fillScreen(BG);
  drawHeader();
  drawCenteredText("OpenWeatherMap", 83, 2, ACCENT);
  drawCenteredText(message, 119, 1, UI_WHITE);
}

void drawError(const String& message) {
  gfx->fillScreen(BG);
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

  gfx->draw16bitRGBBitmap(
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

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) {
    DEBUG_PRINTLN("Could not initialise OpenWeather icon request");
    return false;
  }

  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  int status = http.GET();

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
  return (screenW * 60) / 100;
}

int weatherMetricsPanelWidth() {
  return screenW - weatherMetricsPanelX() - 10;
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

void drawWeatherMetricItem(
  Arduino_GFX* target,
  int index,
  int x,
  int y,
  int panelWidth
) {
  target->fillRoundRect(
    x + 2,
    y,
    panelWidth - 4,
    WEATHER_METRIC_CARD_HEIGHT,
    6,
    PANEL);

  String label = weatherMetricLabel(index);
  target->setTextSize(1);
  target->setTextColor(MUTED);
  target->setCursor(x + 9, y + 5);
  target->print(label);

  String value = weatherMetricValue(index);
  uint8_t valueSize = value.length() <= 6 ? 2 : 1;
  target->setTextSize(valueSize);
  target->setTextColor(ACCENT);
  int16_t x1, y1;
  uint16_t w, h;
  target->getTextBounds(value.c_str(), 0, 0, &x1, &y1, &w, &h);
  int valueX = x + (panelWidth - (int)w) / 2;
  int valueY = y + (valueSize == 2 ? 20 : 25);
  target->setCursor(valueX, valueY);
  target->print(value);
}

// Render the metrics panel into a small off-screen buffer, then transfer the
// complete panel in one bitmap operation. This avoids clearing/repainting
// individual cards directly to the LCD for each animation frame.
void drawWeatherMetricsViewport(int baseOffset, int scrollPixels) {
  int panelWidth = weatherMetricsPanelWidth();

  if (weatherMetricsCanvas != nullptr && !renderingTabTransition) {
    weatherMetricsCanvas->fillScreen(BG);

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

      drawWeatherMetricItem(weatherMetricsCanvas, index, 0, y, panelWidth);
    }

    weatherMetricsCanvas->flush();
    return;
  }

  // Direct-render fallback: honour fractional card offsets when PSRAM buffer
  // allocation fails. Cards are drawn only when fully inside the viewport.
  gfx->fillRect(
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
    if (localY < 0 ||
        localY + WEATHER_METRIC_CARD_HEIGHT > WEATHER_METRIC_PANEL_HEIGHT) {
      continue;
    }

    drawWeatherMetricItem(
      gfx,
      index,
      weatherMetricsPanelX(),
      WEATHER_METRIC_PANEL_TOP + localY,
      panelWidth);
  }
}

void drawWeatherMetricsCounter() {
  int firstItem = weatherMetricOffset + 1;
  int lastItem = min(
    weatherMetricOffset + WEATHER_METRICS_VISIBLE,
    WEATHER_METRIC_COUNT);
  String rangeText = String(firstItem) + "-" + String(lastItem) +
                     "/" + String(WEATHER_METRIC_COUNT);
  int x = weatherMetricsPanelX() + 5;
  int y = WEATHER_METRIC_PANEL_BOTTOM + 2;
  gfx->fillRect(x, y, 50, 8, BG);
  drawText(
    rangeText,
    x,
    y,
    1,
    UI_YELLOW);
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

int clampWeatherMetricScrollPixels(int pixels) {
  return constrain(pixels, 0, maxWeatherMetricScrollPixels());
}

void drawWeatherMetricsAtScrollPosition(int pixels) {
  pixels = clampWeatherMetricScrollPixels(pixels);

  int baseOffset = pixels / WEATHER_METRIC_ROW_STEP;
  int scrollPixels = pixels % WEATHER_METRIC_ROW_STEP;
  int maxOffset = max(
    0,
    WEATHER_METRIC_COUNT - WEATHER_METRICS_VISIBLE);

  if (baseOffset > maxOffset) {
    baseOffset = maxOffset;
    scrollPixels = 0;
  }
  drawWeatherMetricsViewport(baseOffset, scrollPixels);
}

void animateWeatherMetricsSnap(int startPixels, int targetOffset) {
  const int maxOffset = max(
    0,
    WEATHER_METRIC_COUNT - WEATHER_METRICS_VISIBLE);
  targetOffset = constrain(targetOffset, 0, maxOffset);
  int targetPixels = targetOffset * WEATHER_METRIC_ROW_STEP;

  if (startPixels != targetPixels && weatherMetricsCanvas != nullptr) {
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
  drawWeatherMetricsCounter();
}

void animateWeatherMetricsScroll(int direction) {
  int targetOffset = weatherMetricOffset + direction;
  int maxOffset = max(
    0,
    WEATHER_METRIC_COUNT - WEATHER_METRICS_VISIBLE);

  if (targetOffset < 0 || targetOffset > maxOffset) return;

  // If memory is too constrained for the buffer, update the page in one
  // redraw rather than using a multi-frame direct-render animation.
  if (weatherMetricsCanvas == nullptr) {
    weatherMetricOffset = targetOffset;
    drawWeatherMetricsPanel(weatherMetricOffset, 0);
    drawWeatherMetricsCounter();
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
  drawWeatherMetricsCounter();
}

void drawWeather() {
  currentTab = 0;
  gfx->fillScreen(BG);
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
  drawWeatherMetricsCounter();

  gfx->drawFastHLine(10, screenH - 29, screenW - 20, PANEL);
  drawTabIndicator();
}

// =====================================================
// FORECAST SCREEN
// =====================================================

void drawForecastItem(Arduino_GFX* target, int index, int y) {
  target->fillRoundRect(8, y, screenW - 16, FORECAST_CARD_HEIGHT, 6, PANEL);

  target->setTextSize(1);
  target->setTextColor(UI_WHITE);
  target->setCursor(16, y + (FORECAST_CARD_HEIGHT - 8) / 2);
  target->print(forecast[index].timeText);

  String desc = forecast[index].description;
  if (desc.length() > 12) desc = desc.substring(0, 11) + ".";
  target->setTextColor(MUTED);
  target->setCursor(90, y + (FORECAST_CARD_HEIGHT - 8) / 2);
  target->print(desc);

  String temp = String(forecast[index].temp, 1) + "C";
  target->setTextColor(UI_YELLOW);
  int16_t x1, y1;
  uint16_t w, h;
  target->getTextBounds(temp.c_str(), 0, 0, &x1, &y1, &w, &h);
  target->setCursor(screenW - 16 - w, y + (FORECAST_CARD_HEIGHT - 8) / 2);
  target->print(temp);
}

void drawForecastFooter() {
  int firstItem = forecastCount > 0 ? forecastOffset + 1 : 0;
  int lastItem = min(forecastOffset + ITEMS_PER_PAGE, forecastCount);
  String rangeText = String(firstItem) + "-" + String(lastItem) + "/" + String(forecastCount);

  drawText(rangeText, screenW - 58, screenH - 25, 1, UI_YELLOW);
  drawTabIndicator();
}

// Render the moving region off-screen, then flush it to the LCD as one RGB565
// bitmap. This avoids the many separate SPI drawing operations that caused
// visible tearing/flicker during the earlier direct-to-display animation.
void drawForecastViewport(int baseOffset, int scrollPixels) {
  if (forecastCanvas != nullptr && !renderingTabTransition) {
    forecastCanvas->fillScreen(BG);

    int firstIndex = max(0, baseOffset - 1);
    int lastIndex = min(forecastCount, baseOffset + ITEMS_PER_PAGE + 1);
    for (int index = firstIndex; index < lastIndex; index++) {
      int y = FORECAST_ROW_TOP +
              (index - baseOffset) * FORECAST_ROW_STEP -
              scrollPixels;

      // Ignore fully off-screen cards. The canvas clips any partial card
      // at its edges, so no card can overwrite the fixed title or footer.
      if (y + FORECAST_CARD_HEIGHT <= FORECAST_VIEWPORT_TOP ||
          y >= FORECAST_VIEWPORT_BOTTOM) {
        continue;
      }

      drawForecastItem(forecastCanvas, index, y - FORECAST_VIEWPORT_TOP);
    }

    forecastCanvas->flush();
    return;
  }

  // Safe fallback if the canvas buffer could not be allocated: render the
  // three settled rows directly and disable frame-by-frame animation below.
  gfx->fillRect(
    0,
    FORECAST_VIEWPORT_TOP,
    screenW,
    FORECAST_VIEWPORT_HEIGHT,
    BG);

  int visible = min(ITEMS_PER_PAGE, max(0, forecastCount - baseOffset));
  for (int i = 0; i < visible; i++) {
    int y = FORECAST_ROW_TOP + i * FORECAST_ROW_STEP;
    drawForecastItem(gfx, baseOffset + i, y);
  }
}

void drawForecast() {
  currentTab = 1;
  gfx->fillScreen(BG);
  drawHeader();
  drawText("UPCOMING HOURLY FORECAST", 10, 44, 1, UI_WHITE);

  if (!forecastAvailable || forecastCount == 0) {
    drawCenteredText(forecastError.length() ? forecastError : "No hourly data", 100, 1, MUTED);
    drawForecastFooter();
    return;
  }

  // Clear only the forecast area; the static header/title/footer remain fixed.
  gfx->fillRect(
    0,
    FORECAST_VIEWPORT_TOP,
    screenW,
    FORECAST_VIEWPORT_HEIGHT,
    BG);
  drawForecastViewport(forecastOffset, 0);
  drawForecastFooter();
}

// Render the destination tab off-screen, then slide it over the current tab.
// The viewport renderers bypass their own canvases during composition so they
// cannot flush partial content to the physical display mid-transition.
void animateTabTransition(int targetTab, int direction) {
  if (targetTab == currentTab) return;

  if (tabTransitionCanvas == nullptr) {
    if (targetTab == 0) drawWeather();
    else drawForecast();
    return;
  }

  Arduino_GFX* liveDisplay = gfx;
  renderingTabTransition = true;
  gfx = tabTransitionCanvas;

  if (targetTab == 0) drawWeather();
  else drawForecast();

  gfx = liveDisplay;
  renderingTabTransition = false;

  uint16_t* frameBuffer = tabTransitionCanvas->getFramebuffer();
  if (frameBuffer == nullptr) {
    // The composed frame is unavailable: redraw directly rather than leave
    // the previous tab visible.
    if (targetTab == 0) drawWeather();
    else drawForecast();
    return;
  }

  const int contentHeight = screenH -
    TAB_TRANSITION_CONTENT_TOP -
    TAB_TRANSITION_FOOTER_HEIGHT;
  const uint16_t* contentFrame = frameBuffer +
    ((size_t)TAB_TRANSITION_CONTENT_TOP * (size_t)screenW);

  const int animationFrames = 8;
  const int animationDelayMs = 9;

  for (int frame = 1; frame < animationFrames; frame++) {
    float progress = (float)frame / animationFrames;
    float easedProgress = progress * progress * (3.0f - 2.0f * progress);
    int travel = (int)(screenW * easedProgress);
    int xOffset = direction > 0 ? screenW - travel : -screenW + travel;

    // Slide only the central content. Header, divider and footer remain fixed
    // while the incoming page covers the outgoing page from one side.
    liveDisplay->draw16bitRGBBitmap(
      xOffset,
      TAB_TRANSITION_CONTENT_TOP,
      (uint16_t*)contentFrame,
      screenW,
      contentHeight);
    delay(animationDelayMs);
  }

  // Finish at an exact alignment, then update only the footer's small dynamic
  // range label and tab marker; do not repaint the shared header/footer bars.
  liveDisplay->draw16bitRGBBitmap(
    0,
    TAB_TRANSITION_CONTENT_TOP,
    (uint16_t*)contentFrame,
    screenW,
    contentHeight);

  const int footerLabelX = screenW - 64;
  const int footerLabelY = screenH - 28;
  liveDisplay->fillRect(footerLabelX, footerLabelY, 64, 14, BG);
  if (targetTab == 1) {
    int firstItem = forecastCount > 0 ? forecastOffset + 1 : 0;
    int lastItem = min(forecastOffset + ITEMS_PER_PAGE, forecastCount);
    String rangeText = String(firstItem) + "-" + String(lastItem) +
                       "/" + String(forecastCount);
    drawText(rangeText, screenW - 58, screenH - 25, 1, UI_YELLOW);
  }
  drawTabIndicator();
}

// Animate one row with easing. Each frame is drawn into the off-screen canvas
// and transferred as a single viewport bitmap; the header does not repaint.
void animateForecastScroll(int direction) {
  int targetOffset = forecastOffset + direction;
  int maxOffset = max(0, forecastCount - ITEMS_PER_PAGE);
  if (targetOffset < 0 || targetOffset > maxOffset) return;

  // If memory is unavailable, prefer a single clean page redraw over a
  // visibly flickering multi-frame direct-render animation.
  if (forecastCanvas == nullptr) {
    forecastOffset = targetOffset;
    drawForecast();
    return;
  }

  const int animationFrames = 7;
  const int animationDelayMs = 8;
  int startOffset = forecastOffset;

  for (int frame = 1; frame <= animationFrames; frame++) {
    float progress = (float)frame / animationFrames;
    float easedProgress = progress * progress * (3.0f - 2.0f * progress);
    int scrollPixels = (int)(direction * FORECAST_ROW_STEP * easedProgress);
    drawForecastViewport(startOffset, scrollPixels);
    delay(animationDelayMs);
  }

  forecastOffset = targetOffset;
  // Only the counter needs updating after the last frame; the final animated
  // viewport already matches the target offset.
  drawForecastFooter();
}

// =====================================================
// TOUCH REGISTER
// =====================================================

bool readTouchSample(TouchSample& sample) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(REG_GESTURE);
  if (Wire.endTransmission(false) != 0) return false;

  const uint8_t sampleSize = 6;
  uint8_t received = Wire.requestFrom(
    (uint8_t)TOUCH_ADDR,
    sampleSize);
  if (received != sampleSize) {
    while (Wire.available()) Wire.read();
    return false;
  }

  uint8_t data[sampleSize];
  for (uint8_t i = 0; i < sampleSize; i++) {
    data[i] = (uint8_t)Wire.read();
  }

  sample.gesture = data[0];
  sample.fingerCount = data[1] & 0x0F;
  sample.event = (data[2] >> 6) & 0x03; // 0=down, 1=up, 2=contact
  sample.rawX = ((uint16_t)(data[2] & 0x0F) << 8) | data[3];
  sample.rawY = ((uint16_t)(data[4] & 0x0F) << 8) | data[5];
  return true;
}

// The CST816 coordinates are reported in the panel's portrait-native frame.
// This board is mounted in landscape. The observed gesture mapping corresponds
// to screenX = rawY, screenY = reversed rawX for the 320x240 logical display.
void touchCoordinatesToScreen(
  const TouchSample& sample,
  int& screenX,
  int& screenY
) {
  screenX = (int)sample.rawY;
  screenY = screenH - 1 - (int)sample.rawX;

  screenX = constrain(screenX, 0, screenW - 1);
  screenY = constrain(screenY, 0, screenH - 1);
}

void updateWeatherMetricsTouchDrag(const TouchSample& sample) {
  const bool touchPresent =
    sample.fingerCount > 0 && sample.event != 1; // ignore touch-up coordinates

  if (!metricsTouchDragActive && touchPresent && currentTab == 0) {
    int touchX = 0;
    int touchY = 0;
    touchCoordinatesToScreen(sample, touchX, touchY);

    bool insideMetricsPanel =
      touchX >= weatherMetricsPanelX() &&
      touchY >= WEATHER_METRIC_PANEL_TOP &&
      touchY < WEATHER_METRIC_PANEL_BOTTOM;

    if (insideMetricsPanel) {
      metricsTouchDragActive = true;
      metricsTouchDragMoved = false;
      metricsDragStartY = touchY;
      metricsDragStartScroll =
        weatherMetricOffset * WEATHER_METRIC_ROW_STEP;
      metricsDragCurrentScroll = metricsDragStartScroll;
      DEBUG_PRINTF(
        "Metric drag started at screen (%d,%d), scroll=%d\n",
        touchX,
        touchY,
        metricsDragStartScroll);
    }
  }

  if (metricsTouchDragActive && touchPresent) {
    int touchX = 0;
    int touchY = 0;
    touchCoordinatesToScreen(sample, touchX, touchY);
    int nextScroll = clampWeatherMetricScrollPixels(
      metricsDragStartScroll + metricsDragStartY - touchY);

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
      (metricsDragCurrentScroll + WEATHER_METRIC_ROW_STEP / 2) /
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
}

// =====================================================
// NAVIGATION
// =====================================================

void handleGesture(uint8_t gesture) {
  unsigned long now = millis();
  if (now - lastGestureTime < 250UL) return;
  lastGestureTime = now;

  DEBUG_PRINTF(
    "Navigation gesture accepted: raw=0x%02X; tab=%d; offset=%d\n",
    gesture, currentTab, forecastOffset);

  // These are provisional mappings. Use the raw transition logs to verify
  // physical directions on this particular panel before changing the codes.
  if (gesture == GESTURE_SCREEN_RIGHT) {
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

  if (gesture == GESTURE_SCREEN_LEFT) {
    if (currentTab == 1) {
      animateTabTransition(0, -1);
      DEBUG_PRINTLN("Action: mapped horizontal-right gesture returned to weather");
    } else {
      DEBUG_PRINTLN("Action: mapped horizontal-right gesture; already on weather");
    }
    return;
  }

  if (currentTab == 0) {
    if (gesture == GESTURE_SCREEN_UP) {
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
    } else if (gesture == GESTURE_SCREEN_DOWN) {
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

  if (gesture == GESTURE_SCREEN_UP) {
    int maxOffset = max(0, forecastCount - ITEMS_PER_PAGE);
    if (forecastOffset < maxOffset) {
      animateForecastScroll(1);
      DEBUG_PRINTF("Action: mapped vertical-up gesture advanced one entry; offset=%d\n", forecastOffset);
    } else {
      DEBUG_PRINTLN("Action: already at end of forecast");
    }
    return;
  }

  if (gesture == GESTURE_SCREEN_DOWN) {
    if (forecastOffset > 0) {
      animateForecastScroll(-1);
      DEBUG_PRINTF("Action: mapped vertical-down gesture moved back one entry; offset=%d\n", forecastOffset);
    } else {
      DEBUG_PRINTLN("Action: already at beginning of forecast");
    }
  }
}

void pollTouch() {
  if (!touchAvailable) return;

  TouchSample sample;
  if (!readTouchSample(sample)) return;
  uint8_t gesture = sample.gesture;

  // Always log transitions in demo mode, including the return to zero.
  if (gesture != previousRawGesture) {
    DEBUG_PRINTF("Touch register transition: 0x%02X -> 0x%02X\n", previousRawGesture, gesture);
    previousRawGesture = gesture;
  }

  updateWeatherMetricsTouchDrag(sample);

  // A drag on the metric cards is driven by coordinates. Suppress the
  // controller's completed swipe code so the same motion is not applied twice.
  bool releaseGestureSuppressed =
    suppressMetricGesture &&
    (millis() - metricsGestureSuppressionStartedMs < 300UL);
  if (suppressMetricGesture && !releaseGestureSuppressed) {
    suppressMetricGesture = false;
  }

  if (metricsTouchDragActive || releaseGestureSuppressed) {
    if (gesture != 0) lastHandledGesture = gesture;
    else lastHandledGesture = 0;
    return;
  }

  if (gesture == 0) {
    lastHandledGesture = 0;
    return;
  }

  if (gesture < GESTURE_SCREEN_LEFT || gesture > GESTURE_SCREEN_UP) return;
  if (gesture == lastHandledGesture) return;

  lastHandledGesture = gesture;
  DEBUG_PRINTF("Touch raw gesture: 0x%02X\n", gesture);
  handleGesture(gesture);
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
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url =
    "https://api.openweathermap.org/data/2.5/weather?q=" + String(CITY) + "&appid=" + String(apiKey) + "&units=metric";

  if (!http.begin(client, url)) {
    if (weatherAvailable) drawWeather();
    else drawError("HTTP init failed");
    return false;
  }

  http.setConnectTimeout(15000);
  http.setTimeout(15000);
  int code = http.GET();

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
  locationLat = doc["coord"]["lat"] | locationLat;
  locationLon = doc["coord"]["lon"] | locationLon;
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
  const time_t now = time(nullptr);
  for (int i = 0; i < forecastCount; i++) {
    int sample = i % 12;
    forecast[i].timestamp = (uint32_t)(now + (time_t)i * 3600);
    forecast[i].timeText = formatForecastTime(
      forecast[i].timestamp, locationTimezoneOffset);
    forecast[i].description = descriptions[sample];
    forecast[i].temp = temperatures[sample];
  }
  forecastAvailable = true;
  DEBUG_PRINTLN("Demo mode: loaded local weather and 24 sample forecast entries; no API calls.");
  return true;
}

bool fetchOneCallForecastPage(const String& url, String& nextUrl) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) {
    forecastError = "Hourly API unavailable";
    DEBUG_PRINTLN("One Call HTTP initialisation failed");
    return false;
  }

  http.setConnectTimeout(15000);
  http.setTimeout(15000);
  int code = http.GET();

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

  if (!gfx->begin()) {
    Serial.println("Display initialisation failed!");
    while (true) delay(1000);
  }

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  screenW = gfx->width();
  screenH = gfx->height();

  // Buffer only the scrolling forecast viewport, not the entire 320x240 UI.
  // This keeps memory use modest while letting each animation frame flush
  // as one rectangular bitmap to the ST7789.
  forecastCanvas = new Arduino_Canvas(
    screenW,
    FORECAST_VIEWPORT_HEIGHT,
    gfx,
    0,
    FORECAST_VIEWPORT_TOP,
    0);

  if (forecastCanvas == nullptr ||
      !forecastCanvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
    DEBUG_PRINTLN("Forecast canvas unavailable; animated scrolling disabled.");
    delete forecastCanvas;
    forecastCanvas = nullptr;
  } else {
    DEBUG_PRINTF(
      "Forecast canvas ready: %d x %d\n",
      screenW,
      FORECAST_VIEWPORT_HEIGHT);
  }

  // Buffer just the narrow metrics column (about 35 KB at 118x152 RGB565).
  weatherMetricsCanvas = new Arduino_Canvas(
    weatherMetricsPanelWidth(),
    WEATHER_METRIC_PANEL_HEIGHT,
    gfx,
    weatherMetricsPanelX(),
    WEATHER_METRIC_PANEL_TOP,
    0);

  if (weatherMetricsCanvas == nullptr ||
      !weatherMetricsCanvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
    DEBUG_PRINTLN("Weather-metrics canvas unavailable; scrolling animation disabled.");
    delete weatherMetricsCanvas;
    weatherMetricsCanvas = nullptr;
  } else {
    DEBUG_PRINTF(
      "Weather-metrics canvas ready: %d x %d\n",
      weatherMetricsPanelWidth(),
      WEATHER_METRIC_PANEL_HEIGHT);
  }

  // Full-screen RGB565 buffer used only when changing tabs. This is the
  // largest canvas, so allocate it last and keep instant-switch fallback.
  tabTransitionCanvas = new Arduino_Canvas(
    screenW,
    screenH,
    gfx,
    0,
    0,
    0);

  if (tabTransitionCanvas == nullptr ||
      !tabTransitionCanvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
    DEBUG_PRINTLN("Tab-transition canvas unavailable; tab changes will be instant.");
    delete tabTransitionCanvas;
    tabTransitionCanvas = nullptr;
  } else {
    DEBUG_PRINTF(
      "Tab-transition canvas ready: %d x %d\n",
      screenW,
      screenH);
  }

  // Determine the actual heap region for every canvas, rather than assuming
  // aligned_alloc() placed them in PSRAM. These checks inspect live pointers.
  reportCanvasAllocation(
    "forecast",
    forecastCanvas,
    (size_t)screenW * FORECAST_VIEWPORT_HEIGHT * sizeof(uint16_t));
  reportCanvasAllocation(
    "weather metrics",
    weatherMetricsCanvas,
    (size_t)weatherMetricsPanelWidth() *
      WEATHER_METRIC_PANEL_HEIGHT * sizeof(uint16_t));
  reportCanvasAllocation(
    "tab transition",
    tabTransitionCanvas,
    (size_t)screenW * screenH * sizeof(uint16_t));
  reportHeapSummary();

  DEBUG_PRINTF("Logical display size: %d x %d\n", screenW, screenH);
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(100000);

  Wire.beginTransmission(TOUCH_ADDR);
  touchAvailable = Wire.endTransmission() == 0;
  DEBUG_PRINTLN(touchAvailable ? "CST816 detected at 0x15" : "Touch controller not detected");

  initialiseTimeZone();

#if USE_DEMO_WEATHER_DATA
  // Set a repeatable local clock so the header and forecast time labels render
  // during offline UI development. The clock advances normally after setup.
  struct tm demoClock = {};
  demoClock.tm_year = 2026 - 1900;
  demoClock.tm_mon = 9;
  demoClock.tm_mday = 28;
  demoClock.tm_hour = 18;
  demoClock.tm_min = 0;
  demoClock.tm_isdst = -1;
  time_t demoEpoch = mktime(&demoClock);
  struct timeval demoSystemTime;
  demoSystemTime.tv_sec = demoEpoch;
  demoSystemTime.tv_usec = 0;
  settimeofday(&demoSystemTime, nullptr);
  DEBUG_PRINTLN("DEMO MODE ENABLED: network and API calls are disabled.");
#else
  drawLoading("Starting up...");
  connectWiFi();  // Starts a time API request when connectivity is available.
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