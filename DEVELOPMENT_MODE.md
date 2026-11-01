# Offline development mode

The dashboard supports an offline demo mode for UI and interaction development without using the OpenWeather allowance or depending on Wi-Fi.

## Current firmware default

In `weatherDashboard.ino`, the current tracked setting is:

```cpp
#define USE_DEMO_WEATHER_DATA 0
```

- `0` (**current default — live mode**): uses live Wi-Fi, TimeAPI.io clock sync, location lookup, OpenWeather current weather, One Call hourly forecast and weather-icon downloads. A local `secrets.h` with the required credentials is needed.
- `1` (**offline demo mode**): uses deterministic sample Manchester weather and 24 hourly forecast entries. It skips Wi-Fi, location and weather/time/icon network requests. The clock starts from a fixed local development time and then advances normally.

## Enable / disable

To switch modes, change the `USE_DEMO_WEATHER_DATA` definition near the top of `weatherDashboard.ino`, then compile and upload the firmware:

```cpp
#define USE_DEMO_WEATHER_DATA 1  // offline demo mode
```

Set it back to `0` for live network operation.

Keep demo mode enabled while implementing or testing display layouts, gestures, scrolling and metric cards. Switch to live mode when verifying API, Wi-Fi, location or time integrations. Do not commit API credentials; keep them in the ignored `secrets.h` file.

## Sample values

The fixture populates all eight current-weather metrics and a repeating 24-hour forecast containing clear, cloudy and rainy conditions. Values are illustrative, not live observations.

The normal periodic refresh still runs in demo mode, but it reloads local fixture data instead of making requests.
