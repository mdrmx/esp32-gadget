# Phase 0 — Step 3: Build and toolchain baseline

**Status:** Repository-side audit recorded; local toolchain capture and a successful clean build are still pending.  
**Repository:** \`mdrmx/esp32-gadget\`  
**Baseline:** \`main\` at the merged Step 2 commit \`f02baf4f47a010a4ded73d4653b758799f10677f\`  
**Firmware file:** \`weatherDashboard.ino\`

## 1. Purpose and scope

This step establishes what can be verified from the tracked repository and defines the evidence needed to reproduce the current firmware build before Phase 1 moves code into modules.

The repository audit is complete. The developer machine's installed IDE, board package, compiler, board-menu options and library versions are not encoded in the repository, and cannot be inferred reliably from the source. Those values must be captured from the machine that currently builds and uploads the working device.

**No firmware compile, upload or physical-board test was performed for this document.** It is not a claim that a clean build has passed.

## 2. Repository evidence

### 2.1 Sketch layout

The primary sketch is at the repository root as \`weatherDashboard.ino\`. The repository's usual clone directory is \`esp32-gadget\`.

Arduino's sketch specification requires the primary \`.ino\` filename to match the sketch root folder name. Consequently, opening the default \`esp32-gadget\` checkout directly as a standard Arduino sketch may fail the sketch-name requirement.

For a clean checkout that retains the tracked file layout, clone the repository into a directory named \`weatherDashboard\`:

\`\`\`bash
git clone https://github.com/mdrmx/esp32-gadget.git weatherDashboard
cd weatherDashboard
\`\`\`

This changes only the local checkout directory name; it does not rename or move tracked repository files. The requirement is described in the [Arduino sketch specification](https://docs.arduino.cc/arduino-cli/sketch-specification/).

### 2.2 Build configuration currently tracked

The inspected repository does not contain a \`platformio.ini\`, \`arduino-cli.yaml\`, \`library.properties\` or explicit checked-in board/FQBN configuration. The Step 2 audit also found no PlatformIO, CMake or test-project configuration at the inspected baseline.

The repository therefore does not currently pin:
- Arduino IDE or Arduino CLI version.
- Espressif ESP32 Arduino core version.
- Board FQBN or board-menu options.
- Compiler/toolchain version.
- External Arduino library versions.

Do not add version pins until they have been confirmed against the environment that successfully builds and runs this exact firmware.

### 2.3 Board selection evidence

The hardware baseline is the Waveshare ESP32-S3 Touch LCD 2. Waveshare's Arduino instructions identify the Espressif ESP32 board package and use **ESP32S3 Dev Module** as the board selection example. They also describe enabling **USB CDC On Boot** when using the board's native USB interface.

Reference: [Waveshare ESP32-S3 Touch LCD 2 — Arduino instructions](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-2).

This is vendor guidance, not proof of the exact menu options used for the current working firmware. Record the actual selected board and every non-default option from the development machine. In particular, do not assume PSRAM, flash mode/size, partition scheme, USB mode or upload settings from the generic board name alone.

### 2.4 Source-level dependencies

The current firmware's direct includes fall into two groups.

**ESP32 Arduino core / platform headers:**

- \`WiFi.h\`
- \`WiFiClientSecure.h\`
- \`HTTPClient.h\`
- \`Wire.h\`
- \`time.h\` and \`sys/time.h\`
- \`esp_heap_caps.h\` and \`esp_memory_utils.h\`

These are supplied by the ESP32 Arduino platform / underlying ESP32 toolchain rather than being separate Library Manager dependencies.

**External Arduino libraries identified from the sketch includes:**

| Include | Library dependency | Version status |
|---|---|---|
| \`ArduinoJson.h\` | ArduinoJson | Not pinned in repository |
| \`Arduino_GFX_Library.h\` | GFX Library for Arduino / Arduino_GFX | Not pinned in repository |
| \`PNGdec.h\` | PNGdec | Not pinned in repository |

The exact installed release of each library must be captured from the working environment. This table reflects direct source includes; it is not a claim that these are the only transitive dependencies.

### 2.5 Local credentials required by the source

The sketch includes \`secrets.h\` directly. The file is ignored by Git, and the source references these mandatory macros:

- \`WIFI_SSID\`
- \`WIFI_PASSWORD\`
- \`OPENWEATHER_API_KEY\`

The Google Geolocation key is optional; the setup guide says that when it is absent, Google Wi-Fi positioning is skipped and the firmware falls back to IP geolocation.

A safe template is now provided as \`secrets.example.h\`. For a local build:

1. Copy \`secrets.example.h\` to \`secrets.h\`.
2. Enter the local Wi-Fi and OpenWeather values in \`secrets.h\`.
3. Add the Google key only when using that service.
4. Keep \`secrets.h\` untracked. Do not paste credentials into build logs, issues or pull requests.

The tracked example contains placeholders only. A successful compile does not validate that the credentials or API services work.

## 3. Clean-checkout preparation

From the parent directory in which the project should live:

\`\`\`bash
git clone https://github.com/mdrmx/esp32-gadget.git weatherDashboard
cd weatherDashboard
cp secrets.example.h secrets.h
\`\`\`

On Windows, copy \`secrets.example.h\` to \`secrets.h\` using File Explorer or PowerShell:

\`\`\`powershell
Copy-Item secrets.example.h secrets.h
\`\`\`

Edit only the local \`secrets.h\` with the real values. Before committing, check:

\`\`\`bash
git status --short
\`\`\`

The local \`secrets.h\` must not appear as an untracked or staged file. The existing \`.gitignore\` rule excludes it.

## 4. Capture the real environment

Complete this table on the machine that currently builds and uploads the device. Do not substitute guessed or latest versions.

| Item | Value to record | Current evidence |
|---|---|---|
| Operating system | OS and version | Not recorded in repository |
| IDE / CLI | Arduino IDE version, or Arduino CLI version | Not recorded |
| Board platform | Espressif \`esp32\` core version | Not recorded |
| Board selection | Exact board name and FQBN | Vendor example: ESP32S3 Dev Module; exact selection unverified |
| Board options | USB CDC, flash, PSRAM, partition, upload settings and other selected options | Not recorded |
| Compiler | Compiler / toolchain version reported during verbose build | Not recorded |
| ArduinoJson | Installed library version | Not recorded |
| Arduino_GFX | Installed library version | Not recorded |
| PNGdec | Installed library version | Not recorded |
| Build command | Exact command or IDE action and configuration | Not recorded |
| Build result | Pass/fail and complete compiler output | Not yet run in this audit |

For Arduino CLI, the following commands capture the installed environment and available board definition:

\`\`\`bash
arduino-cli version
arduino-cli core list
arduino-cli lib list
arduino-cli board listall
\`\`\`

After identifying the exact FQBN and selected board options, inspect that board definition and compile the sketch directory:

\`\`\`bash
arduino-cli board details --fqbn <FQBN>
arduino-cli compile --fqbn <FQBN> --warnings all --verbose /path/to/weatherDashboard
\`\`\`

Replace \`<FQBN>\` with the actual FQBN, including any selected option values required by the installed core. Do not copy a generic FQBN into the project as a substitute for recording the working configuration.

If the project is currently built with Arduino IDE rather than Arduino CLI, capture the IDE version from **Help → About**, the selected board and all **Tools** menu options, and the installed library versions from Library Manager. A CLI build should not be treated as equivalent until it has been shown to use the same platform and options.

## 5. Build verification checklist

The build/toolchain step can be marked complete only when the following evidence exists:

- [ ] A clean checkout can be prepared using the documented sketch-folder name.
- [ ] A local \`secrets.h\` can be created from the example without adding secrets to Git.
- [ ] Exact IDE / CLI, ESP32 core, board FQBN and board-menu options are recorded.
- [ ] Exact installed versions of ArduinoJson, Arduino_GFX and PNGdec are recorded.
- [ ] A clean compile succeeds from the documented checkout, with the full build output retained.
- [ ] The firmware uploads and boots on the physical Waveshare device.
- [ ] The board / build configuration is recorded in a form that another checkout can reproduce.

The final two checks require the physical device and its working development environment. They are deliberately left unchecked here.

## 6. Risks and follow-on work

1. **Sketch-folder naming:** a default clone directory named \`esp32-gadget\` does not match the primary sketch file \`weatherDashboard.ino\`. The documented clone-directory workaround avoids changing the firmware layout in this audit.
2. **Unpinned dependencies:** updates to the ESP32 core or external libraries could change compilation, memory use or runtime behaviour. Pin only after identifying the known-good versions.
3. **Board configuration:** the generic ESP32-S3 board selection is not a complete record of PSRAM, flash, USB and partition settings.
4. **Secrets:** a fresh checkout needs a local \`secrets.h\`; the example template makes the required macro names discoverable without publishing credentials.
5. **Demo-mode documentation:** the source currently sets \`USE_DEMO_WEATHER_DATA\` to \`0\`, while \`DEVELOPMENT_MODE.md\` describes \`1\` as the current default. This remains a separate Phase 0 documentation-reconciliation item and is not changed in this step.

## 7. Step outcome

**Completed in this repository step:** static build-layout and source-dependency audit; documented clean-checkout preparation; added a placeholder-only secrets template; identified the evidence needed to reproduce the build.

**Still outstanding:** capture the actual local IDE / CLI and dependency versions, confirm the board options, produce a clean successful build, and verify upload / boot on hardware.

No firmware source or runtime behaviour was changed.
