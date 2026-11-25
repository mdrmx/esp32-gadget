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

  enum class Availability : uint8_t {
    Unknown = 0,
    Checking,
    Available,
    Unavailable
  };

  struct ProfileRow {
    char ssid[33];
    bool preferred;
    bool active;
    Availability availability;
  };

  enum {
    // Saved profiles and scan results share one list geometry so switching
    // between the two screens does not change row scale or spacing.
    ListRowsTop = 72,
    ListRowHeight = 43,
    ListRowsVisible = 3,
    // A row can move upward by at most ListRowHeight - 1 px before
    // the next row becomes the first item in the list.
    ListRowsMaskTop = ListRowsTop - ListRowHeight + 1,
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
      int scrollPositionPixels,
      const String& connectionStatus,
      const String& message) const {
    if (displayManager_.target() == nullptr) return;
    beginPage(location, clock);
    textRenderer_.drawText("WI-FI NETWORKS", 10, 46, 2, colours_.white);
    drawStatusLine(connectionStatus, message, 63);

    const int first = scrollPositionPixels / ListRowHeight;
    const int rowOffset = scrollPositionPixels % ListRowHeight;
    for (uint8_t row = 0; row <= ListRowsVisible; ++row) {
      const int index = first + row;
      if (index < 0 || index >= count || profiles == nullptr) continue;

      const int y = ListRowsTop + row * ListRowHeight - rowOffset;
      Arduino_GFX* target = displayManager_.target();
      const int cardWidth = displayManager_.width() - 16;
      const int cardHeight = ListRowHeight - 2;
      target->fillRoundRect(
          8, y, cardWidth, cardHeight, 4, colours_.panel);

      // The preferred/active profile is identified by the card outline rather
      // than a text label, leaving the SSID and availability state uncluttered.
      if (profiles[index].preferred || profiles[index].active) {
        target->drawRoundRect(
            8, y, cardWidth, cardHeight, 4, colours_.accent);
      }

      // Centre the size-2 SSID and availability badge on the same horizontal
      // line. Reserve the badge's width before shortening long SSIDs.
      const int availabilityX = 214;
      const int availabilityWidth = 98;
      const int ssidX = 15;
      const int ssidMaxWidth = availabilityX - ssidX - 8;
      const String ssid = shortenSsidToWidth(
          profiles[index].ssid, 2, ssidMaxWidth);
      const int nameHeight = textRenderer_.measureTextHeight(ssid, 2);
      const int nameTop = y + (cardHeight - nameHeight) / 2;
      textRenderer_.drawText(ssid, ssidX, nameTop, 2, colours_.white);

      const int availabilityHeight = 15;
      const int availabilityTop =
          y + (cardHeight - availabilityHeight) / 2;
      drawAvailabilityTag(
          profiles[index].availability, availabilityX, availabilityTop,
          availabilityWidth, availabilityHeight);
    }

    Arduino_GFX* target = displayManager_.target();
    if (target != nullptr) {
      target->fillRect(
          0, ListRowsMaskTop, displayManager_.width(),
          ListRowsTop - ListRowsMaskTop, colours_.background);
      target->fillRect(
          0, ListRowsTop + ListRowsVisible * ListRowHeight,
          displayManager_.width(),
          displayManager_.height() -
              (ListRowsTop + ListRowsVisible * ListRowHeight),
          colours_.background);
    }

    // Restore the fixed header after clipping list rows at either edge.
    headerRenderer_.draw(
        location, clock, colours_.panel, colours_.white, colours_.accent);
    textRenderer_.drawText("WI-FI NETWORKS", 10, 46, 2, colours_.white);
    drawStatusLine(connectionStatus, message, 63);

    if (count == 0) {
      textRenderer_.drawCenteredText(
          "No saved networks", 132, 1, colours_.muted);
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
      int scrollPositionPixels,
      bool scanning,
      bool failed,
      const String& connectionStatus,
      const String& message) const {
    if (displayManager_.target() == nullptr) return;
    beginPage(location, clock);
    textRenderer_.drawText("AVAILABLE NETWORKS", 10, 46, 2, colours_.white);

    String status;
    bool showStatus = false;
    if (scanning) {
      status = "Scanning nearby networks...";
      showStatus = true;
    } else if (failed) {
      status = "Scan failed - tap RESCAN";
      showStatus = true;
    } else if (count == 0) {
      status = "No visible networks found";
      showStatus = true;
    }
    if (message.length() > 0) {
      status = message;
      showStatus = true;
    }
    if (showStatus) drawStatusLine(connectionStatus, status, 63);

    // Keep a partial row in view while the touch position follows the finger.
    // The sketch bounds scrollPositionPixels to one resisted row overshoot.
    const int first = scrollPositionPixels / ListRowHeight;
    const int rowOffset = scrollPositionPixels % ListRowHeight;
    for (uint8_t row = 0; row <= ListRowsVisible; ++row) {
      const int index = first + row;
      if (index < 0 || index >= count || results == nullptr) continue;

      const int y =
          ListRowsTop + row * ListRowHeight - rowOffset;
      Arduino_GFX* target = displayManager_.target();
      target->fillRoundRect(8, y, displayManager_.width() - 16,
                            ListRowHeight - 2, 4, colours_.panel);

      const bool requiresPassword = !results[index].open;
      drawScanNetworkName(results[index].ssid, y, requiresPassword);
      if (!results[index].supported) {
        // Preserve the explicit warning for security modes this manager
        // cannot join; the lock remains beside the SSID when a password is used.
        textRenderer_.drawText(
            "UNSUPPORTED", 15, y + ListRowHeight - 12, 1, colours_.error);
      }
      drawSignalStrengthBars(results[index].rssiDbm, y);
    }

    // Rows may move beyond the viewport while the finger pulls against an
    // edge. Mask those portions, then restore the fixed title/status/footer.
    Arduino_GFX* target = displayManager_.target();
    if (target != nullptr) {
      target->fillRect(
          0, ListRowsMaskTop, displayManager_.width(),
          ListRowsTop - ListRowsMaskTop, colours_.background);
      target->fillRect(
          0, ListRowsTop + ListRowsVisible * ListRowHeight,
          displayManager_.width(),
          displayManager_.height() -
              (ListRowsTop + ListRowsVisible * ListRowHeight),
          colours_.background);
    }

    // The moving first row can reach the header divider during a drag.
    // Restore the complete header after clipping the overflow, so the blue
    // separator and Wi-Fi indicator never retain pixels from a scan row.
    headerRenderer_.draw(
        location, clock, colours_.panel, colours_.white, colours_.accent);
    textRenderer_.drawText("AVAILABLE NETWORKS", 10, 46, 2, colours_.white);
    if (showStatus) drawStatusLine(connectionStatus, status, 63);

    drawButton("SAVED", 8, FooterTop, 145, FooterHeight,
               colours_.panel, colours_.white);
    drawButton("RESCAN", 167, FooterTop, 145, FooterHeight,
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
      bool showPassword,
      int highlightedRow,
      int highlightedColumn,
      const String& message) const {
    if (displayManager_.target() == nullptr) return;
    beginPage(location, clock);
    textRenderer_.drawText("ENTER PASSWORD", 10, 42, 2, colours_.white);
    textRenderer_.drawText(
        shortenSsid(ssid != nullptr ? ssid : "", 30),
        10, 59, 1, colours_.muted);
    if (message.length() > 0) {
      textRenderer_.drawText(
          shortenSsid(message.c_str(), 17), 214, 59, 1, colours_.error);
    } else {
      textRenderer_.drawText("8-63 chars", 248, 59, 1, colours_.muted);
    }

    Arduino_GFX* target = displayManager_.target();
    target->fillRoundRect(8, PasswordFieldTop, 205, 21, 4,
                          colours_.panel);
    target->drawRoundRect(8, PasswordFieldTop, 205, 21, 4,
                          colours_.accent);

    String visible;
    if (password != nullptr) {
      if (showPassword) {
        visible = String(password);
      } else {
        const size_t length = strlen(password);
        for (size_t i = 0; i < length; ++i) visible += '*';
      }
    }
    if (visible.length() > 31) {
      visible = "..." + visible.substring(visible.length() - 28);
    }
    textRenderer_.drawText(visible, 15, PasswordFieldTop + 7, 1,
                           colours_.white);
    drawButton(showPassword ? "HIDE" : "SHOW", 220,
               PasswordFieldTop, 92, 21,
               colours_.panel, colours_.accent);

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
        const bool highlighted =
            row == highlightedRow && column == highlightedColumn;
        target->fillRoundRect(
            x, y, KeyboardKeyWidth, KeyboardKeyHeight, 3,
            highlighted ? colours_.accent : colours_.panel);
        target->drawRoundRect(
            x, y, KeyboardKeyWidth, KeyboardKeyHeight, 3,
            highlighted ? colours_.white : colours_.muted);
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
    const bool backspaceHighlighted =
        highlightedRow == 2 && highlightedColumn == 9;
    target->fillRoundRect(
        backspaceX, backspaceY, KeyboardKeyWidth, KeyboardKeyHeight, 3,
        backspaceHighlighted ? colours_.accent : colours_.error);
    target->drawRoundRect(
        backspaceX, backspaceY, KeyboardKeyWidth, KeyboardKeyHeight, 3,
        backspaceHighlighted ? colours_.white : colours_.accent);
    textRenderer_.drawText("<", backspaceX + 10, backspaceY + 6, 1,
                           colours_.white);

    drawButton(shifted ? "SHIFT*" : "SHIFT", 8,
               KeyboardFunctionTop, 62, 22,
               colours_.panel, colours_.white,
               highlightedRow == 4 && highlightedColumn == 0);
    drawButton("SPACE", 74, KeyboardFunctionTop, 60, 22,
               colours_.panel, colours_.white,
               highlightedRow == 4 && highlightedColumn == 1);
    drawButton("DELETE", 138, KeyboardFunctionTop, 60, 22,
               colours_.panel, colours_.white,
               highlightedRow == 4 && highlightedColumn == 2);
    drawButton(symbols ? "ABC" : "SYM", 202,
               KeyboardFunctionTop, 110, 22,
               colours_.panel, colours_.accent,
               highlightedRow == 4 && highlightedColumn == 3);

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
  // Fit the SSID into the space left of the signal bars and, when needed,
  // a lock glyph. Width measurement avoids clipping with variable SSID text.
  void drawScanNetworkName(
      const char* ssid,
      int rowY,
      bool requiresPassword) const {
    const int textX = 15;
    const int signalBarsLeft = displayManager_.width() - 42;
    const int lockWidth = requiresPassword ? 14 : 0;
    const int lockGap = requiresPassword ? 4 : 0;
    const int maximumTextWidth =
        signalBarsLeft - 4 - textX - lockWidth - lockGap;

    String label(ssid != nullptr ? ssid : "");
    bool shortened = false;
    while (label.length() > 0 &&
           textRenderer_.measureTextWidth(label, 2) > maximumTextWidth) {
      label.remove(label.length() - 1);
      shortened = true;
    }
    if (shortened) {
      while (label.length() > 0 &&
             textRenderer_.measureTextWidth(label + ".", 2) >
                 maximumTextWidth) {
        label.remove(label.length() - 1);
      }
      label += ".";
    }

    const int cardHeight = ListRowHeight - 2;
    const int textHeight = textRenderer_.measureTextHeight(label, 2);
    const int contentTop = rowY + (cardHeight - textHeight) / 2;
    textRenderer_.drawText(label, textX, contentTop, 2, colours_.white);
    if (requiresPassword && label.length() > 0) {
      const int lockX =
          textX + textRenderer_.measureTextWidth(label, 2) + lockGap;
      drawSecurityIcon(lockX, contentTop, colours_.muted);
    }
  }

  // Compact closed padlock drawn directly with display primitives.
  // Open networks do not call this helper and therefore have no icon.
  void drawSecurityIcon(int x, int y, uint16_t colour) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return;

    target->drawRoundRect(x + 3, y + 1, 8, 10, 4, colour);
    target->fillRoundRect(x, y + 7, 14, 9, 2, colour);
    target->drawLine(x + 7, y + 10, x + 7, y + 13,
                     colours_.background);
  }

  // Four ascending bars use the same RSSI thresholds as the header icon.
  // A muted bar remains visible below the current signal level.
  void drawSignalStrengthBars(int rssiDbm, int rowY) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return;

    uint8_t activeBars = 0;
    if (rssiDbm >= -55) {
      activeBars = 4;
    } else if (rssiDbm >= -67) {
      activeBars = 3;
    } else if (rssiDbm >= -75) {
      activeBars = 2;
    } else if (rssiDbm >= -85) {
      activeBars = 1;
    }

    const int barHeights[] = {5, 9, 13, 17};
    const int barWidth = 4;
    const int barGap = 3;
    const int cardHeight = ListRowHeight - 2;
    const int tallestBar = barHeights[3];
    const int baseline =
        rowY + (cardHeight - tallestBar) / 2 + tallestBar;
    const int startX = displayManager_.width() - 42;
    for (uint8_t bar = 0; bar < 4; ++bar) {
      const uint16_t colour = bar < activeBars
          ? colours_.accent : colours_.muted;
      target->fillRoundRect(
          startX + bar * (barWidth + barGap),
          baseline - barHeights[bar],
          barWidth, barHeights[bar], 1, colour);
    }
  }

  void drawAvailabilityTag(
      Availability availability,
      int x,
      int y,
      int width,
      int height) const {
    const char* label = "UNKNOWN";
    uint16_t fill = colours_.panel;
    uint16_t text = colours_.muted;
    switch (availability) {
      case Availability::Checking:
        label = "CHECKING";
        fill = colours_.panel;
        text = colours_.accent;
        break;
      case Availability::Available:
        label = "AVAILABLE";
        fill = colours_.success;
        text = colours_.background;
        break;
      case Availability::Unavailable:
        label = "UNAVAILABLE";
        fill = colours_.error;
        text = colours_.white;
        break;
      case Availability::Unknown:
      default:
        break;
    }

    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return;
    target->fillRoundRect(x, y, width, height, 3, fill);
    const String value(label);
    const int textWidth = textRenderer_.measureTextWidth(value, 1);
    textRenderer_.drawText(
        value, x + (width - textWidth) / 2, y + 4, 1, text);
  }

  void beginPage(const String& location, const String& clock) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return;
    target->fillScreen(colours_.background);
    headerRenderer_.draw(
        location, clock, colours_.panel, colours_.white, colours_.accent);
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
      uint16_t text,
      bool highlighted = false) const {
    Arduino_GFX* target = displayManager_.target();
    if (target == nullptr) return;
    target->fillRoundRect(
        x, y, width, height, 5,
        highlighted ? colours_.accent : fill);
    target->drawRoundRect(
        x, y, width, height, 5,
        highlighted ? colours_.white : colours_.accent);
    const String value(label);
    const int textWidth = textRenderer_.measureTextWidth(value, 1);
    textRenderer_.drawText(
        value, x + (width - textWidth) / 2,
        y + (height - 8) / 2, 1,
        highlighted ? colours_.background : text);
  }

  String shortenSsidToWidth(
      const char* ssid,
      int textSize,
      int maximumWidth) const {
    String label(ssid != nullptr ? ssid : "");
    bool shortened = false;
    while (label.length() > 0 &&
           textRenderer_.measureTextWidth(label, textSize) > maximumWidth) {
      label.remove(label.length() - 1);
      shortened = true;
    }
    if (shortened) {
      while (label.length() > 0 &&
             textRenderer_.measureTextWidth(label + ".", textSize) >
                 maximumWidth) {
        label.remove(label.length() - 1);
      }
      label += ".";
    }
    return label;
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
