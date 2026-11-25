#include "src/services/WiFiSignalMeter.h"

#include <stdio.h>

namespace {
int failures = 0;
#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    ++failures; \
  } \
} while (0)

void startsWithoutSamplesAndMapsStrengthThresholds() {
  WiFiSignalMeter meter;
  CHECK(!meter.hasSamples());
  CHECK(meter.sampleCount() == 0);
  CHECK(meter.averageRssiDbm() == -127);
  CHECK(meter.level() == 0);

  CHECK(meter.addSample(-55));
  CHECK(meter.level() == 4);
  CHECK(WiFiSignalMeter::WindowSize == 5);

  meter.reset();
  CHECK(meter.addSample(-56));
  CHECK(meter.level() == 3);
  meter.reset();
  CHECK(meter.addSample(-68));
  CHECK(meter.level() == 2);
  meter.reset();
  CHECK(meter.addSample(-76));
  CHECK(meter.level() == 1);
  meter.reset();
  CHECK(meter.addSample(-86));
  CHECK(meter.level() == 0);
}

void averagesRecentSamplesAndDropsOldest() {
  WiFiSignalMeter meter;
  CHECK(meter.addSample(-40));
  CHECK(meter.addSample(-60));
  CHECK(meter.addSample(-70));
  CHECK(meter.addSample(-80));
  CHECK(meter.addSample(-90));
  CHECK(meter.sampleCount() == 5);
  CHECK(meter.averageRssiDbm() == -68);
  CHECK(meter.level() == 2);

  CHECK(meter.addSample(-50));
  CHECK(meter.sampleCount() == 5);
  CHECK(meter.averageRssiDbm() == -70);
  CHECK(meter.level() == 2);
}

void rejectsInvalidValuesWithoutChangingAverage() {
  WiFiSignalMeter meter;
  CHECK(meter.addSample(-60));
  CHECK(!meter.addSample(0));
  CHECK(!meter.addSample(5));
  CHECK(!meter.addSample(-127));
  CHECK(!meter.addSample(-128));
  CHECK(meter.sampleCount() == 1);
  CHECK(meter.averageRssiDbm() == -60);
  CHECK(meter.level() == 3);
}

void resetClearsTheWindow() {
  WiFiSignalMeter meter;
  CHECK(meter.addSample(-44));
  CHECK(meter.addSample(-48));
  meter.reset();
  CHECK(!meter.hasSamples());
  CHECK(meter.sampleCount() == 0);
  CHECK(meter.level() == 0);
  CHECK(meter.addSample(-82));
  CHECK(meter.level() == 1);
}
}

int main() {
  startsWithoutSamplesAndMapsStrengthThresholds();
  averagesRecentSamplesAndDropsOldest();
  rejectsInvalidValuesWithoutChangingAverage();
  resetClearsTheWindow();

  if (failures != 0) {
    fprintf(stderr, "%d WiFiSignalMeter test(s) failed.\n", failures);
    return 1;
  }

  printf("WiFiSignalMeter tests passed.\n");
  return 0;
}
