# Landscape touch gesture diagnostic

The CST816 register values must be measured on the actual screen orientation. Do not assume that display rotation changes the controller's raw gesture codes.

## Expected app interaction

- Current-weather tab: physical right-to-left swipe opens forecast.
- Forecast tab: physical bottom-to-top swipe advances one forecast item.
- Forecast tab: physical top-to-bottom swipe moves back one forecast item.
- Forecast tab: physical left-to-right swipe returns to current weather.

## Capture procedure

The current live-mode default (`USE_DEMO_WEATHER_DATA 0`) compiles the normal `DEBUG_PRINT` / `DEBUG_PRINTF` macros to no-ops. To use this guide's gesture log, temporarily set `USE_DEMO_WEATHER_DATA 1`, compile and upload, then open Serial Monitor at 115200 baud. Demo mode keeps the UI interaction available but disables live Wi-Fi, location, time and weather requests. Restore `0` and recompile after the diagnostic session. Do not enable or publish credentials just to obtain gesture logs.

Record the `Touch raw gesture: 0xNN` line for each gesture, performing only one gesture at a time:

1. Right to left, while on current weather.
2. Left to right, while on forecast.
3. Bottom to top, while on forecast.
4. Top to bottom, while on forecast.

If multiple serial lines are emitted for one physical gesture, note them all. The gesture register appears to be latched until it reads zero; a stale non-zero value could prevent repeating the same gesture until the controller clears it.

## Why measure first?

The app's current mapping assumes `0x01` is physical right-to-left, `0x02` is left-to-right, `0x03` is top-to-bottom, and `0x04` is bottom-to-top. Earlier logs already confirmed `0x03` and `0x04` for vertical gestures, but did not conclusively verify the horizontal values. Updating navigation based only on display rotation risks reproducing the same issue.

Once all four observed values are confirmed, update the gesture constants in `weatherDashboard.ino` to match those values and test that vertical gestures are ignored on the current-weather tab.
