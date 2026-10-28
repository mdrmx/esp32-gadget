#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "secrets.h"

// =====================================================
// CONFIGURATION
// =====================================================

const char* wifiSsid = WIFI_SSID;
const char* wifiPassword = WIFI_PASSWORD;
const char* apiKey = OPENWEATHER_API_KEY;
const char* CITY = "Manchester,GB";
float locationLat = 53.4808f;
float locationLon = -2.2426f;
long locationTimezoneOffset = 0;
const unsigned long UPDATE_INTERVAL = 600000UL;

// =====================================================
// DISPLAY
// =====================================================

#define LCD_SCLK 39
#define LCD_MOSI 38
#define LCD_MISO 40
#define LCD_DC   42
#define LCD_RST  -1
#define LCD_CS   45
#define LCD_BL   1

#define LCD_ROTATION 1
#define LCD_WIDTH    240
#define LCD_HEIGHT   320

Arduino_DataBus* bus = new Arduino_ESP32SPI(
  LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO
);

Arduino_GFX* gfx = new Arduino_ST7789(
  bus, LCD_RST, LCD_ROTATION, true, LCD_WIDTH, LCD_HEIGHT
);

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
#define GESTURE_SCREEN_LEFT  0x01
#define GESTURE_SCREEN_RIGHT 0x02
#define GESTURE_SCREEN_DOWN  0x03
#define GESTURE_SCREEN_UP    0x04

bool touchAvailable = false;
uint8_t lastHandledGesture = 0;
uint8_t previousRawGesture = 0;
unsigned long lastGestureTime = 0;

// =====================================================
// COLOURS
// =====================================================

const uint16_t BG           = 0x1082;
const uint16_t PANEL        = 0x2128;
const uint16_t UI_WHITE     = 0xFFFF;
const uint16_t MUTED        = 0xBDF7;
const uint16_t ACCENT       = 0x5DDF;
const uint16_t UI_YELLOW    = 0xFEA0;
const uint16_t UI_GREEN     = 0x7E8A;
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
bool weatherAvailable = false;
bool forecastAvailable = false;
unsigned long lastUpdate = 0;

const int MAX_FORECAST = 48;
const int ITEMS_PER_PAGE = 4;

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

// =====================================================
// HEADER
// =====================================================

void drawHeader() {
  gfx->fillRect(0, 0, screenW, 36, PANEL);
  drawText("WEATHER", 10, 10, 2, UI_WHITE);

  drawText(
    currentTab == 0 ? "NOW" : "FORECAST",
    screenW - (currentTab == 0 ? 48 : 80),
    13, 1, ACCENT
  );
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
// WEATHER CONDITION ICONS (drawn with Arduino_GFX primitives)
// =====================================================

void drawSunIcon(int cx, int cy, int radius) {
  const uint16_t sunColour = UI_YELLOW;
  gfx->drawLine(cx, cy - radius - 7, cx, cy - radius - 3, sunColour);
  gfx->drawLine(cx, cy + radius + 3, cx, cy + radius + 7, sunColour);
  gfx->drawLine(cx - radius - 7, cy, cx - radius - 3, cy, sunColour);
  gfx->drawLine(cx + radius + 3, cy, cx + radius + 7, cy, sunColour);
  gfx->drawLine(cx - radius - 5, cy - radius - 5, cx - radius - 2, cy - radius - 2, sunColour);
  gfx->drawLine(cx + radius + 2, cy - radius - 2, cx + radius + 5, cy - radius - 5, sunColour);
  gfx->drawLine(cx - radius - 5, cy + radius + 5, cx - radius - 2, cy + radius + 2, sunColour);
  gfx->drawLine(cx + radius + 2, cy + radius + 2, cx + radius + 5, cy + radius + 5, sunColour);
  gfx->fillCircle(cx, cy, radius, sunColour);
}

void drawMoonIcon(int cx, int cy, int radius) {
  gfx->fillCircle(cx, cy, radius, UI_YELLOW);
  gfx->fillCircle(cx + radius / 2, cy - radius / 2, radius, BG);
}

void drawCloudIcon(int x, int y, uint16_t colour) {
  gfx->fillCircle(x + 13, y + 17, 10, colour);
  gfx->fillCircle(x + 26, y + 11, 13, colour);
  gfx->fillCircle(x + 39, y + 18, 9, colour);
  gfx->fillRoundRect(x + 5, y + 16, 40, 16, 7, colour);
}

void drawWeatherConditionIcon(const String& iconCode, const String& condition, int x, int y) {
  String code = iconCode.length() >= 2 ? iconCode.substring(0, 2) : "";
  bool night = iconCode.endsWith("n");
  uint16_t cloudColour = MUTED;

  if (code == "01") {
    if (night) drawMoonIcon(x + 28, y + 25, 10);
    else drawSunIcon(x + 28, y + 25, 10);
    return;
  }

  if (code == "02") {
    if (night) drawMoonIcon(x + 35, y + 13, 7);
    else drawSunIcon(x + 35, y + 13, 7);
    drawCloudIcon(x + 1, y + 13, cloudColour);
    return;
  }

  if (code == "03" || code == "04" || code == "09" ||
      code == "10" || code == "11" || code == "13") {
    drawCloudIcon(x + 1, y + 9, cloudColour);
  }

  if (code == "09" || code == "10") {
    gfx->drawLine(x + 15, y + 42, x + 11, y + 48, ACCENT);
    gfx->drawLine(x + 28, y + 42, x + 24, y + 48, ACCENT);
    gfx->drawLine(x + 40, y + 42, x + 36, y + 48, ACCENT);
    return;
  }

  if (code == "11") {
    gfx->fillTriangle(x + 27, y + 34, x + 19, y + 44, x + 26, y + 44, UI_YELLOW);
    gfx->fillTriangle(x + 26, y + 42, x + 31, y + 42, x + 22, y + 51, UI_YELLOW);
    return;
  }

  if (code == "13") {
    gfx->fillCircle(x + 16, y + 42, 2, UI_WHITE);
    gfx->fillCircle(x + 28, y + 46, 2, UI_WHITE);
    gfx->fillCircle(x + 39, y + 41, 2, UI_WHITE);
    gfx->drawLine(x + 28, y + 42, x + 28, y + 49, UI_WHITE);
    gfx->drawLine(x + 24, y + 46, x + 32, y + 46, UI_WHITE);
    return;
  }

  if (code == "50") {
    gfx->drawFastHLine(x + 2, y + 14, 48, MUTED);
    gfx->drawFastHLine(x + 7, y + 24, 40, MUTED);
    gfx->drawFastHLine(x + 1, y + 34, 46, MUTED);
    return;
  }

  if (code == "03" || code == "04") {
    // A second, darker cloud gives overcast conditions more visual weight.
    gfx->drawFastHLine(x + 8, y + 42, 38, cloudColour);
  } else if (condition.equalsIgnoreCase("Clear")) {
    if (night) drawMoonIcon(x + 28, y + 25, 10);
    else drawSunIcon(x + 28, y + 25, 10);
  } else {
    drawCloudIcon(x + 1, y + 9, cloudColour);
  }
}

// =====================================================
// CURRENT WEATHER SCREEN
// =====================================================

void drawWeather() {
  currentTab = 0;
  gfx->fillScreen(BG);
  drawHeader();

  String city = weatherCity;
  if (city.length() > 18) city = city.substring(0, 17) + ".";
  drawText(city, 12, 48, 2, UI_WHITE);

  String description = weatherDescription;
  if (description.length() > 23) description = description.substring(0, 22) + ".";
  drawText(description, 12, 73, 1, MUTED);

  // Stack the condition icon above the temperature in the left column.
  drawWeatherConditionIcon(weatherIcon, weatherMain, 68, 78);
  drawText(String(temperature, 1) + " C", 42, 143, 3, UI_YELLOW);
  drawText("Feels " + String(feelsLike, 1) + " C", 63, 178, 1, MUTED);

  int cardX = (screenW * 60) / 100;
  int cardW = screenW - cardX - 10;

  gfx->fillRoundRect(cardX, 47, cardW, 72, 8, PANEL);
  drawText("HUMIDITY", cardX + 10, 57, 1, MUTED);
  gfx->setTextSize(3);
  gfx->setTextColor(ACCENT);
  gfx->setCursor(cardX + 10, 78);
  gfx->printf("%d%%", humidity);

  gfx->fillRoundRect(cardX, 127, cardW, 72, 8, PANEL);
  drawText("PRESSURE", cardX + 10, 137, 1, MUTED);
  gfx->setTextSize(2);
  gfx->setTextColor(ACCENT);
  gfx->setCursor(cardX + 10, 157);
  gfx->printf("%d", pressure);
  drawText("hPa", cardX + 10, 183, 1, MUTED);

  gfx->drawFastHLine(10, screenH - 29, screenW - 20, PANEL);
  drawText("SWIPE RIGHT TO LEFT", 12, screenH - 19, 1, MUTED);
  drawTabIndicator();
}

// =====================================================
// FORECAST SCREEN
// =====================================================

void drawForecast() {
  currentTab = 1;
  gfx->fillScreen(BG);
  drawHeader();
  drawText("UPCOMING HOURLY FORECAST", 10, 44, 1, UI_WHITE);

  int visible = forecastCount - forecastOffset;
  if (visible > ITEMS_PER_PAGE) visible = ITEMS_PER_PAGE;

  if (!forecastAvailable || forecastCount == 0) {
    drawCenteredText(forecastError.length() ? forecastError : "No hourly data", 100, 1, MUTED);
  } else {
    for (int i = 0; i < visible; i++) {
      int index = forecastOffset + i;
      int y = 62 + i * 39;
      gfx->fillRoundRect(8, y, screenW - 16, 33, 6, PANEL);
      drawText(forecast[index].timeText, 16, y + 11, 1, UI_WHITE);

      String desc = forecast[index].description;
      if (desc.length() > 12) desc = desc.substring(0, 11) + ".";
      drawText(desc, 90, y + 11, 1, MUTED);

      String temp = String(forecast[index].temp, 1) + "C";
      gfx->setTextSize(1);
      gfx->setTextColor(UI_YELLOW);
      int16_t x1, y1;
      uint16_t w, h;
      gfx->getTextBounds(temp.c_str(), 0, y + 11, &x1, &y1, &w, &h);
      gfx->setCursor(screenW - 16 - w, y + 11);
      gfx->print(temp);
    }
  }

  int firstItem = forecastCount > 0 ? forecastOffset + 1 : 0;
  int lastItem = min(forecastOffset + ITEMS_PER_PAGE, forecastCount);
  String rangeText = String(firstItem) + "-" + String(lastItem) + "/" + String(forecastCount);
  drawText("UP/DOWN SCROLL", 10, screenH - 25, 1, MUTED);
  drawText(rangeText, screenW - 58, screenH - 25, 1, UI_YELLOW);
  drawTabIndicator();
}

// =====================================================
// TOUCH REGISTER
// =====================================================

bool readTouchRegister(uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}

// =====================================================
// NAVIGATION
// =====================================================

void handleGesture(uint8_t gesture) {
  unsigned long now = millis();
  if (now - lastGestureTime < 250UL) return;
  lastGestureTime = now;

  Serial.printf(
    "Navigation gesture accepted: raw=0x%02X; tab=%d; offset=%d\n",
    gesture, currentTab, forecastOffset
  );

  // These are provisional mappings. Use the raw transition logs to verify
  // physical directions on this particular panel before changing the codes.
  if (gesture == GESTURE_SCREEN_RIGHT) {
    if (currentTab == 0) {
      forecastOffset = 0;
      if (forecastAvailable && forecastCount > 0) {
        drawForecast();
        Serial.println("Action: mapped horizontal-left gesture opened forecast");
      } else {
        // Show the forecast screen even when retrieval failed, so the API
        // error message is visible instead of only a generic unavailable log.
        drawForecast();
        Serial.printf("Action: forecast unavailable; reason: %s\\n", forecastError.length() ? forecastError.c_str() : "No hourly records");
      }
    } else {
      Serial.println("Action: mapped horizontal-left gesture; already on forecast");
    }
    return;
  }

  if (gesture == GESTURE_SCREEN_LEFT) {
    if (currentTab == 1) {
      drawWeather();
      Serial.println("Action: mapped horizontal-right gesture returned to weather");
    } else {
      Serial.println("Action: mapped horizontal-right gesture; already on weather");
    }
    return;
  }

  if (currentTab == 0) {
    Serial.println("Action: vertical gesture ignored on current-weather screen");
    return;
  }

  if (gesture == GESTURE_SCREEN_UP) {
    int maxOffset = max(0, forecastCount - ITEMS_PER_PAGE);
    if (forecastOffset < maxOffset) {
      forecastOffset++;
      drawForecast();
      Serial.printf("Action: mapped vertical-up gesture advanced one entry; offset=%d\n", forecastOffset);
    } else {
      Serial.println("Action: already at end of forecast");
    }
    return;
  }

  if (gesture == GESTURE_SCREEN_DOWN) {
    if (forecastOffset > 0) {
      forecastOffset--;
      drawForecast();
      Serial.printf("Action: mapped vertical-down gesture moved back one entry; offset=%d\n", forecastOffset);
    } else {
      Serial.println("Action: already at beginning of forecast");
    }
  }
}

void pollTouch() {
  if (!touchAvailable) return;

  uint8_t gesture = 0;
  if (!readTouchRegister(REG_GESTURE, gesture)) return;

  // Always log transitions, including the return to zero, so a real swipe
  // can be identified from the exact controller code and stale register state.
  if (gesture != previousRawGesture) {
    Serial.printf("Touch register transition: 0x%02X -> 0x%02X\n", previousRawGesture, gesture);
    previousRawGesture = gesture;
  }

  if (gesture == 0) {
    lastHandledGesture = 0;
    return;
  }

  if (gesture < GESTURE_SCREEN_LEFT || gesture > GESTURE_SCREEN_UP) return;
  if (gesture == lastHandledGesture) return;

  lastHandledGesture = gesture;
  Serial.printf("Touch raw gesture: 0x%02X\n", gesture);
  handleGesture(gesture);
}

// =====================================================
// WI-FI
// =====================================================

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  drawLoading("Connecting Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPassword);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000UL) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("Wi-Fi connection timed out");
  return false;
}

// =====================================================
// FETCH CURRENT WEATHER
// =====================================================

bool fetchWeather() {
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
    "https://api.openweathermap.org/data/2.5/weather?q=" +
    String(CITY) +
    "&appid=" + String(apiKey) +
    "&units=metric";

  if (!http.begin(client, url)) {
    if (weatherAvailable) drawWeather();
    else drawError("HTTP init failed");
    return false;
  }

  http.setConnectTimeout(15000);
  http.setTimeout(15000);
  int code = http.GET();

  if (code != HTTP_CODE_OK) {
    Serial.printf("Weather HTTP status: %d\n", code);
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
    Serial.printf("Weather JSON error: %s\n", error.c_str());
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
  weatherAvailable = true;

  Serial.printf(
    "%s: %.1f C, %s, humidity %d%%, pressure %d hPa\n",
    weatherCity.c_str(), temperature, weatherDescription.c_str(), humidity, pressure
  );
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
      const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
      return String(dayNames[localTime.tm_wday]) + " " + String(clockText);
    }
  }

  return String(clockText);
}

bool fetchOneCallForecastPage(const String& url, String& nextUrl) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) {
    forecastError = "Hourly API unavailable";
    Serial.println("One Call HTTP initialisation failed");
    return false;
  }

  http.setConnectTimeout(15000);
  http.setTimeout(15000);
  int code = http.GET();

  if (code != HTTP_CODE_OK) {
    // Capture the response body before closing HTTP. OpenWeather normally
    // includes a JSON message for rejected keys, subscriptions, and limits.
    String errorBody = http.getString();
    Serial.printf("One Call hourly HTTP status: %d\\n", code);
    if (errorBody.length() > 0) {
      if (errorBody.length() > 240) errorBody = errorBody.substring(0, 240);
      Serial.printf("One Call error response: %s\\n", errorBody.c_str());
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
    Serial.printf("One Call hourly JSON error: %s\n", error.c_str());
    forecastError = "Hourly data parse failed";
    return false;
  }

  locationTimezoneOffset = doc["timezone_offset"] | locationTimezoneOffset;
  nextUrl = doc["next"] | "";

  JsonArray list = doc["data"].as<JsonArray>();
  if (list.isNull()) {
    Serial.println("One Call response did not contain a data array.");
    serializeJson(doc, Serial);
    Serial.println();
    forecastError = "Unexpected One Call data";
    return false;
  }
  Serial.printf("One Call page contains %u record(s).\\n", (unsigned)list.size());
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

    forecast[forecastCount].timestamp = timestamp;
    forecast[forecastCount].timeText = formatForecastTime(timestamp, locationTimezoneOffset);
    forecast[forecastCount].description = String(desc);
    forecast[forecastCount].temp = temp;
    forecastCount++;
  }

  return true;
}

bool fetchForecast() {
  if (!connectWiFi()) {
    forecastAvailable = false;
    forecastError = "Wi-Fi unavailable";
    return false;
  }

  forecastCount = 0;
  forecastError = "";

  String nextUrl =
    "https://api.openweathermap.org/data/4.0/onecall/timeline/1h?lat=" +
    String(locationLat, 4) +
    "&lon=" + String(locationLon, 4) +
    "&units=metric&appid=" + String(apiKey);

  // The 1-hour endpoint returns up to 20 entries per request. Follow its
  // prepared pagination URL until we have up to 48 hours or no next page.
  int pageCount = 0;
  const int MAX_HOURLY_PAGES = 3;

  while (nextUrl.length() > 0 &&
         forecastCount < MAX_FORECAST &&
         pageCount < MAX_HOURLY_PAGES) {
    String pageUrl = nextUrl;
    String pageNext = "";
    if (!fetchOneCallForecastPage(pageUrl, pageNext)) {
      if (forecastCount == 0) {
        forecastAvailable = false;
        Serial.println("Hourly forecast unavailable. Check One Call API 4.0 subscription and API key.");
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
    Serial.println("One Call returned without any parsable hourly records.");
  }

  Serial.printf("Hourly forecast records received: %d across %d page(s)\\n", forecastCount, pageCount);
  return forecastAvailable;
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Waveshare Weather Dashboard");

  if (!gfx->begin()) {
    Serial.println("Display initialisation failed!");
    while (true) delay(1000);
  }

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  screenW = gfx->width();
  screenH = gfx->height();

  Serial.printf("Logical display size: %d x %d\n", screenW, screenH);
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(100000);

  Wire.beginTransmission(TOUCH_ADDR);
  touchAvailable = Wire.endTransmission() == 0;
  Serial.println(touchAvailable ? "CST816 detected at 0x15" : "Touch controller not detected");

  drawLoading("Starting up...");
  connectWiFi();
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

  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = millis();
    Serial.println("Refreshing weather and forecast...");
    bool currentOK = fetchWeather();
    fetchForecast();

    if (currentTab == 1) drawForecast();
    else if (currentOK || weatherAvailable) drawWeather();
    else drawError("Refresh failed");
  }

  delay(30);
}
