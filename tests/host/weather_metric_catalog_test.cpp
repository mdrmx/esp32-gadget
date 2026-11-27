#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/apps/weather/WeatherMetricCatalog.h"

static void check(bool condition, const char* expression, int line) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    exit(EXIT_FAILURE);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
  typedef WeatherMetricCatalog Catalog;
  const Catalog::MetricId expectedIds[Catalog::Count] = {
    Catalog::RainChance,
    Catalog::Wind,
    Catalog::UvIndex,
    Catalog::Humidity,
    Catalog::Pressure,
    Catalog::CloudCover,
    Catalog::Visibility,
    Catalog::RainLastHour
  };
  const char* expectedLabels[Catalog::Count] = {
    "RAIN CHANCE",
    "WIND",
    "UV INDEX",
    "HUMIDITY",
    "PRESSURE",
    "CLOUD COVER",
    "VISIBILITY",
    "RAIN (1H)"
  };

  CHECK(Catalog::Count == 8);
  bool seen[Catalog::Count] = {};
  for (int cardIndex = 0; cardIndex < Catalog::Count; ++cardIndex) {
    const Catalog::MetricId metric = Catalog::metricForCard(cardIndex);
    CHECK(metric == expectedIds[cardIndex]);
    CHECK(strcmp(Catalog::label(metric), expectedLabels[cardIndex]) == 0);

    const int identity = static_cast<int>(metric);
    CHECK(identity >= 0);
    CHECK(identity < Catalog::Count);
    CHECK(!seen[identity]);
    seen[identity] = true;
  }

  CHECK(Catalog::metricForCard(-1) == Catalog::Invalid);
  CHECK(Catalog::metricForCard(Catalog::Count) == Catalog::Invalid);
  CHECK(strcmp(Catalog::label(Catalog::Invalid), "") == 0);

  printf("WeatherMetricCatalog tests passed.\n");
  return EXIT_SUCCESS;
}
