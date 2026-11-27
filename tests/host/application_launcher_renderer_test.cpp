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
  0xFF9C, 0xFF9C, 0x28C1, 0x28C1, 0xD303, 0x28C1
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

  // Four square icon tiles are drawn with no BACK button or footer prompt.
  CHECK(target->fillRoundRectCount == 4);
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
  CHECK(target->drawRoundRectCount == 4);
  CHECK(target->drawRoundRectColour[0] == LauncherColours.text);
  CHECK(target->drawRoundRectColour[1] == LauncherColours.text);

  // The user-supplied Weather glyph is drawn as a 32x32 flash bitmap,
  // centred in the 68px tile and tinted with the OS ink colour.
  CHECK(target->drawBitmapCount == 1);
  CHECK(target->lastBitmapX == 74);
  CHECK(target->lastBitmapY == 68);
  CHECK(target->lastBitmapWidth == 32);
  CHECK(target->lastBitmapHeight == 32);
  CHECK(target->lastBitmapColour == LauncherColours.text);
  CHECK(target->lastBitmap != nullptr);

  // Only the launcher title is printed: no app names, BACK or prompt.
  CHECK(target->printCount == 1);
  CHECK(target->printedText[0] == String("APPLICATIONS"));

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
  CHECK(target->fillRoundRectY[0] == 104);
  CHECK(target->fillRoundRectWidth[0] == 68);
  CHECK(target->fillRoundRectHeight[0] == 68);
  CHECK(target->drawRoundRectColour[0] == LauncherColours.text);
  CHECK(target->drawBitmapCount == 1);
  CHECK(target->lastBitmapX == 144);
  CHECK(target->lastBitmapY == 122);
  CHECK(target->printCount == 1);
  CHECK(target->printedText[0] == String("APPLICATIONS"));

  uint8_t applicationIndex = 99;
  CHECK(renderer.applicationAt(126, 89, applicationIndex, manager));
  CHECK(applicationIndex == 0);
  CHECK(renderer.applicationAt(193, 156, applicationIndex, manager));
  CHECK(applicationIndex == 0);
  CHECK(!renderer.applicationAt(125, 89, applicationIndex, manager));
  CHECK(!renderer.applicationAt(194, 156, applicationIndex, manager));
  return true;
}

static bool testTwoAppRowIsVerticallyCentred() {
  DisplayManager displayManager;
  TextRenderer textRenderer(displayManager);
  ApplicationLauncherRenderer renderer(
      displayManager, textRenderer, LauncherColours);
  ApplicationManager manager;
  FakeApplication weather("weather");
  FakeApplication maps("maps");

  CHECK(manager.registerApplication(weather));
  CHECK(manager.registerApplication(maps));
  CHECK(displayManager.begin());
  renderer.draw(manager, nullptr);

  Arduino_GFX* target = displayManager.target();
  CHECK(target != nullptr);
  CHECK(target->fillRoundRectCount == 2);
  CHECK(target->fillRoundRectX[0] == 56);
  CHECK(target->fillRoundRectX[1] == 196);
  CHECK(target->fillRoundRectY[0] == 89);
  CHECK(target->fillRoundRectY[1] == 104);
  CHECK(target->drawRoundRectColour[0] == LauncherColours.text);
  CHECK(target->drawBitmapCount == 1);
  CHECK(target->printCount == 1);
  CHECK(target->printedText[0] == String("APPLICATIONS"));

  uint8_t applicationIndex = 99;
  CHECK(renderer.applicationAt(60, 90, applicationIndex, manager));
  CHECK(applicationIndex == 0);
  CHECK(renderer.applicationAt(200, 90, applicationIndex, manager));
  CHECK(applicationIndex == 1);
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
  CHECK(target->printCount == 2);
  CHECK(target->printedText[0] == String("APPLICATIONS"));
  CHECK(target->printedText[1] == String("NO APPLICATIONS"));
  return true;
}

int main() {
  if (!testIconGridLayoutAndHitTargets()) return EXIT_FAILURE;
  if (!testSingleWeatherIconIsCentred()) return EXIT_FAILURE;
  if (!testTwoAppRowIsVerticallyCentred()) return EXIT_FAILURE;
  if (!testEmptyLauncherState()) return EXIT_FAILURE;

  printf("ApplicationLauncherRenderer tests passed.\n");
  return EXIT_SUCCESS;
}
