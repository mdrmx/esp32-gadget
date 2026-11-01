# Hourly weather data and OpenWeather icons

The dashboard's hourly forecast uses OpenWeather One Call API 4.0, endpoint:

`https://api.openweathermap.org/data/4.0/onecall/timeline/1h`

It requests hourly metric data using the coordinates currently held by the firmware's Location flow (Google Wi-Fi positioning when available, then IP geolocation, then the configured fallback). The current-weather request does not normally replace coordinates when a location fix has already been resolved. The forecast code follows the API's `next` URLs to load up to 48 records, with up to 20 records per response.

## Official condition icons

The current conditions icon is downloaded using the icon code from the current-weather response. For example, `10d` loads:

`https://openweathermap.org/img/wn/10d@2x.png`

OpenWeather's weather-conditions documentation lists these icon codes and says the icons may be used in your own solutions: https://openweathermap.org/api/weather-conditions

The ESP32-S3 cannot render a PNG from a URL like a browser can. This sketch downloads the 2× PNG asset (`{icon}@2x.png`), caches the response bytes, decodes it to RGB565 and draws it on the display. A cache miss currently performs a synchronous HTTPS request from the Weather rendering path; this is a known refactor boundary, not an ideal long-term design. Install **PNGdec by Larry Bank** from Arduino IDE Library Manager.

## One Call account requirement

One Call API 4.0 requires its separate One Call API subscription. Each paginated page is a separate request. If access isn't enabled, the screen and serial log show an explicit One Call error instead of labelling three-hourly data as hourly.
