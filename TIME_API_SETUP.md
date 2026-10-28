# Location header clock using a time API

## Behaviour

- The shared header displays the weather location on the left and a 24-hour `HH:MM` clock on the right, on both current-weather and hourly-forecast screens.
- At startup, the ESP32 requests the current date and time from TimeAPI.io for `Europe/London`.
- The response's date and local-time fields are converted to a Unix epoch using the configured UK timezone and daylight-saving rules, then written into the ESP32 system clock with `settimeofday()`.
- After the initial request, the ESP32 system clock advances locally, so the screen does not need to query the time API every second.
- The API is refreshed every six hours while online. If no valid time has been received yet or a refresh fails, it retries no more often than every five minutes.
- With no RTC fitted, a reboot while offline means the device cannot know the real date/time until the API request succeeds again.

## Time API

Request:

`GET https://timeapi.io/api/Time/current/zone?timeZone=Europe%2FLondon`

The firmware reads the `year`, `month`, `day`, `hour`, `minute`, and `seconds` fields. TimeAPI's response fields describe local civil time in the requested zone. The firmware uses the UK timezone rule to account for GMT/BST when converting the response into system epoch time.

If the request fails, Serial Monitor logs the HTTP status or JSON failure. The display shows `--:--` until a valid time has been set.

## Libraries

- **PNGdec by Larry Bank**: required for the official OpenWeather condition icons.

No DS1307 wiring or RTClib library is needed for this version.

## Verification

1. Upload the firmware and open Serial Monitor at 115200 baud.
2. Confirm `Requesting current date/time from TimeAPI.io...` and `System clock set from API: ... Europe/London`.
3. Confirm the header clock updates as minutes pass.
4. Leave the device powered and verify the clock continues without repeated API requests each second.
5. Restart while offline to confirm the limitation: the clock displays `--:--` until a time API request succeeds.
