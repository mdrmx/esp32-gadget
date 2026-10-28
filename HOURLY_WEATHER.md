# Hourly weather data and condition icons

The dashboard's hourly forecast uses OpenWeather One Call API 4.0, endpoint:

`https://api.openweathermap.org/data/4.0/onecall/timeline/1h`

It requests hourly metric data based on the latitude/longitude returned by the current-weather endpoint and follows the API's `next` URLs to load up to 48 records. The endpoint returns up to 20 records per response.

**Account requirement:** One Call API 4.0 requires its separate One Call API subscription. OpenWeather currently lists the first 1,000 requests per day as free; usage above the daily quota is billed per request. Each paginated page is a separate call. If access isn't enabled, the screen and serial log show an explicit One Call error instead of labelling three-hourly data as hourly.

The main weather screen uses the condition icon code from the standard current-weather response and draws a local icon from Arduino_GFX primitives, avoiding a separate image-decoder library.
