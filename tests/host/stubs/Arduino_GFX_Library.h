#pragma once

#include <Arduino.h>
#include <string.h>

#ifndef GFX_SKIP_OUTPUT_BEGIN
#define GFX_SKIP_OUTPUT_BEGIN 1
#endif

class Arduino_GFX {
public:
  virtual ~Arduino_GFX() = default;

  virtual int width() const { return width_; }
  virtual int height() const { return height_; }
  virtual bool begin() { return true; }
  virtual bool begin(uint8_t) { return true; }

  void setTextSize(uint8_t size) { textSize = size; }
  void setTextColor(uint16_t colour) { textColour = colour; }
  void setCursor(int x, int y) {
    cursorX = x;
    cursorY = y;
  }

  void print(const String& text) {
    lastPrinted = text;
    if (printCount < MAX_RECORDED_PRINTS) {
      printedText[printCount] = text;
      printedX[printCount] = cursorX;
      printedY[printCount] = cursorY;
      printedSize[printCount] = textSize;
      printedColour[printCount] = textColour;
    }
    ++printCount;
  }

  void fillScreen(uint16_t colour) {
    ++fillScreenCount;
    lastFillScreenColour = colour;
  }

  void fillRect(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    uint16_t colour
  ) {
    ++fillRectCount;
    lastFillRectX = x;
    lastFillRectY = y;
    lastFillRectWidth = width;
    lastFillRectHeight = height;
    lastFillRectColour = colour;
  }

  void drawFastHLine(
    int16_t x,
    int16_t y,
    int16_t width,
    uint16_t colour
  ) {
    ++horizontalLineCount;
    lastLineX = x;
    lastLineY = y;
    lastLineWidth = width;
    lastLineColour = colour;
  }

  void getTextBounds(
    const char* text,
    int16_t x,
    int16_t y,
    int16_t* x1,
    int16_t* y1,
    uint16_t* width,
    uint16_t* height
  ) {
    if (x1 != nullptr) *x1 = x;
    if (y1 != nullptr) *y1 = y;
    const size_t length = text != nullptr ? strlen(text) : 0;
    if (width != nullptr) {
      *width = static_cast<uint16_t>(
        length * static_cast<size_t>(6U * textSize));
    }
    if (height != nullptr) {
      *height = static_cast<uint16_t>(8U * textSize);
    }
  }

  uint8_t textSize = 1;
  uint16_t textColour = 0;
  int cursorX = 0;
  int cursorY = 0;
  String lastPrinted;
  int printCount = 0;

  static const int MAX_RECORDED_PRINTS = 16;
  String printedText[MAX_RECORDED_PRINTS];
  int printedX[MAX_RECORDED_PRINTS] = {};
  int printedY[MAX_RECORDED_PRINTS] = {};
  uint8_t printedSize[MAX_RECORDED_PRINTS] = {};
  uint16_t printedColour[MAX_RECORDED_PRINTS] = {};

  int fillScreenCount = 0;
  uint16_t lastFillScreenColour = 0;

  int fillRectCount = 0;
  int lastFillRectX = 0;
  int lastFillRectY = 0;
  int lastFillRectWidth = 0;
  int lastFillRectHeight = 0;
  uint16_t lastFillRectColour = 0;

  int horizontalLineCount = 0;
  int lastLineX = 0;
  int lastLineY = 0;
  int lastLineWidth = 0;
  uint16_t lastLineColour = 0;

protected:
  Arduino_GFX(int width = 320, int height = 240)
    : width_(width), height_(height) {}

private:
  int width_;
  int height_;
};

class Arduino_ESP32SPI {
public:
  Arduino_ESP32SPI(int, int, int, int, int) {}
};

class Arduino_ST7789 : public Arduino_GFX {
public:
  Arduino_ST7789(
    Arduino_ESP32SPI*,
    int,
    int rotation,
    bool,
    int width,
    int height
  )
    : Arduino_GFX(rotation ? height : width, rotation ? width : height) {}
};

class Arduino_Canvas : public Arduino_GFX {
public:
  Arduino_Canvas(int width, int height, Arduino_GFX*, int, int, int)
    : Arduino_GFX(width, height) {}
};
