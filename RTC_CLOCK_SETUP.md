# Location header clock and DS1307 RTC

## What it does

- The shared display header shows the weather location on the left and a 24-hour `HH:MM` clock on the right, on both the current-weather and hourly-forecast screens.
- On Wi-Fi, the ESP32 obtains time from NTP and writes the synchronised UTC time to the DS1307.
- The DS1307 then retains UTC time from its backup battery when the device is switched off or has no network.
- The firmware applies the UK `GMT/BST` timezone rule when displaying the time, including daylight-saving transitions while offline.
- When connected, the clock resynchronises every six hours; if initial NTP sync fails, it retries after five minutes.

The display clock uses `--:--` until either the RTC holds a valid time or NTP obtains one.

## Arduino library

Install **RTClib by Adafruit** from Arduino IDE Library Manager. PNGdec is still required by the current-weather icon feature.

## DS1307 module wiring

The firmware already starts the shared I²C bus for the CST816 touch controller using:

- ESP32 GPIO 48: SDA
- ESP32 GPIO 47: SCL

The RTC can share that bus; the CST816 uses I²C address `0x15` and the DS1307 normally uses `0x68`. Do not start a second I²C bus.

| DS1307 module pin | Connection |
| --- | --- |
| VCC | Regulated 5 V supply |
| GND | Common ground shared with ESP32 and level shifter |
| SDA | High-voltage side SDA of a bidirectional I²C level shifter |
| SCL | High-voltage side SCL of a bidirectional I²C level shifter |
| SQW | Leave unconnected |

Level shifter connections:

| Level shifter pin | Connection |
| --- | --- |
| LV | ESP32 3.3 V |
| HV | Same regulated 5 V supply as RTC VCC |
| GND | Common ground |
| LV1 / LV SDA | ESP32 GPIO 48 (SDA) |
| LV2 / LV SCL | ESP32 GPIO 47 (SCL) |
| HV1 / HV SDA | DS1307 SDA |
| HV2 / HV SCL | DS1307 SCL |

**Do not connect the DS1307 module's SDA/SCL directly to ESP32 GPIO pins unless you have verified that the module's pull-ups are at 3.3 V.** The DS1307 is specified for 4.5–5.5 V operation, and its I²C bus can be pulled up to 5.5 V; ESP32 GPIOs are not 5 V tolerant. A bidirectional I²C level shifter is the safe option.

Use a regulated 5 V output (such as the PowerBoost 1000C's 5 V output in this setup), not a raw battery connection, for module VCC. Connect all grounds together.

### Backup-cell caution

Check the exact DS1307 module's battery/charging circuit. Some inexpensive RTC boards include a trickle-charging path intended for a rechargeable LIR2032 cell. If the fitted coin cell is a non-rechargeable CR2032 and the board charges it, do not power it in that configuration; use the manufacturer's intended rechargeable cell or remove/disable the charging path correctly.

## Verification

After wiring and uploading:

1. Serial Monitor should report `DS1307 detected at I2C address 0x68.`
2. On network connection, it should report the NTP local time and `DS1307 synchronised to UTC`.
3. The top-right clock should advance once per minute.
4. Disconnect Wi-Fi or restart the device; the clock should continue using the battery-backed RTC.
5. The location in the top-left header should appear on both tabs.
