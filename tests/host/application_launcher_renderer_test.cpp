#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../../src/apps/ApplicationManager.h"
#include "../../src/ui/ApplicationLauncherRenderer.h"
#include "../../src/ui/icons/LauncherPngAssets.h"

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

class FakeApplication : public Application {
 public:
  explicit FakeApplication(const char* id) : id_(id) {}

  const char* id() const override { return id_; }
  bool begin() override { return true; }
  void update() override {}
  void end() override {}

 private:
  const char* id_;
};

static const ApplicationLauncherRenderer::Colours LauncherColours = {
  0xFF9C, 0xFF9C, 0x28C1, 0x28C1, 0xD303, 0x28C1
};

static bool iconPixelAt(int x, int y) {
  const uint8_t byteValue =
    WeatherLauncherIcon::Bitmap[y * WeatherLauncherIcon::BytesPerRow + x / 8];
  return (byteValue & (0x80U >> (x % 8))) != 0;
}

struct IconDrawCapture {
  uint8_t icons[5];
  int x[5];
  int y[5];
  uint8_t count;
};

static void capturePlaceholderIcon(
    void* userContext, uint8_t icon, int x, int y) {
  IconDrawCapture* capture = (IconDrawCapture*)userContext;
  if (capture == nullptr || capture->count >= 5) return;
  const uint8_t slot = capture->count++;
  capture->icons[slot] = icon;
  capture->x[slot] = x;
  capture->y[slot] = y;
}

static bool testFlashResidentPngAssets() {
  const uint8_t expectedSizes[5] = {2268, 1898, 2499, 1499, 3032};
  for (uint8_t icon = 1; icon <= 5; ++icon) {
    const uint8_t* data = nullptr;
    size_t length = 0;
    CHECK(LauncherPngAssets::get(icon, &data, &length));
    CHECK(data != nullptr);
    CHECK(length == expectedSizes[icon - 1]);

    // PNG signature and the big-endian IHDR dimensions are checked against
    // the exact embedded flash data, not only the source PNG filenames.
    CHECK(data[0] == 0x89);
    CHECK(data[1] == 0x50);
    CHECK(data[2] == 0x4e);
    CHECK(data[3] == 0x47);
    CHECK(data[4] == 0x0d);
    CHECK(data[5] == 0x0a);
    CHECK(data[6] == 0x1a);
    CHECK(data[7] == 0x0a);
    CHECK(data[16] == 0);
    CHECK(data[17] == 0);
    CHECK(data[18] == 0);
    CHECK(data[19] == 64);
    CHECK(data[20] == 0);
    CHECK(data[21] == 0);
    CHECK(data[22] == 0);
    CHECK(data[23] == 64);
    CHECK(data[24] == 8);
    CHECK(data[25] == 6);  // 8-bit RGBA
  }

  const uint8_t* data = nullptr;
  size_t length = 0;
  CHECK(!LauncherPngAssets::get(0, &data, &length));
  CHECK(!LauncherPngAssets::get(6, &data, &length));
  CHECK(!LauncherPngAssets::get(1, nullptr, &length));
  CHECK(!LauncherPngAssets::get(1, &data, nullptr));
  return true;
}

static bool testWeatherIconRetainsOriginalBitmap() {
  CHECK(WeatherLauncherIcon::Width == 32);
  CHECK(WeatherLauncherIcon::Height == 32);
  CHECK(WeatherLauncherIcon::BytesPerRow == 4);

  // Preserve the original two transparent rows above and below the artwork.
  for (int x = 0; x < WeatherLauncherIcon::Width; ++x) {
    CHECK(!iconPixelAt(x, 0));
    CHECK(!iconPixelAt(x, 1));
    CHECK(!iconPixelAt(x, 30));
    CHECK(!iconPixelAt(x, 31));
  }
  CHECK(iconPixelAt(9, 8));
  return true;
}

static bool testSixTileCatalogAndRegisteredHitTargets() {
  DisplayManager displayManager;
  TextRenderer textRenderer(displayManager);
  ApplicationLauncherRenderer renderer(
      displayManager, textRenderer, LauncherColours);
  ApplicationManager manager;

  renderer.draw(manager, nullptr);
  CHECK(displayManager.target() == nullptr);

  FakeApplication weather("weather");
  CHECK(manager.registerApplication(weather));
  CHECK(displayManager.begin());

  IconDrawCapture capture = {};
  renderer.draw(
      manager, "weather", String(), capturePlaceholderIcon, &capture);

  Arduino_GFX* target = displayManager.target();
  CHECK(target != nullptr);
  CHECK(target->fillScreenCount == 1);
  CHECK(target->lastFillScreenColour == LauncherColours.background);

  // The visual catalogue is always a three-column, two-row grid.
  CHECK(target->fillRoundRectCount == 6);
  CHECK(target->drawRoundRectCount == 6);
  const int expectedX[6] = {38, 126, 214, 38, 126, 214};
  const int expectedY[6] = {60, 60, 60, 148, 148, 148};
  for (int i = 0; i < 6; ++i) {
    CHECK(target->fillRoundRectX[i] == expectedX[i]);
    CHECK(target->fillRoundRectY[i] == expectedY[i]);
    CHECK(target->fillRoundRectWidth[i] == 68);
    CHECK(target->fillRoundRectHeight[i] == 68);
    CHECK(target->drawRoundRectColour[i] == LauncherColours.text);
  }

  // The original 32x32 Weather glyph stays centred in its own tile.
  CHECK(target->drawBitmapCount == 1);
  CHECK(target->lastBitmapX == 56);
  CHECK(target->lastBitmapY == 78);
  CHECK(target->lastBitmapWidth == 32);
  CHECK(target->lastBitmapHeight == 32);
  CHECK(target->lastBitmapColour == LauncherColours.text);
  CHECK(target->lastBitmap != nullptr);

  // Five non-Weather assets are delegated to the PNG drawer, inset by 2 px.
  CHECK(capture.count == 5);
  for (uint8_t i = 0; i < capture.count; ++i) {
    CHECK(capture.icons[i] == (uint8_t)(i + 1));
  }
  CHECK(capture.x[0] == 128);
  CHECK(capture.y[0] == 62);
  CHECK(capture.x[1] == 216);
  CHECK(capture.y[1] == 62);
  CHECK(capture.x[2] == 40);
  CHECK(capture.y[2] == 150);
  CHECK(capture.x[3] == 128);
  CHECK(capture.y[3] == 150);
  CHECK(capture.x[4] == 216);
  CHECK(capture.y[4] == 150);

  // Only the launcher title is printed: no app names or instructions.
  CHECK(target->printCount == 1);
  CHECK(target->printedText[0] == String("APPLICATIONS"));

  // Weather is the only registered app, so the other five tiles are inert.
  uint8_t applicationIndex = 99;
  CHECK(renderer.applicationAt(60, 65, applicationIndex, manager));
  CHECK(applicationIndex == 0);
  CHECK(!renderer.applicationAt(130, 65, applicationIndex, manager));
  CHECK(!renderer.applicationAt(220, 65, applicationIndex, manager));
  CHECK(!renderer.applicationAt(60, 155, applicationIndex, manager));
  CHECK(!renderer.applicationAt(130, 155, applicationIndex, manager));
  CHECK(!renderer.applicationAt(220, 155, applicationIndex, manager));
  CHECK(!renderer.applicationAt(20, 65, applicationIndex, manager));
  CHECK(!renderer.applicationAt(60, 136, applicationIndex, manager));
  CHECK(!renderer.applicationAt(60, 213, applicationIndex, manager));

  return true;
}

static bool testRegisteredPlaceholderUsesManagerIndex() {
  DisplayManager displayManager;
  TextRenderer textRenderer(displayManager);
  ApplicationLauncherRenderer renderer(
      displayManager, textRenderer, LauncherColours);
  ApplicationManager manager;
  FakeApplication plant("plant");
  FakeApplication weather("weather");

  // Registration order need not match the fixed visual catalogue order.
  CHECK(manager.registerApplication(plant));
  CHECK(manager.registerApplication(weather));
  CHECK(displayManager.begin());
  renderer.draw(manager, nullptr);

  uint8_t applicationIndex = 99;
  CHECK(renderer.applicationAt(40, 150, applicationIndex, manager));
  CHECK(applicationIndex == 0);
  CHECK(renderer.applicationAt(40, 62, applicationIndex, manager));
  CHECK(applicationIndex == 1);
  CHECK(!renderer.applicationAt(128, 62, applicationIndex, manager));
  return true;
}

static bool testEmptyLauncherStillShowsReferenceCatalogue() {
  DisplayManager displayManager;
  TextRenderer textRenderer(displayManager);
  ApplicationLauncherRenderer renderer(
      displayManager, textRenderer, LauncherColours);
  ApplicationManager manager;

  CHECK(displayManager.begin());
  renderer.draw(manager, nullptr);

  Arduino_GFX* target = displayManager.target();
  CHECK(target != nullptr);
  CHECK(target->fillRoundRectCount == 6);
  CHECK(target->drawRoundRectCount == 6);
  CHECK(target->drawBitmapCount == 1);
  CHECK(target->printCount == 1);
  CHECK(target->printedText[0] == String("APPLICATIONS"));
  return true;
}

int main() {
  if (!testFlashResidentPngAssets()) return EXIT_FAILURE;
  if (!testWeatherIconRetainsOriginalBitmap()) return EXIT_FAILURE;
  if (!testSixTileCatalogAndRegisteredHitTargets()) return EXIT_FAILURE;
  if (!testRegisteredPlaceholderUsesManagerIndex()) return EXIT_FAILURE;
  if (!testEmptyLauncherStillShowsReferenceCatalogue()) return EXIT_FAILURE;

  printf("ApplicationLauncherRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
