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
      true, true, false, startX, startY, startMs, 320, 240));
  CHECK(!gesture.update(
      false, true, false, endX, endY, endMs - 1, 320, 240));
  return gesture.update(
      false, false, true, endX, endY, endMs, 320, 240);
}

static bool testQuickBottomEdgeSwipeIsRecognised() {
  BottomEdgeSwipeGesture gesture;
  CHECK(swipe(gesture, 158, 234, 160, 174, 100, 360));
  return true;
}

static bool testGestureRequiresCentralActivationBand() {
  BottomEdgeSwipeGesture left;
  CHECK(!swipe(left, 40, 234, 42, 174, 100, 360));
  CHECK(!left.consumeTouchSample());

  BottomEdgeSwipeGesture right;
  CHECK(!swipe(right, 260, 234, 258, 174, 100, 360));
  CHECK(!right.consumeTouchSample());

  // On a 320 px display, the 64 px band is [128, 192).
  BottomEdgeSwipeGesture innerLeftBoundary;
  CHECK(swipe(innerLeftBoundary, 128, 234, 130, 174, 100, 360));

  BottomEdgeSwipeGesture innerRightBoundary;
  CHECK(swipe(innerRightBoundary, 191, 234, 189, 174, 100, 360));

  BottomEdgeSwipeGesture outsideRightBoundary;
  CHECK(!swipe(outsideRightBoundary, 192, 234, 190, 174, 100, 360));
  CHECK(!outsideRightBoundary.consumeTouchSample());
  return true;
}

static bool testGestureRequiresBottomEdgeOrigin() {
  BottomEdgeSwipeGesture gesture;
  CHECK(!swipe(gesture, 158, 205, 160, 145, 100, 360));
  return true;
}

static bool testGestureRequiresUpwardTravel() {
  BottomEdgeSwipeGesture gesture;
  CHECK(!swipe(gesture, 158, 234, 160, 183, 100, 360));
  return true;
}

static bool testGestureRejectsSlowAndHorizontalContacts() {
  BottomEdgeSwipeGesture slow;
  CHECK(!swipe(slow, 158, 234, 160, 174, 100, 900));

  BottomEdgeSwipeGesture horizontal;
  CHECK(!swipe(horizontal, 158, 234, 210, 174, 100, 360));
  return true;
}

static bool testBottomEdgeContactsAreConsumedEvenWhenNotGestures() {
  BottomEdgeSwipeGesture gesture;
  CHECK(!gesture.update(true, true, false, 158, 234, 100, 320, 240));
  CHECK(gesture.consumeTouchSample());
  CHECK(!gesture.update(false, true, false, 159, 220, 150, 320, 240));
  CHECK(gesture.consumeTouchSample());
  CHECK(!gesture.update(false, false, true, 159, 220, 170, 320, 240));
  CHECK(gesture.consumeTouchSample());
  CHECK(!gesture.consumeTouchSample());
  return true;
}

static bool testNonBottomContactsAreNotClaimed() {
  BottomEdgeSwipeGesture gesture;
  CHECK(!gesture.update(true, true, false, 158, 180, 100, 320, 240));
  CHECK(!gesture.consumeTouchSample());
  CHECK(!gesture.update(false, false, true, 158, 180, 140, 320, 240));
  CHECK(!gesture.consumeTouchSample());
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
  if (!testGestureRequiresCentralActivationBand()) return EXIT_FAILURE;
  if (!testGestureRequiresBottomEdgeOrigin()) return EXIT_FAILURE;
  if (!testGestureRequiresUpwardTravel()) return EXIT_FAILURE;
  if (!testGestureRejectsSlowAndHorizontalContacts()) return EXIT_FAILURE;
  if (!testBottomEdgeContactsAreConsumedEvenWhenNotGestures()) return EXIT_FAILURE;
  if (!testNonBottomContactsAreNotClaimed()) return EXIT_FAILURE;
  if (!testGestureHandlesMillisecondRollover()) return EXIT_FAILURE;

  printf("BottomEdgeSwipeGesture tests passed.\n");
  return EXIT_SUCCESS;
}
