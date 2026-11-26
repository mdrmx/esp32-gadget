#ifndef ESP32_GADGET_APPLICATION_LAUNCHER_RENDERER_H
#define ESP32_GADGET_APPLICATION_LAUNCHER_RENDERER_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <Arduino.h>

#include "../apps/ApplicationManager.h"
#include "../hardware/DisplayManager.h"
#include "TextRenderer.h"

// Compact two-column launcher for the manager's fixed maximum of four apps.
// It renders app ids as labels; friendly names/icons can be added when a
// second application introduces a real metadata requirement.
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
    Margin = 8,
    ColumnGap = 8,
    CardTop = 46,
    CardHeight = 66,
    RowGap = 8,
    FooterTop = 210,
    BackLeft = 8,
    BackRight = 84,
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
      const char* returnApplicationId,
      const String& message = String()) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) {
      return;
    }

    const int width = displayManager_.width();
    const int cardWidth = (width - (Margin * 2) - ColumnGap) / 2;
    target->fillScreen(colours_.background);
    target->fillRect(0, 0, width, HeaderHeight, colours_.panel);
    textRenderer_.drawText("APPLICATIONS", Margin, 5, 2, colours_.text);

    char countLabel[24];
    snprintf(
        countLabel,
        sizeof(countLabel),
        "%u AVAILABLE",
        static_cast<unsigned int>(manager.registeredCount()));
    textRenderer_.drawText(countLabel, Margin, 24, 1, colours_.muted);
    target->drawFastHLine(0, HeaderHeight, width, colours_.accent);

    const uint8_t count = manager.registeredCount() <
            MaximumVisibleApplications
        ? manager.registeredCount()
        : MaximumVisibleApplications;

    if (count == 0) {
      textRenderer_.drawCenteredText(
          "NO APPLICATIONS", 98, 2, colours_.muted);
    }

    for (uint8_t i = 0; i < count; ++i) {
      const Application* application = manager.applicationAt(i);
      if (application == nullptr || application->id() == nullptr ||
          application->id()[0] == '\0') {
        continue;
      }

      const int x = cardLeft(i, width, cardWidth);
      const int y = cardTop(i);
      const bool isReturnApplication =
          returnApplicationId != nullptr &&
          strcmp(returnApplicationId, application->id()) == 0;

      target->fillRoundRect(
          x, y, cardWidth, CardHeight, 8, colours_.panel);
      target->drawRoundRect(
          x, y, cardWidth, CardHeight, 8,
          isReturnApplication ? colours_.selected : colours_.muted);

      String label = makeLabel(application->id(), cardWidth - 22);
      textRenderer_.drawText(
          label, x + 11, y + 15, 2, colours_.text);
      textRenderer_.drawText(
          isReturnApplication ? "LAST USED" : "OPEN",
          x + 11, y + 43, 1,
          isReturnApplication ? colours_.selected : colours_.muted);
    }

    target->fillRoundRect(
        BackLeft, FooterTop, BackRight - BackLeft, 24, 6,
        colours_.panel);
    target->drawRoundRect(
        BackLeft, FooterTop, BackRight - BackLeft, 24, 6,
        colours_.accent);
    textRenderer_.drawText("BACK", BackLeft + 18, FooterTop + 7, 1,
                           colours_.text);

    const String footer = message.length() > 0
        ? message
        : String("SELECT AN APP");
    textRenderer_.drawText(footer, BackRight + 8, FooterTop + 8, 1,
                           message.length() > 0 ? colours_.selected
                                               : colours_.muted);
  }

  bool applicationAt(
      int x,
      int y,
      uint8_t& applicationIndex,
      const ApplicationManager& manager) const {
    const int width = displayManager_.width();
    const int cardWidth = (width - (Margin * 2) - ColumnGap) / 2;
    const uint8_t count = manager.registeredCount() <
            MaximumVisibleApplications
        ? manager.registeredCount()
        : MaximumVisibleApplications;

    for (uint8_t i = 0; i < count; ++i) {
      const Application* application = manager.applicationAt(i);
      if (application == nullptr || application->id() == nullptr ||
          application->id()[0] == '\0') {
        continue;
      }

      const int left = cardLeft(i, width, cardWidth);
      const int top = cardTop(i);
      if (x >= left && x < left + cardWidth &&
          y >= top && y < top + CardHeight) {
        applicationIndex = i;
        return true;
      }
    }
    return false;
  }

  bool backButtonContains(int x, int y) const {
    return x >= BackLeft && x < BackRight &&
           y >= FooterTop && y < FooterTop + 24;
  }

 private:
  DisplayManager& displayManager_;
  TextRenderer& textRenderer_;
  Colours colours_;

  static int cardLeft(uint8_t index, int width, int cardWidth) {
    return Margin + (index % 2) * (cardWidth + ColumnGap);
  }

  static int cardTop(uint8_t index) {
    return CardTop + (index / 2) * (CardHeight + RowGap);
  }

  String makeLabel(const char* applicationId, int maximumWidth) const {
    char labelBuffer[32];
    strncpy(labelBuffer, applicationId, sizeof(labelBuffer) - 1);
    labelBuffer[sizeof(labelBuffer) - 1] = '\0';

    for (size_t i = 0; labelBuffer[i] != '\0'; ++i) {
      if (labelBuffer[i] >= 'a' && labelBuffer[i] <= 'z') {
        labelBuffer[i] = static_cast<char>(
            labelBuffer[i] - 'a' + 'A');
      }
    }

    const size_t originalLength = strlen(labelBuffer);
    String label(labelBuffer);
    while (label.length() > 0 &&
           textRenderer_.measureTextWidth(label, 2) > maximumWidth) {
      label.remove(label.length() - 1, 1);
    }

    if (label.length() < originalLength && label.length() > 0) {
      while (label.length() > 0 &&
             textRenderer_.measureTextWidth(label + ".", 2) > maximumWidth) {
        label.remove(label.length() - 1, 1);
      }
      if (label.length() > 0) {
        label += ".";
      }
    }
    return label;
  }
};

#endif  // ESP32_GADGET_APPLICATION_LAUNCHER_RENDERER_H
