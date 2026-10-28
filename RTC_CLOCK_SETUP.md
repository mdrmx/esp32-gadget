# Location header clock and DS1307 RTC

## Behaviour

- The shared header shows the configured weather location and a 24-hour `HH:MM` clock on both tabs.
- At startup, the ESP32 reads the DS1307 once and initialises the ESP32 system clock with `settimeofday()`.
- The header reads only the ESP32 system clock; it does not query the RTC during every redraw.
- When Wi-Fi is available, ESP-IDF SNTP updates system time asynchronously. A sync callback queues a DS1307 write for the main loop, so the callback itself does not perform I²C work.
- The DS1307 stores UTC. The ESP32 applies the UK `GMT/BST` timezone rule to the displayed time, including daylight-saving changes while offline.
- If Wi-Fi or NTP is unavailable, the system clock continues from the RTC initialised at startup. When connectivity returns, SNTP can correct the system clock and synchronise the RTC again.

The display clock shows `--:--` until a valid time is available from the DS1307 or NTP.

## Arduino libraries

Install these libraries from Arduino IDE Library Manager:

- **RTClib by Adafruit**
- **PNGdec by Larry Bank** (required for the OpenWeather condition icons)

## DS1307 module wiring

The firmware shares the existing I²C bus used by the CST816 touch controller:

- ESP32 GPIO 48: SDA
- ESP32 GPIO 47: SCL

The CST816 touch controller uses I²C address `0x15`; the DS1307 normally uses `0x68`. Do not create a second I²C bus.

| DS1307 module pin | Connection |
| --- | --- |
| VCC | Regulated 5 V supply |
| GND | Common ground shared with ESP32 and level shifter |
| SDA | High-voltage SDA on a bidirectional I²C level shifter |
| SCL | High-voltage SCL on a bidirectional I²C level shifter |
| SQW | Leave unconnected |

Level-shifter connections:

| Level shifter pin | Connection |
| --- | --- |
| LV | ESP32 3.3 V |
| HV | Same regulated 5 V supply as RTC VCC |
| GND | Common ground |
| LV1 / LV SDA | ESP32 GPIO 48 (SDA) |
| LV2 / LV SCL | ESP32 GPIO 47 (SCL) |
| HV1 / HV SDA | DS1307 SDA |
| HV2 / HV SCL | DS1307 SCL |

**Do not connect the DS1307 module's SDA/SCL directly to the ESP32 unless you've verified the RTC board's pull-ups are at 3.3 V.** The DS1307's I²C lines may be pulled as high as 5.5 V, while ESP32 GPIOs are not 5 V tolerant. A bidirectional level shifter is the safe option.

Use regulated 5 V for RTC VCC, not a raw battery connection. Connect all grounds together.

### Backup-cell caution

Inspect the exact module's backup-battery circuitry. Some boards include a charging path intended for a rechargeable LIR2032. Do not use a non-rechargeable CR2032 if the board charges it; use the intended rechargeable cell or safely remove/disable the charging path according to the module's design.

## How to verify

1. Upload the firmware and open Serial Monitor at 115200 baud.
2. Confirm `DS1307 detected at I2C address 0x68.`
3. With Wi-Fi available, confirm the log reports that asynchronous SNTP synchronisation has started, followed by `SNTP synchronisation completed; RTC update queued.` and `DS1307 updated with synchronised UTC time.`
4. Confirm the header clock advances without waiting for the RTC each second.
5. Disconnect Wi-Fi and restart. The clock should continue from the DS1307 battery-backed time.
6. Check that the same location and clock appear in both tabs.
