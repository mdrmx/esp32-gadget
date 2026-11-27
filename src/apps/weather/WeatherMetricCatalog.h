#pragma once

// Stable Weather metric identities and the order in which cards appear.
// Weather supplies live values; this catalog controls card order and labels.
class WeatherMetricCatalog {
public:
  enum MetricId {
    Invalid = -1,
    Humidity = 0,
    Pressure,
    UvIndex,
    RainChance,
    Wind,
    CloudCover,
    Visibility,
    RainLastHour
  };

  enum {
    Count = 8
  };

  static MetricId metricForCard(int cardIndex) {
    static const MetricId CardOrder[Count] = {
      RainChance,
      Wind,
      UvIndex,
      Humidity,
      Pressure,
      CloudCover,
      Visibility,
      RainLastHour
    };

    if (cardIndex < 0 || cardIndex >= Count) return Invalid;
    return CardOrder[cardIndex];
  }

  static const char* label(MetricId metric) {
    switch (metric) {
      case RainChance: return "RAIN CHANCE";
      case Wind: return "WIND";
      case UvIndex: return "UV INDEX";
      case Humidity: return "HUMIDITY";
      case Pressure: return "PRESSURE";
      case CloudCover: return "CLOUD COVER";
      case Visibility: return "VISIBILITY";
      case RainLastHour: return "RAIN (1H)";
      case Invalid:
      default:
        return "";
    }
  }
};
