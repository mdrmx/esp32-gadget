#include <stdio.h>
#include <stdlib.h>

#include "../../src/apps/ApplicationManager.h"
#include "../../src/ui/ApplicationLauncherRenderer.h"

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
  0xF79C, 0xEF3B, 0x18C3, 0x736C, 0xD345, 0xD345
};

static bool testIconGridLayoutAndHitTargets() {
  DisplayManager displayManager;
  TextRenderer textRenderer(displayManager);
  ApplicationLauncherRenderer renderer(
      displayManager, textRenderer, LauncherColours);
  ApplicationManager manager;

  renderer.draw(manager, nullptr);
  CHECK(displayManager.target() == nullptr);

  FakeApplication weather("weather");
  FakeApplication maps("maps");
  FakeApplication settings("settings");
  FakeApplication diagnostics("diagnostics");
  CHECK(manager.registerApplication(weather));
  CHECK(manager.registerApplication(maps));
  CHECK(manager.registerApplication(settings));
  CHECK(manager.registerApplication(diagnostics));
  CHECK(displayManager.begin());

  renderer.draw(manager, "weather");
  Arduino_GFX* target = displayManager.target();
  CHECK(target != nullptr);
  CHECK(target->fillScreenCount == 1);
  CHECK(target->lastFillScreenColour == LauncherColours.background);

  // Four square icon tiles and the BACK control are drawn. The old wide
  // application cards and per-app text labels are intentionally gone.
  CHECK(target->fillRoundRectCount == 5);
  CHECK(target->fillRoundRectX[0] == 56);
  CHECK(target->fillRoundRectY[0] == 50);
  CHECK(target->fillRoundRectWidth[0] == 68);
  CHECK(target->fillRoundRectHeight[0] == 68);
  CHECK(target->fillRoundRectX[1] == 196);
  CHECK(target->fillRoundRectY[1] == 50);
  CHECK(target->fillRoundRectX[2] == 56);
  CHECK(target->fillRoundRectY[2] == 130);
  CHECK(target->fillRoundRectX[3] == 196);
  CHECK(target->fillRoundRectY[3] == 130);
  CHECK(target->fillRoundRectX[4] == 8);
  CHECK(target->fillRoundRectY[4] == 210);
  CHECK(target->fillRoundRectWidth[4] == 76);
  CHECK(target->fillRoundRectHeight[4] == 24);

  CHECK(target->drawRoundRectCount == 5);
  CHECK(target->drawRoundRectColour[0] == LauncherColours.selected);
  CHECK(target->drawRoundRectColour[1] == LauncherColours.muted);

  // The user-supplied Weather glyph is drawn as a 48x48 flash bitmap,
  // centred in the 68px tile and tinted with the OS ink colour.
  CHECK(target->drawBitmapCount == 1);
  CHECK(target->lastBitmapX == 66);
  CHECK(target->lastBitmapY == 60);
  CHECK(target->lastBitmapWidth == 48);
  CHECK(target->lastBitmapHeight == 48);
  CHECK(target->lastBitmapColour == LauncherColours.text);
  CHECK(target->lastBitmap != nullptr);

  // No application ids, OPEN/LAST USED labels or metadata text are rendered.
  CHECK(target->printCount == 3);
  CHECK(target->printedText[0] == String("APPLICATIONS"));
  CHECK(target->printedText[0] != String("WEATHER"));
  CHECK(target->printedText[1] == String("BACK"));
  CHECK(target->printedText[2] == String("SELECT AN APP"));

  uint8_t applicationIndex = 99;
  CHECK(renderer.applicationAt(60, 54, applicationIndex, manager));
  CHECK(applicationIndex == 0);
  CHECK(renderer.applicationAt(200, 54, applicationIndex, manager));
  CHECK(applicationIndex == 1);
  CHECK(renderer.applicationAt(60, 140, applicationIndex, manager));
  CHECK(applicationIndex == 2);
  CHECK(renderer.applicationAt(200, 140, applicationIndex, manager));
  CHECK(applicationIndex == 3);
  CHECK(!renderer.applicationAt(160, 60, applicationIndex, manager));
  CHECK(!renderer.applicationAt(60, 120, applicationIndex, manager));
  CHECK(!renderer.applicationAt(20, 60, applicationIndex, manager));
  CHECK(!renderer.applicationAt(60, 205, applicationIndex, manager));

  CHECK(renderer.backButtonContains(8, 210, "weather"));
  CHECK(renderer.backButtonContains(83, 233, "weather"));
  CHECK(!renderer.backButtonContains(8, 210, nullptr));
  CHECK(!renderer.backButtonContains(84, 220, "weather"));
  CHECK(!renderer.backButtonContains(20, 209, "weather"));
  return true;
}

static bool testSingleWeatherIconIsCentred() {
  DisplayManager displayManager;
  TextRenderer textRenderer(displayManager);
  ApplicationLauncherRenderer renderer(
      displayManager, textRenderer, LauncherColours);
  ApplicationManager manager;
  FakeApplication weather("weather");

  CHECK(manager.registerApplication(weather));
  CHECK(displayManager.begin());
  renderer.draw(manager, nullptr);

  Arduino_GFX* target = displayManager.target();
  CHECK(target != nullptr);
  CHECK(target->fillRoundRectCount == 1);
  CHECK(target->fillRoundRectX[0] == 126);
  CHECK(target->fillRoundRectY[0] == 89);
  CHECK(target->fillRoundRectWidth[0] == 68);
  CHECK(target->fillRoundRectHeight[0] == 68);
  CHECK(target->drawBitmapCount == 1);
  CHECK(target->lastBitmapX == 136);
  CHECK(target->lastBitmapY == 99);
  CHECK(target->printCount == 2);
  CHECK(target->printedText[0] == String("APPLICATIONS"));
  CHECK(target->printedText[1] == String("SELECT AN APP"));

  uint8_t applicationIndex = 99;
  CHECK(renderer.applicationAt(126, 89, applicationIndex, manager));
  CHECK(applicationIndex == 0);
  CHECK(renderer.applicationAt(193, 156, applicationIndex, manager));
  CHECK(applicationIndex == 0);
  CHECK(!renderer.applicationAt(125, 89, applicationIndex, manager));
  CHECK(!renderer.applicationAt(194, 156, applicationIndex, manager));
  CHECK(!renderer.backButtonContains(8, 210, nullptr));
  return true;
}

static bool testEmptyLauncherState() {
  DisplayManager displayManager;
  TextRenderer textRenderer(displayManager);
  ApplicationLauncherRenderer renderer(
      displayManager, textRenderer, LauncherColours);
  ApplicationManager manager;

  CHECK(displayManager.begin());
  renderer.draw(manager, nullptr);
  Arduino_GFX* target = displayManager.target();
  CHECK(target != nullptr);
  CHECK(target->fillRoundRectCount == 0);
  CHECK(target->drawBitmapCount == 0);
  CHECK(target->printCount == 3);
  CHECK(target->printedText[0] == String("APPLICATIONS"));
  CHECK(target->printedText[1] == String("NO APPLICATIONS"));
  CHECK(target->printedText[2] == String("SELECT AN APP"));
  return true;
}

int main() {
  if (!testIconGridLayoutAndHitTargets()) return EXIT_FAILURE;
  if (!testSingleWeatherIconIsCentred()) return EXIT_FAILURE;
  if (!testEmptyLauncherState()) return EXIT_FAILURE;

  printf("ApplicationLauncherRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
