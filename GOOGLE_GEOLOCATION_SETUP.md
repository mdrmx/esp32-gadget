# Google Wi-Fi Geolocation setup

The dashboard attempts Google Wi-Fi positioning at startup, before using the existing IP-based location lookup. It scans nearby Wi-Fi beacons; it does not connect to the scanned networks.

## Configure the API key

The local `secrets.h` file is ignored by Git so credentials are not committed. Add this line to your existing `secrets.h`, alongside the Wi-Fi and OpenWeather credentials:

```cpp
#define GOOGLE_GEOLOCATION_API_KEY "YOUR_GOOGLE_GEOLOCATION_API_KEY"
```

Replace the placeholder with the API key from your Google Cloud project. Do not commit `secrets.h`, paste the key into an issue/PR, or hard-code it in `weatherDashboard.ino`.

The Google Maps Platform **Geolocation API** must be enabled and billing configured for the project. Restrict the key to the Geolocation API and set an appropriate quota/budget alert. Because the key is used by firmware on the device, do not assume it can be kept secret from someone with physical access to the device.

If the key is missing, the sketch still compiles and skips Google positioning, then tries IP geolocation.

## What happens at startup

1. The ESP32 scans nearby Wi-Fi access points without connecting to them.
2. It filters non-universally-administered / multicast BSSIDs and Google's documented reserved MAC range, then keeps up to 15 strongest unique candidates.
3. If at least two candidates remain, it POSTs their BSSIDs, RSSI values and channels to Google with `considerIp: false`. This prevents a successful response from silently using IP geolocation instead.
4. On a valid response, the dashboard uses Google's latitude/longitude for current weather and the hourly forecast. The returned accuracy radius is logged to Serial Monitor.
5. If scanning, filtering, the request, or response validation fails, the firmware falls back to the existing IP-based lookup. If that also fails, it keeps the configured fallback coordinates.

The BSSID list, signal strengths and channels are sent to Google to perform the location lookup. No SSIDs, Wi-Fi passwords or handshakes are sent.

## Testing

Open Serial Monitor at **115200 baud** and restart the board. Look for messages beginning with `[LOCATION]`, particularly:

- `Scanning nearby Wi-Fi access points`
- the number of detected networks and usable BSSIDs
- `Google Wi-Fi positioning succeeded` and its accuracy radius
- a fallback reason if Google positioning could not be used

The location lookup runs at startup. Restart the board to trigger another Wi-Fi positioning attempt.
