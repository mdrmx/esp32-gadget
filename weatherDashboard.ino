
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

// CST816 gesture values mapped to the physical screen in landscape.
// Observed on this device: physical bottom-to-top reports 0x04 and
// physical top-to-bottom reports 0x03. With the 90-degree landscape
// rotation, physical right-to-left / left-to-right report 0x01 / 0x02.
#define GESTURE_SCREEN_LEFT  0x01  // physical right-to-left: open forecast
#define GESTURE_SCREEN_RIGHT 0x02  // physical left-to-right: return to current weather
#define GESTURE_SCREEN_DOWN  0x03  // physical top-to-bottom: previous forecast entry
#define GESTURE_SCREEN_UP    0x04  // physical bottom-to-top: next forecast entry

bool touchAvailable = false;
uint8_t lastHandledGesture = 0;
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

float temperature = 0.0;
float feelsLike = 0.0;
int humidity = 0;
int pressure = 0;

bool weatherAvailable = false;
bool forecastAvailable = false;

unsigned long lastUpdate = 0;

const int MAX_FORECAST = 40;
const int ITEMS_PER_PAGE = 4;

struct ForecastItem {
  String timeText;
  String description;
  float temp;
};

ForecastItem forecast[MAX_FORECAST];

int forecastCount = 0;
int forecastOffset = 0;
int currentTab = 0;

// =====================================================
// TEXT HELPERS
// =====================================================

void drawText(
  const String& text,
  int x,
  int y,
  uint8_t size,
  uint16_t colour
) {
  gfx->setTextSize(size);
  gfx->setTextColor(colour);
  gfx->setCursor(x, y);
  gfx->print(text);
}

void drawCenteredText(
  const String& text,
  int y,
  uint8_t size,
  uint16_t colour
) {
  gfx->setTextSize(size);
  gfx->setTextColor(colour);

  int16_t x1, y1;
  uint16_t w, h;

  gfx->getTextBounds(
    text.c_str(), 0, y, &x1, &y1, &w, &h
  );

  int x = (screenW - (int)w) / 2;

  if (x < 4) x = 4;
  if (x + (int)w > screenW - 4) {
    x = screenW - 4 - w;
  }

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

  gfx->fillRoundRect(
    screenW / 2 - 24, y, 48, 4, 2, PANEL
  );

  gfx->fillRoundRect(
    screenW / 2 - 24 + (currentTab * 24),
    y, 24, 4, 2, UI_YELLOW
  );
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
// CURRENT WEATHER SCREEN
// =====================================================

void drawWeather() {
  currentTab = 0;

  gfx->fillScreen(BG);
  drawHeader();

  String city = weatherCity;
  if (city.length() > 18) {
    city = city.substring(0, 17) + ".";
  }

  drawText(city, 12, 48, 2, UI_WHITE);

  String description = weatherDescription;
  if (description.length() > 23) {
    description = description.substring(0, 22) + ".";
  }

  drawText(description, 12, 73, 1, MUTED);

  drawText(
    String(temperature, 1) + " C",
    12, 91, 4, UI_YELLOW
  );

  drawText(
    "Feels " + String(feelsLike, 1) + " C",
    12, 137, 1, MUTED
  );

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

  gfx->drawFastHLine(
    10, screenH - 29, screenW - 20, PANEL
  );

  drawText("SWIPE LEFT FOR FORECAST", 12, screenH - 19, 1, MUTED);

  drawTabIndicator();
}

// =====================================================
// FORECAST SCREEN
// =====================================================

void drawForecast() {
  currentTab = 1;

  gfx->fillScreen(BG);
  drawHeader();

  drawText("UPCOMING FORECAST", 10, 44, 1, UI_WHITE);

  int visible = forecastCount - forecastOffset;
  if (visible > ITEMS_PER_PAGE) visible = ITEMS_PER_PAGE;

  if (!forecastAvailable || forecastCount == 0) {
    drawCenteredText("No forecast data", 100, 1, MUTED);
  } else {
    for (int i = 0; i < visible; i++) {
      int index = forecastOffset + i;
      int y = 62 + i * 39;

      gfx->fillRoundRect(
        8, y, screenW - 16, 33, 6, PANEL
      );

      drawText(
        forecast[index].timeText,
        16, y + 11, 1, UI_WHITE
      );

      String desc = forecast[index].description;
      if (desc.length() > 12) {
        desc = desc.substring(0, 11) + ".";
      }

      drawText(desc, 90, y + 11, 1, MUTED);

      String temp = String(forecast[index].temp, 1) + "C";

      gfx->setTextSize(1);
      gfx->setTextColor(UI_YELLOW);

      int16_t x1, y1;
      uint16_t w, h;

      gfx->getTextBounds(
        temp.c_str(), 0, y + 11, &x1, &y1, &w, &h
      );

      gfx->setCursor(screenW - 16 - w, y + 11);
      gfx->print(temp);
    }
  }

  drawText("UP/DOWN SCROLL  |  RIGHT BACK", 10, screenH - 25, 1, MUTED);

  // Visible progress indicator to confirm that each swipe redraws the list.
  int firstItem = forecastCount > 0 ? forecastOffset + 1 : 0;
  int lastItem = min(forecastOffset + ITEMS_PER_PAGE, forecastCount);
  String rangeText = String(firstItem) + "-" + String(lastItem) + "/" + String(forecastCount);
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

  if (Wire.requestFrom(
        (uint8_t)TOUCH_ADDR, (uint8_t)1
      ) != 1) {
    return false;
  }

  value = Wire.read();
  return true;
}

// =====================================================
// NAVIGATION
// =====================================================

void handleGesture(uint8_t gesture) {
  // pollTouch() latches each reported gesture until the register clears,
  // so this is only a short guard against noisy transitions.
  unsigned long now = millis();
  if (now - lastGestureTime < 250UL) return;
  lastGestureTime = now;

  Serial.printf(
    "Navigation gesture accepted: 0x%02X; tab=%d; offset=%d\n",
    gesture, currentTab, forecastOffset
  );

  // HORIZONTAL gestures switch tabs. On the landscape display, a physical
  // right-to-left swipe reports 0x01; a left-to-right swipe reports 0x02.
  if (gesture == GESTURE_SCREEN_LEFT) {
    if (currentTab == 0) {
      forecastOffset = 0;
      if (forecastAvailable && forecastCount > 0) {
        drawForecast();
        Serial.println("Action: right-to-left swipe opened forecast");
      } else {
        drawWeather();
        drawText("Forecast unavailable", 12, screenH - 42, 1, ERROR_COLOUR);
        Serial.println("Action: forecast unavailable");
      }
    } else {
      Serial.println("Action: right-to-left swipe; already on forecast");
    }
    return;
  }

  if (gesture == GESTURE_SCREEN_RIGHT) {
    if (currentTab == 1) {
      drawWeather();
      Serial.println("Action: left-to-right swipe returned to current weather");
    } else {
      Serial.println("Action: left-to-right swipe; already on current weather");
    }
    return;
  }

  // VERTICAL gestures only scroll the forecast. They never switch tabs.
  // This is the key distinction that was missing before.
  if (currentTab == 0) {
    Serial.println("Action: vertical swipe ignored on current-weather screen");
    return;
  }

  if (gesture == GESTURE_SCREEN_UP) {
    int maxOffset = max(0, forecastCount - ITEMS_PER_PAGE);
    if (forecastOffset < maxOffset) {
      forecastOffset++;
      drawForecast();
      Serial.printf(
        "Action: bottom-to-top swipe advanced one forecast entry; offset=%d\n",
        forecastOffset
      );
    } else {
      Serial.println("Action: already at the end of the forecast");
    }
    return;
  }

  if (gesture == GESTURE_SCREEN_DOWN) {
    if (forecastOffset > 0) {
      forecastOffset--;
      drawForecast();
      Serial.printf(
        "Action: top-to-bottom swipe moved back one forecast entry; offset=%d\n",
        forecastOffset
      );
    } else {
      Serial.println("Action: already at the beginning of the forecast");
    }
  }
}

void pollTouch() {
  if (!touchAvailable) return;

  uint8_t gesture = 0;
  if (!readTouchRegister(REG_GESTURE, gesture)) return;

  // Wait for the controller to clear its gesture register before accepting
  // another gesture. Recognise all four raw direction codes so horizontal
  // tab navigation and vertical forecast scrolling remain separate.
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

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 20000UL
  ) {
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
  DeserializationError error =
    deserializeJson(doc, http.getStream());

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

  temperature = doc["main"]["temp"] | 0.0f;
  feelsLike = doc["main"]["feels_like"] | 0.0f;
  humidity = doc["main"]["humidity"] | 0;
  pressure = doc["main"]["pressure"] | 0;

  weatherAvailable = true;

  Serial.printf(
    "%s: %.1f C, %s, humidity %d%%, pressure %d hPa\n",
    weatherCity.c_str(), temperature,
    weatherDescription.c_str(), humidity, pressure
  );

  return true;
}

// =====================================================
// FETCH FORECAST
// =====================================================

bool fetchForecast() {
  if (!connectWiFi()) {
    forecastAvailable = false;
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url =
    "https://api.openweathermap.org/data/2.5/forecast?q=" +
    String(CITY) +
    "&appid=" + String(apiKey) +
    "&units=metric";

  Serial.println("Requesting forecast...");

  if (!http.begin(client, url)) {
    forecastAvailable = false;
    return false;
  }

  http.setConnectTimeout(15000);
  http.setTimeout(15000);

  int code = http.GET();

  if (code != HTTP_CODE_OK) {
    Serial.printf("Forecast HTTP status: %d\n", code);
    http.end();
    forecastAvailable = false;
    return false;
  }

  JsonDocument doc;
  DeserializationError error =
    deserializeJson(doc, http.getStream());

  http.end();

  if (error) {
    Serial.printf("Forecast JSON error: %s\n", error.c_str());
    forecastAvailable = false;
    return false;
  }

  JsonArray list = doc["list"].as<JsonArray>();
  forecastCount = 0;

  for (JsonObject item : list) {
    if (forecastCount >= MAX_FORECAST) break;

    const char* dtText = item["dt_txt"] | "";
    const char* desc = item["weather"][0]["main"] | "Unknown";
    float temp = item["main"]["temp"] | 0.0f;

    String timeText = String(dtText);

    if (timeText.length() >= 16) {
      timeText =
        timeText.substring(5, 10) + " " +
        timeText.substring(11, 16);
    }

    forecast[forecastCount].timeText = timeText;
    forecast[forecastCount].description = String(desc);
    forecast[forecastCount].temp = temp;

    forecastCount++;
  }

  forecastAvailable = forecastCount > 0;

  Serial.printf("Forecast periods received: %d\n", forecastCount);

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

  Serial.printf(
    "Logical display size: %d x %d\n",
    screenW, screenH
  );

  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(100000);

  Wire.beginTransmission(TOUCH_ADDR);
  touchAvailable = Wire.endTransmission() == 0;

  Serial.println(
    touchAvailable
      ? "CST816 detected at 0x15"
      : "Touch controller not detected"
  );

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

    if (currentTab == 1) {
      drawForecast();
    } else if (currentOK || weatherAvailable) {
      drawWeather();
    } else {
      drawError("Refresh failed");
    }
  }

  delay(30);
}
