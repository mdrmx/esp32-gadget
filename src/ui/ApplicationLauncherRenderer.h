#ifndef ESP32_GADGET_APPLICATION_LAUNCHER_RENDERER_H
#define ESP32_GADGET_APPLICATION_LAUNCHER_RENDERER_H

#include <stdint.h>
#include <string.h>

#include <Arduino.h>

#include "../apps/ApplicationManager.h"
#include "../hardware/DisplayManager.h"
#include "TextRenderer.h"
#include "icons/WeatherLauncherIcon.h"

// The launcher shows a stable visual catalogue independently of the currently
// registered applications. Unregistered entries are artwork-only placeholders.
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

  enum Icon : uint8_t {
    IconWeather = 0,
    IconMap = 1,
    IconNfc = 2,
    IconPlant = 3,
    IconRemote = 4,
    IconSmartHome = 5,
    IconCount = 6
  };

  typedef void (*IconDrawCallback)(
      void* context, uint8_t icon, int x, int y);

  enum {
    HeaderHeight = 36,
    DisplayHeight = 240,
    IconTileSize = 68,
    IconTileRadius = 14,
    GridColumnGap = 20,
    GridRowGap = 20,
    GridColumns = 3,
    GridTop = 60,
    GridRowStep = IconTileSize + GridRowGap,
    MessageTop = 218
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
      const String& message = String(),
      IconDrawCallback drawIcon = nullptr,
      void* iconContext = nullptr) const {
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

    // Always draw the six catalogue entries. Registration only controls which
    // entries are selectable; placeholder artwork does not consume app slots.
    for (uint8_t icon = 0; icon < IconCount; ++icon) {
      const int x = tileLeft(icon, width);
      const int y = tileTop(icon);
      target->fillRoundRect(
          x, y, IconTileSize, IconTileSize, IconTileRadius,
          colours_.background);
      target->drawRoundRect(
          x, y, IconTileSize, IconTileSize, IconTileRadius,
          colours_.text);
      drawApplicationIcon(target, icon, x, y, drawIcon, iconContext);
    }

    // Keep the normal launcher completely label-free. A failed app start
    // may still show a short diagnostic so the user knows the tap was handled.
    if (message.length() > 0) {
      textRenderer_.drawCenteredText(
          message, MessageTop, 1, colours_.accent);
    }

    (void)manager;
  }

  // A visual catalogue tile is a hit target only when an application with its
  // stable id is registered. This keeps the artwork-only placeholders inert.
  bool applicationAt(
      int x,
      int y,
      uint8_t& applicationIndex,
      const ApplicationManager& manager) const {
    for (uint8_t icon = 0; icon < IconCount; ++icon) {
      uint8_t registeredIndex = 0;
      if (!registeredIndexForIcon(icon, manager, registeredIndex)) {
        continue;
      }

      const int left = tileLeft(icon, displayManager_.width());
      const int top = tileTop(icon);
      if (x >= left && x < left + IconTileSize &&
          y >= top && y < top + IconTileSize) {
        applicationIndex = registeredIndex;
        return true;
      }
    }
    return false;
  }

 private:
  DisplayManager& displayManager_;
  TextRenderer& textRenderer_;
  Colours colours_;

  static const char* applicationIdForIcon(uint8_t icon) {
    switch (icon) {
      case IconWeather: return "weather";
      case IconMap: return "map";
      case IconNfc: return "nfc";
      case IconPlant: return "plant";
      case IconRemote: return "remote";
      case IconSmartHome: return "smart-home";
      default: return nullptr;
    }
  }

  static bool registeredIndexForIcon(
      uint8_t icon,
      const ApplicationManager& manager,
      uint8_t& registeredIndex) {
    const char* applicationId = applicationIdForIcon(icon);
    if (applicationId == nullptr) {
      return false;
    }

    for (uint8_t i = 0; i < manager.registeredCount(); ++i) {
      const Application* application = manager.applicationAt(i);
      if (application == nullptr || application->id() == nullptr) {
        continue;
      }
      if (strcmp(application->id(), applicationId) == 0) {
        registeredIndex = i;
        return true;
      }
    }
    return false;
  }

  static int tileLeft(uint8_t index, int displayWidth) {
    const int gridWidth =
        (IconTileSize * GridColumns) +
        (GridColumnGap * (GridColumns - 1));
    const int sideMargin = (displayWidth - gridWidth) / 2;
    const int column = index % GridColumns;
    return sideMargin + column * (IconTileSize + GridColumnGap);
  }

  static int tileTop(uint8_t index) {
    return GridTop + (index / GridColumns) * GridRowStep;
  }

  void drawApplicationIcon(
      Arduino_GFX* target,
      uint8_t icon,
      int tileX,
      int tileY,
      IconDrawCallback drawIcon,
      void* iconContext) const {
    if (icon == IconWeather) {
      const int iconX =
          tileX + (IconTileSize - WeatherLauncherIcon::Width) / 2;
      const int iconY =
          tileY + (IconTileSize - WeatherLauncherIcon::Height) / 2;
      target->drawBitmap(
          iconX, iconY, WeatherLauncherIcon::Bitmap,
          WeatherLauncherIcon::Width, WeatherLauncherIcon::Height,
          colours_.text);
      return;
    }

    if (drawIcon != nullptr) {
      // The 64x64 PNG artwork sits inside a two-pixel inset in each tile.
      drawIcon(iconContext, icon, tileX + 2, tileY + 2);
      return;
    }

    // Host-only/failure fallback if no PNG drawer is supplied.
    const int centreX = tileX + IconTileSize / 2;
    const int centreY = tileY + IconTileSize / 2;
    target->drawFastHLine(centreX - 10, centreY, 20, colours_.text);
    target->drawFastVLine(centreX, centreY - 10, 20, colours_.text);
  }
};

#endif  // ESP32_GADGET_APPLICATION_LAUNCHER_RENDERER_H
