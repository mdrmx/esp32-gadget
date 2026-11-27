#ifndef ESP32_GADGET_APPLICATION_LAUNCHER_RENDERER_H
#define ESP32_GADGET_APPLICATION_LAUNCHER_RENDERER_H

#include <stdint.h>
#include <string.h>

#include <Arduino.h>

#include "../apps/ApplicationManager.h"
#include "../hardware/DisplayManager.h"
#include "TextRenderer.h"
#include "icons/WeatherLauncherIcon.h"

// Compact, mobile-style icon launcher for the manager's fixed maximum of four
// apps. The Weather icon is a flash-resident monochrome bitmap; other apps use
// a simple built-in fallback glyph until they provide their own artwork.
class ApplicationLauncherRenderer {
 public:
  struct Colours {
    uint16_t background;
    uint16_t panel;
    uint16_t text;
    uint16_t muted;
    uint16_t accent;
    uint16_t selected;
  };

  enum {
    HeaderHeight = 36,
    DisplayHeight = 240,
    IconTileSize = 68,
    IconTileRadius = 14,
    GridColumnGap = 72,
    GridTop = 64,
    GridRowStep = 80,
    MessageTop = 218,
    MaximumVisibleApplications = 4
  };

  ApplicationLauncherRenderer(
      DisplayManager& displayManager,
      TextRenderer& textRenderer,
      const Colours& colours)
      : displayManager_(displayManager),
        textRenderer_(textRenderer),
        colours_(colours) {}

  void draw(
      const ApplicationManager& manager,
      const char* /*returnApplicationId*/,
      const String& message = String()) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) {
      return;
    }

    const int width = displayManager_.width();
    target->fillScreen(colours_.background);
    target->fillRect(0, 0, width, HeaderHeight, colours_.background);
    textRenderer_.drawText("APPLICATIONS", 8, 10, 2, colours_.text);
    target->drawFastHLine(0, HeaderHeight, width, colours_.text);
    target->drawFastHLine(0, HeaderHeight + 1, width, colours_.text);

    const uint8_t count = visibleApplicationCount(manager);
    if (count == 0) {
      textRenderer_.drawCenteredText(
          "NO APPLICATIONS", 98, 2, colours_.muted);
    }

    for (uint8_t i = 0; i < count; ++i) {
      const Application* application = manager.applicationAt(i);
      if (!isValidApplication(application)) {
        continue;
      }

      const int x = tileLeft(i, count, width);
      const int y = tileTop(i, count);
      target->fillRoundRect(
          x, y, IconTileSize, IconTileSize, IconTileRadius,
          colours_.background);
      target->drawRoundRect(
          x, y, IconTileSize, IconTileSize, IconTileRadius,
          colours_.text);
      drawApplicationIcon(target, application->id(), x, y);
    }

    // Keep the normal launcher completely label-free. A failed app start
    // may still show a short diagnostic so the user knows the tap was handled.
    if (message.length() > 0) {
      textRenderer_.drawCenteredText(
          message, MessageTop, 1, colours_.accent);
    }
  }

  bool applicationAt(
      int x,
      int y,
      uint8_t& applicationIndex,
      const ApplicationManager& manager) const {
    const uint8_t count = visibleApplicationCount(manager);
    const int width = displayManager_.width();

    for (uint8_t i = 0; i < count; ++i) {
      if (!isValidApplication(manager.applicationAt(i))) {
        continue;
      }

      const int left = tileLeft(i, count, width);
      const int top = tileTop(i, count);
      if (x >= left && x < left + IconTileSize &&
          y >= top && y < top + IconTileSize) {
        applicationIndex = i;
        return true;
      }
    }
    return false;
  }

 private:
  DisplayManager& displayManager_;
  TextRenderer& textRenderer_;
  Colours colours_;

  static uint8_t visibleApplicationCount(
      const ApplicationManager& manager) {
    return manager.registeredCount() < MaximumVisibleApplications
        ? manager.registeredCount()
        : MaximumVisibleApplications;
  }

  static bool isValidApplication(const Application* application) {
    return application != nullptr &&
           application->id() != nullptr &&
           application->id()[0] != '\0';
  }

  static int tileLeft(uint8_t index, uint8_t count, int displayWidth) {
    if (count == 1) {
      return (displayWidth - IconTileSize) / 2;
    }
    const int sideMargin =
        (displayWidth - (IconTileSize * 2) - GridColumnGap) / 2;
    return sideMargin + (index % 2) * (IconTileSize + GridColumnGap);
  }

  static int tileTop(uint8_t index, uint8_t count) {
    if (count <= 2) {
      // Centre a single icon or single row of two icons in the full body
      // area below the fixed header. Keep this independent of a footer.
      return (HeaderHeight + DisplayHeight - IconTileSize) / 2;
    }
    return GridTop + (index / 2) * GridRowStep;
  }

  void drawApplicationIcon(
      Arduino_GFX* target, const char* applicationId, int tileX, int tileY
  ) const {
    const int iconX =
        tileX + (IconTileSize - WeatherLauncherIcon::Width) / 2;
    const int iconY =
        tileY + (IconTileSize - WeatherLauncherIcon::Height) / 2;

    if (strcmp(applicationId, "weather") == 0) {
      target->drawBitmap(
          iconX, iconY, WeatherLauncherIcon::Bitmap,
          WeatherLauncherIcon::Width, WeatherLauncherIcon::Height,
          colours_.text);
      return;
    }

    // Neutral placeholder glyph for apps without a registered bitmap yet.
    const int centreX = tileX + IconTileSize / 2;
    const int centreY = tileY + IconTileSize / 2;
    target->drawFastHLine(centreX - 10, centreY, 20, colours_.text);
    target->drawFastVLine(centreX, centreY - 10, 20, colours_.text);
  }
};

#endif  // ESP32_GADGET_APPLICATION_LAUNCHER_RENDERER_H
