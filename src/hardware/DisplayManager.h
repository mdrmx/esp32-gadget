#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Owns the ST7789 display, its render target and the Weather app's current
// off-screen canvases. Drawing and interaction remain in weatherDashboard.ino.
class DisplayManager {
public:
  DisplayManager() = default;

  DisplayManager(const DisplayManager&) = delete;
  DisplayManager& operator=(const DisplayManager&) = delete;

  ~DisplayManager() {
    releaseResources();
  }

  bool begin() {
    // Keep begin() safe to call again after successful initialisation.
    if (display_ != nullptr) return true;

    // Clean up any partial state left by a previous failed initialisation.
    releaseResources();

    bus_ = new Arduino_ESP32SPI(
      LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO);
    if (bus_ == nullptr) return false;

    display_ = new Arduino_ST7789(
      bus_, LCD_RST, LCD_ROTATION, true, LCD_WIDTH, LCD_HEIGHT);
    if (display_ == nullptr) {
      releaseResources();
      return false;
    }

    if (!display_->begin()) {
      releaseResources();
      return false;
    }

    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);

    width_ = display_->width();
    height_ = display_->height();
    renderTarget_ = display_;
    return true;
  }

  bool createForecastCanvas(int width, int height, int x, int y) {
    return createCanvas(forecastCanvas_, width, height, x, y);
  }

  bool createWeatherMetricsCanvas(int width, int height, int x, int y) {
    return createCanvas(weatherMetricsCanvas_, width, height, x, y);
  }

  bool createTabTransitionCanvas(int width, int height) {
    return createCanvas(tabTransitionCanvas_, width, height, 0, 0);
  }

  int width() const { return width_; }
  int height() const { return height_; }

  // target() is the physical display normally and the transition canvas only
  // while a tab transition is being composed.
  Arduino_GFX* target() const { return renderTarget_; }
  Arduino_GFX* physicalDisplay() const { return display_; }

  Arduino_Canvas* forecastCanvas() const { return forecastCanvas_; }
  Arduino_Canvas* weatherMetricsCanvas() const { return weatherMetricsCanvas_; }
  Arduino_Canvas* tabTransitionCanvas() const { return tabTransitionCanvas_; }

  bool renderingTabTransition() const { return renderingTabTransition_; }

  void beginTabTransitionRender() {
    renderingTabTransition_ = true;
    renderTarget_ = tabTransitionCanvas_;
  }

  void endTabTransitionRender() {
    renderTarget_ = display_;
    renderingTabTransition_ = false;
  }

private:
  enum {
    LCD_SCLK = 39,
    LCD_MOSI = 38,
    LCD_MISO = 40,
    LCD_DC = 42,
    LCD_RST = -1,
    LCD_CS = 45,
    LCD_BL = 1,
    LCD_ROTATION = 1,
    LCD_WIDTH = 240,
    LCD_HEIGHT = 320
  };

  void releaseCanvas(Arduino_Canvas*& canvas) {
    delete canvas;
    canvas = nullptr;
  }

  // Release dependants before the objects they reference. Arduino_Canvas
  // keeps a non-owning output pointer to the physical display.
  void releaseResources() {
    renderTarget_ = nullptr;
    renderingTabTransition_ = false;

    releaseCanvas(forecastCanvas_);
    releaseCanvas(weatherMetricsCanvas_);
    releaseCanvas(tabTransitionCanvas_);

    // Keep concrete pointer types here: Arduino_GFX and Arduino_DataBus do
    // not provide virtual destructors in the library API.
    delete display_;
    display_ = nullptr;

    delete bus_;
    bus_ = nullptr;

    width_ = 320;
    height_ = 240;
  }

  bool createCanvas(
    Arduino_Canvas*& canvas,
    int width,
    int height,
    int x,
    int y
  ) {
    // Replacing a buffer must release its previous framebuffer first.
    releaseCanvas(canvas);

    if (display_ == nullptr) return false;

    canvas = new Arduino_Canvas(width, height, display_, x, y, 0);
    if (canvas == nullptr || !canvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
      releaseCanvas(canvas);
      return false;
    }
    return true;
  }

  // These concrete types are important for safe deletion: the library's
  // Arduino_GFX / Arduino_DataBus base classes do not have virtual destructors.
  Arduino_ESP32SPI* bus_ = nullptr;
  Arduino_ST7789* display_ = nullptr;
  Arduino_GFX* renderTarget_ = nullptr;

  Arduino_Canvas* forecastCanvas_ = nullptr;
  Arduino_Canvas* weatherMetricsCanvas_ = nullptr;
  Arduino_Canvas* tabTransitionCanvas_ = nullptr;

  int width_ = 320;
  int height_ = 240;
  bool renderingTabTransition_ = false;
};
