#include <stdio.h>
#include <stdlib.h>

#include "../../src/input/BottomEdgeSwipeGesture.h"

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

static bool swipe(
    BottomEdgeSwipeGesture& gesture,
    int startX,
    int startY,
    int endX,
    int endY,
    uint32_t startMs,
    uint32_t endMs) {
  CHECK(!gesture.update(
      true, true, false, startX, startY, startMs, 240));
  CHECK(!gesture.update(
      false, true, false, endX, endY, endMs - 1, 240));
  return gesture.update(
      false, false, true, endX, endY, endMs, 240);
}

static bool testQuickBottomEdgeSwipeIsRecognised() {
  BottomEdgeSwipeGesture gesture;
  CHECK(swipe(gesture, 158, 234, 160, 174, 100, 360));
  return true;
}

static bool testGestureRequiresBottomEdgeOrigin() {
  BottomEdgeSwipeGesture gesture;
  CHECK(!swipe(gesture, 158, 205, 160, 145, 100, 360));
  return true;
}

static bool testGestureRequiresUpwardTravel() {
  BottomEdgeSwipeGesture gesture;
  CHECK(!swipe(gesture, 158, 234, 160, 180, 100, 360));
  return true;
}

static bool testGestureRejectsSlowAndHorizontalContacts() {
  BottomEdgeSwipeGesture slow;
  CHECK(!swipe(slow, 158, 234, 160, 174, 100, 900));

  BottomEdgeSwipeGesture horizontal;
  CHECK(!swipe(horizontal, 158, 234, 210, 174, 100, 360));
  return true;
}

static bool testGestureHandlesMillisecondRollover() {
  BottomEdgeSwipeGesture gesture;
  const uint32_t start = 0xFFFFFF00UL;
  const uint32_t end = static_cast<uint32_t>(start + 300UL);
  CHECK(swipe(gesture, 158, 234, 160, 174, start, end));
  return true;
}

int main() {
  if (!testQuickBottomEdgeSwipeIsRecognised()) return EXIT_FAILURE;
  if (!testGestureRequiresBottomEdgeOrigin()) return EXIT_FAILURE;
  if (!testGestureRequiresUpwardTravel()) return EXIT_FAILURE;
  if (!testGestureRejectsSlowAndHorizontalContacts()) return EXIT_FAILURE;
  if (!testGestureHandlesMillisecondRollover()) return EXIT_FAILURE;

  printf("BottomEdgeSwipeGesture tests passed.\n");
  return EXIT_SUCCESS;
}
