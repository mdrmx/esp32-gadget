# Offline development mode

The dashboard has an offline demo mode so UI and interaction features can be developed without spending OpenWeather allowance or depending on Wi-Fi.

## Enable / disable

In `weatherDashboard.ino`, near the top of the configuration section:

```cpp
#define USE_DEMO_WEATHER_DATA 1
```

- `1` (current default): use deterministic sample Manchester weather and 24 hourly forecast entries. No Wi-Fi connection or HTTP request is made for weather, time, or weather icons. The clock starts from a fixed local development time and then advances normally.
- `0`: use the live weather, One Call hourly forecast, TimeAPI.io clock sync, and OpenWeather icon download paths.

Keep demo mode enabled while implementing or testing display layouts, gestures, scrolling, and metric cards. Switch to `0` only when you specifically need to verify live API integration. Do not commit API credentials; keep them in the ignored `secrets.h` file.

## Sample values

The fixture populates all eight current-weather metrics and a repeating 24-hour forecast containing clear, cloudy, and rainy conditions. Values are illustrative, not live observations.

The normal periodic refresh still runs in demo mode, but it reloads local fixture data instead of making requests.
