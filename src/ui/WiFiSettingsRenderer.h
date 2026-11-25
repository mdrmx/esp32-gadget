#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

#include "../hardware/DisplayManager.h"
#include "../services/WiFiNetworkScanner.h"
#include "HeaderRenderer.h"
#include "TextRenderer.h"

// Presentation-only renderer for the on-device Wi-Fi manager. Hit-testing,
// profile persistence and connection lifecycle remain with the application.
class WiFiSettingsRenderer {
public:
  struct Colours {
    uint16_t background;
    uint16_t panel;
    uint16_t white;
    uint16_t muted;
    uint16_t accent;
    uint16_t success;
    uint16_t error;
  };

  struct ProfileRow {
    char ssid[33];
    bool preferred;
    bool active;
  };

  enum {
    ProfileRowsTop = 78,
    ProfileRowHeight = 24,
    ProfileRowsVisible = 5,
    ScanRowsTop = 76,
    ScanRowHeight = 31,
    ScanRowsVisible = 4,
    FooterTop = 207,
    FooterHeight = 26,
    DetailActionTop = 119,
    DetailActionHeight = 34,
    DetailBackTop = 207,
    PasswordFieldTop = 73,
    KeyboardX = 8,
    KeyboardY = 98,
    KeyboardKeyWidth = 28,
    KeyboardKeyHeight = 20,
    KeyboardGapX = 3,
    KeyboardGapY = 2,
    KeyboardFunctionTop = 188,
    PasswordFooterTop = 213
  };

  WiFiSettingsRenderer(
      DisplayManager& displayManager,
      HeaderRenderer& headerRenderer,
      TextRenderer& textRenderer,
      const Colours& colours)
      : displayManager_(displayManager),
        headerRenderer_(headerRenderer),
        textRenderer_(textRenderer),
        colours_(colours) {}

  void drawProfiles(
      const String& location,
      const String& clock,
      const ProfileRow* profiles,
      uint8_t count,
      const String& connectionStatus,
      const String& message) const {
    if (displayManager_.target() == nullptr) return;
    beginPage(location, clock);
    textRenderer_.drawText("WI-FI NETWORKS", 10, 43, 2, colours_.white);
    drawStatusLine(connectionStatus, message, 62);

    for (uint8_t i = 0; i < count && i < ProfileRowsVisible; ++i) {
      const int y = ProfileRowsTop + i * ProfileRowHeight;
      Arduino_GFX* target = displayManager_.target();
      target->fillRoundRect(8, y, displayManager_.width() - 16,
                            ProfileRowHeight - 2, 4, colours_.panel);

      String ssid = shortenSsid(profiles[i].ssid, 25);
      textRenderer_.drawText(ssid, 15, y + 8, 1, colours_.white);

      if (profiles[i].preferred) {
        textRenderer_.drawText("PREF", 257, y + 8, 1, colours_.accent);
      } else if (profiles[i].active) {
        textRenderer_.drawText("CURRENT", 241, y + 8, 1, colours_.success);
      }
    }

    if (count == 0) {
      textRenderer_.drawCenteredText(
          "No saved networks", 119, 1, colours_.muted);
    }

    drawButton("SCAN", 8, FooterTop, 145, FooterHeight,
               colours_.panel, colours_.accent);
    drawButton("DONE", 167, FooterTop, 145, FooterHeight,
               colours_.accent, colours_.background);
  }

  void drawScan(
      const String& location,
      const String& clock,
      const WiFiScanResult* results,
      uint8_t count,
      uint8_t page,
      bool scanning,
      bool failed,
      const String& connectionStatus,
      const String& message) const {
    if (displayManager_.target() == nullptr) return;
    beginPage(location, clock);
    textRenderer_.drawText("AVAILABLE NETWORKS", 10, 43, 2, colours_.white);

    String status;
    if (scanning) {
      status = "Scanning nearby networks...";
    } else if (failed) {
      status = "Scan failed - tap RESCAN";
    } else if (count == 0) {
      status = "No visible networks found";
    } else {
      status = String(count) + " networks found - strongest first";
    }
    if (message.length() > 0) status = message;
    drawStatusLine(connectionStatus, status, 62);

    const uint8_t first = static_cast<uint8_t>(page * ScanRowsVisible);
    for (uint8_t row = 0; row < ScanRowsVisible; ++row) {
      const uint8_t index = static_cast<uint8_t>(first + row);
      if (index >= count || results == nullptr) break;

      const int y = ScanRowsTop + row * ScanRowHeight;
      Arduino_GFX* target = displayManager_.target();
      target->fillRoundRect(8, y, displayManager_.width() - 16,
                            ScanRowHeight - 2, 4, colours_.panel);

      textRenderer_.drawText(
          shortenSsid(results[index].ssid, 27), 15, y + 5, 1,
          colours_.white);
      const char* security = !results[index].supported
          ? "UNSUPPORTED"
          : (results[index].open ? "OPEN" : "SECURE");
      textRenderer_.drawText(security, 15, y + 18, 1,
                             results[index].supported
                                 ? colours_.muted : colours_.error);
      textRenderer_.drawText(
          String(results[index].rssiDbm) + " dBm",
          247, y + 9, 1, colours_.accent);
    }

    const uint8_t pageCount = count == 0
        ? 1
        : static_cast<uint8_t>((count + ScanRowsVisible - 1) /
                               ScanRowsVisible);
    drawButton("PREV", 8, FooterTop, 54, FooterHeight,
               page > 0 ? colours_.panel : colours_.background,
               page > 0 ? colours_.white : colours_.muted);
    drawButton("SAVED", 69, FooterTop, 76, FooterHeight,
               colours_.panel, colours_.white);
    drawButton("NEXT", 152, FooterTop, 56, FooterHeight,
               page + 1 < pageCount ? colours_.panel : colours_.background,
               page + 1 < pageCount ? colours_.white : colours_.muted);
    drawButton("RESCAN", 215, FooterTop, 97, FooterHeight,
               colours_.accent, colours_.background);
  }

  void drawProfileDetail(
      const String& location,
      const String& clock,
      const char* ssid,
      bool preferred,
      bool active,
      bool connected,
      bool confirmForget,
      const String& message) const {
    if (displayManager_.target() == nullptr) return;
    beginPage(location, clock);
    textRenderer_.drawText("SAVED NETWORK", 10, 44, 2, colours_.white);
    textRenderer_.drawWrappedCenteredText(
        String(ssid != nullptr ? ssid : ""),
        displayManager_.width() / 2, displayManager_.width() - 24,
        69, 1, colours_.white);

    String status = preferred ? "Preferred network" : "Saved profile";
    if (active) status += " - selected";
    if (connected) status += " - connected";
    textRenderer_.drawCenteredText(status, 91, 1,
                                   connected ? colours_.success
                                             : colours_.muted);
    if (message.length() > 0) {
      textRenderer_.drawCenteredText(message, 105, 1, colours_.error);
    }

    if (confirmForget) {
      drawButton("CONFIRM FORGET", 167, DetailActionTop, 145,
                 DetailActionHeight, colours_.error, colours_.white);
      drawButton("CANCEL", 8, DetailActionTop, 145,
                 DetailActionHeight, colours_.panel, colours_.white);
    } else {
      drawButton("CONNECT", 8, DetailActionTop, 145,
                 DetailActionHeight, colours_.accent, colours_.background);
      drawButton("FORGET", 167, DetailActionTop, 145,
                 DetailActionHeight, colours_.panel, colours_.error);
    }

    drawButton("BACK", 8, DetailBackTop, 304, FooterHeight,
               colours_.panel, colours_.white);
  }

  void drawPassword(
      const String& location,
      const String& clock,
      const char* ssid,
      const char* password,
      bool shifted,
      bool symbols,
      const String& message) const {
    if (displayManager_.target() == nullptr) return;
    beginPage(location, clock);
    textRenderer_.drawText("ENTER PASSWORD", 10, 42, 2, colours_.white);
    textRenderer_.drawText(
        shortenSsid(ssid != nullptr ? ssid : "", 36),
        10, 59, 1, colours_.muted);

    Arduino_GFX* target = displayManager_.target();
    target->fillRoundRect(8, PasswordFieldTop, 304, 21, 4,
                          colours_.panel);
    target->drawRoundRect(8, PasswordFieldTop, 304, 21, 4,
                          colours_.accent);

    String masked;
    if (password != nullptr) {
      const size_t length = strlen(password);
      for (size_t i = 0; i < length; ++i) masked += '*';
    }
    textRenderer_.drawText(masked, 15, PasswordFieldTop + 7, 1,
                           colours_.white);
    textRenderer_.drawText(
        message.length() > 0 ? message : "8-63 characters",
        206, PasswordFieldTop + 7, 1,
        message.length() > 0 ? colours_.error : colours_.muted);

    static const char* alphaRows[] = {
      "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm.,-"
    };
    static const char* symbolRows[] = {
      "!@#$%^&*()", "-_=+[]{};:", "'\"<>?/\\|", "~`^%&+=*"
    };

    for (uint8_t row = 0; row < 4; ++row) {
      const char* keys = symbols ? symbolRows[row] : alphaRows[row];
      for (uint8_t column = 0; column < 10; ++column) {
        const char key = keys[column];
        if (key == '\0') continue;
        const int x = KeyboardX +
            column * (KeyboardKeyWidth + KeyboardGapX);
        const int y = KeyboardY +
            row * (KeyboardKeyHeight + KeyboardGapY);
        target->fillRoundRect(x, y, KeyboardKeyWidth,
                              KeyboardKeyHeight, 3, colours_.panel);
        target->drawRoundRect(x, y, KeyboardKeyWidth,
                              KeyboardKeyHeight, 3, colours_.muted);
        char label[2] = {key, '\0'};
        if (!symbols && shifted && row > 0 && key >= 'a' && key <= 'z') {
          label[0] = static_cast<char>(key - ('a' - 'A'));
        }
        textRenderer_.drawText(label, x + 10, y + 6, 1,
                               colours_.white);
      }
    }

    // The tenth cell on the home row is reserved for backspace.
    const int backspaceX = KeyboardX +
        9 * (KeyboardKeyWidth + KeyboardGapX);
    const int backspaceY = KeyboardY +
        2 * (KeyboardKeyHeight + KeyboardGapY);
    target->fillRoundRect(backspaceX, backspaceY, KeyboardKeyWidth,
                          KeyboardKeyHeight, 3, colours_.error);
    textRenderer_.drawText("<", backspaceX + 10, backspaceY + 6, 1,
                           colours_.white);

    drawButton(shifted ? "SHIFT*" : "SHIFT", 8,
               KeyboardFunctionTop, 62, 22,
               colours_.panel, colours_.white);
    drawButton("SPACE", 74, KeyboardFunctionTop, 60, 22,
               colours_.panel, colours_.white);
    drawButton("DELETE", 138, KeyboardFunctionTop, 60, 22,
               colours_.panel, colours_.white);
    drawButton(symbols ? "ABC" : "SYM", 202,
               KeyboardFunctionTop, 110, 22,
               colours_.panel, colours_.accent);

    drawButton("CANCEL", 8, PasswordFooterTop, 145, 23,
               colours_.panel, colours_.white);
    drawButton("JOIN", 167, PasswordFooterTop, 145, 23,
               colours_.accent, colours_.background);
  }

  void drawConnecting(
      const String& location,
      const String& clock,
      const char* ssid,
      bool restoring) const {
    if (displayManager_.target() == nullptr) return;
    beginPage(location, clock);
    textRenderer_.drawCenteredText(
        restoring ? "RESTORING NETWORK" : "CONNECTING",
        55, 2, colours_.white);
    textRenderer_.drawWrappedCenteredText(
        String(ssid != nullptr ? ssid : ""),
        displayManager_.width() / 2, displayManager_.width() - 28,
        90, 1, colours_.accent);
    textRenderer_.drawCenteredText(
        restoring ? "Reconnecting to the previous network"
                  : "Waiting for the access point",
        122, 1, colours_.muted);
    textRenderer_.drawCenteredText(
        "This can take up to 20 seconds",
        145, 1, colours_.muted);
  }

private:
  void beginPage(const String& location, const String& clock) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return;
    target->fillScreen(colours_.background);
    headerRenderer_.draw(
        location, clock, colours_.panel, colours_.white, colours_.accent);
    target->fillRect(
        0, 37, displayManager_.width(), displayManager_.height() - 37,
        colours_.background);
  }

  void drawStatusLine(
      const String& connectionStatus,
      const String& message,
      int y) const {
    String status = message.length() > 0 ? message : connectionStatus;
    if (status.length() > 44) status = status.substring(0, 43) + ".";
    textRenderer_.drawText(status, 10, y, 1,
                           message.length() > 0 ? colours_.accent
                                                : colours_.muted);
  }

  void drawButton(
      const char* label,
      int x,
      int y,
      int width,
      int height,
      uint16_t fill,
      uint16_t text) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return;
    target->fillRoundRect(x, y, width, height, 5, fill);
    target->drawRoundRect(x, y, width, height, 5, colours_.accent);
    const String value(label);
    const int textWidth = textRenderer_.measureTextWidth(value, 1);
    textRenderer_.drawText(
        value, x + (width - textWidth) / 2,
        y + (height - 8) / 2, 1, text);
  }

  static String shortenSsid(const char* ssid, size_t maximum) {
    String value(ssid != nullptr ? ssid : "");
    if (value.length() > maximum) {
      value = value.substring(0, maximum - 1) + ".";
    }
    return value;
  }

  DisplayManager& displayManager_;
  HeaderRenderer& headerRenderer_;
  TextRenderer& textRenderer_;
  Colours colours_;
};
