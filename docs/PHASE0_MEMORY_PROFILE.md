# Phase 0 — On-device memory and PSRAM profile

**Status:** Initial live-mode hardware profile complete  
**Hardware:** Waveshare ESP32-S3 Touch LCD 2  
**Firmware under test:** temporary Phase 0 diagnostics branch, `test/phase0-step4-diagnostics`, commit `59f2011f83b1ac8ce57d51d0f2760d84ecf4f6cd`  
**Production status:** diagnostics were not merged into `main`; do not merge this test-only branch.

## 1. Purpose and interpretation

This profile records the memory behaviour observed on the physical board before the platform refactor. It establishes a comparison point for later changes to canvas allocation, weather/forecast data, icon caching and network services.

The diagnostic firmware reports:

- PSRAM total, current free space, largest free block and minimum-ever free space.
- Internal 8-bit heap current free space, largest free block and minimum-ever free space.
- The framebuffer pointer, requested byte count and detected memory region for each canvas.

The `minimum-ever` values are heap low-water marks **since boot**. They are not per-operation allocation counters. Compare them with current free space and largest-block values; do not interpret a low-water mark as memory that remains allocated.

This is an initial manual test sequence, not a long-duration soak test or proof that every failure path is leak-free.

## 2. Test environment

| Item | Recorded value |
|---|---|
| Board | Waveshare ESP32-S3 Touch LCD 2 |
| Arduino IDE | 2.3.10 |
| ESP32 Arduino core | 3.3.12 |
| FQBN | `esp32:esp32:esp32s3:CDCOnBoot=cdc,UploadMode=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi` |
| GCC | 14.2.0 |
| Live/demo mode | Live mode (`USE_DEMO_WEATHER_DATA 0`) |
| Diagnostic Serial baud | 115200 |
| Diagnostic revision | `59f2011f83b1ac8ce57d51d0f2760d84ecf4f6cd` |

The local `secrets.h` remains ignored by Git. No credentials are part of this record.

## 3. Canvas allocation

The firmware requests three RGB565 framebuffers:

| Canvas | Requested dimensions | Requested bytes | Detected region |
|---|---:|---:|---|
| Forecast viewport | 320 × 152 | 97,280 | PSRAM |
| Weather metrics | 118 × 177 | 41,772 | PSRAM |
| Tab transition | 320 × 240 | 153,600 | PSRAM |
| **Total** | | **292,652** | **All PSRAM** |

The live diagnostic snapshots around allocation were:

| Metric | Before canvas allocation | After canvas allocation |
|---|---:|---:|
| PSRAM free | 8,386,096 B | 8,093,396 B |
| PSRAM largest block | 8,257,524 B | 7,995,380 B |
| PSRAM minimum-ever free | 8,386,096 B | 8,093,396 B |
| Internal 8-bit free | 260,404 B | 260,104 B |
| Internal 8-bit largest block | 196,596 B | 196,596 B |
| Internal 8-bit minimum-ever free | 255,008 B | 254,608 B |

PSRAM free space fell by 292,700 bytes against 292,652 requested framebuffer bytes, a 48-byte difference. The internal free heap fell by 300 bytes and its largest block was unchanged.

**Conclusion:** all three buffers were placed in PSRAM on this board/configuration. The framebuffer allocations did not materially consume the internal heap.

## 4. Settled startup memory

The first manual snapshot after startup reported:

| Metric | Value |
|---|---:|
| PSRAM total | 8,386,608 B |
| PSRAM free | 8,079,348 B |
| PSRAM largest block | 7,995,380 B |
| PSRAM minimum-ever free | 8,067,260 B |
| Internal 8-bit free | 213,608 B |
| Internal 8-bit largest block | 118,772 B |
| Internal 8-bit minimum-ever free | 110,532 B |

Compared with the immediate post-canvas snapshot, settled startup had 46,496 fewer bytes of current internal free heap and a 77,824-byte smaller largest internal block. PSRAM free space was 14,048 bytes lower.

The measurements show that the material change occurred during startup and service initialisation, not progressively during the later manual refreshes. They do **not** identify which startup allocation caused the change. TLS/network clients, JSON parsing and other startup work are plausible contributors, but were not isolated by this test.

The initial startup log also reported a 1,457 ms TimeAPI request and a 3,051 ms `connectWiFi()` duration including the due time sync. Wi-Fi positioning then scanned 15 networks, retained 8 usable BSSIDs and obtained a Google position with an approximately 17 m reported accuracy radius. These are observations from this run, not fixed timing guarantees.

## 5. Manual operation results

### 5.1 Current weather refresh

| Stage | PSRAM free | Internal free | Internal largest block |
|---|---:|---:|---:|
| Before request | 8,079,348 B | 213,560 B | 118,772 B |
| After weather data | 8,078,952 B | 213,560 B | 118,772 B |
| After render | 8,079,092 B | 213,560 B | 118,772 B |

- Weather request: 462 ms.
- Icon cache check: 0 ms (cached icon).
- No change in current internal free heap or largest block.

### 5.2 Hourly forecast refresh

| Stage | PSRAM free | Internal free | Internal largest block |
|---|---:|---:|---:|
| Before request | 8,079,092 B | 213,608 B | 118,772 B |
| After forecast data | 8,078,836 B | 213,552 B | 118,772 B |
| After render | 8,078,844 B | 213,552 B | 118,772 B |

- Forecast page 1: 645 ms.
- Forecast page 2: 95 ms.
- Complete forecast operation: 742 ms.
- Icon cache check: 0 ms.
- 10-second summary: maximum touch-poll gap 845 ms, maximum loop 845 ms, 297 loops.

The internal free-heap change was 56 bytes and the largest internal block did not change.

### 5.3 Forced icon cache miss

| Stage | PSRAM free | Internal free | Internal largest block |
|---|---:|---:|---:|
| Before forced cache miss | 8,080,412 B | 213,608 B | 118,772 B |
| After cache clear | 8,080,412 B | 214,572 B | 118,772 B |
| Before icon buffer allocation | 8,080,060 B | 168,120 B | 118,772 B |
| After icon buffer allocation | 8,080,060 B | 167,156 B | 118,772 B |
| After cache update | 8,080,060 B | 167,156 B | 118,772 B |
| After operation completed | 8,080,156 B | 213,348 B | 118,772 B |

- Icon download/cache operation: 564 ms.
- Reported maximum loop duration and touch-poll gap: 667 ms.
- Internal free heap temporarily fell by about 46 KB during the operation, then recovered to within 260 bytes of the pre-test value.
- The largest internal block remained unchanged; the low-water mark did not fall further.

**Conclusion:** the tested cache-miss path produced temporary internal heap pressure, but the memory was substantially recovered when the operation completed. The test does not prove that every possible PNG size or failed-download path behaves identically.

### 5.4 Combined weather and forecast refresh

| Stage | PSRAM free | Internal free | Internal largest block |
|---|---:|---:|---:|
| Before full refresh | 8,080,412 B | 213,608 B | 118,772 B |
| After data retrieval | 8,079,900 B | 213,136 B | 118,772 B |
| After render | 8,079,900 B | 213,560 B | 118,772 B |
| Final manual snapshot | 8,080,148 B | 213,608 B | 118,772 B |

- Weather request: 448 ms.
- Forecast page 1: 558 ms.
- Forecast page 2: 95 ms.
- Complete forecast operation: 655 ms.
- Combined request time: approximately 1,103 ms.
- Maximum loop duration: 1,207 ms.
- The same summary reported a maximum touch-poll gap of 31 ms.

The internal free heap fell by 472 bytes during data retrieval and recovered 424 bytes after rendering. The final manual snapshot returned to the same 213,608-byte internal free-heap value as before the full refresh. The largest internal block remained unchanged.

## 6. Diagnostic timing caveat

The full-refresh summary's 1,207 ms maximum loop and 31 ms maximum touch-poll gap must not be read as evidence of responsive touch during the blocking request.

Inspection of the temporary diagnostic code shows that it:

1. Samples the touch-poll gap at the beginning of a loop, using the difference from the previous loop-start timestamp.
2. Measures the current loop duration at the end of that loop.
3. Can print and reset the 10-second summary at the end of the current loop.

A long blocking loop is therefore added to the maximum-loop figure before the corresponding gap can be observed at the next loop start. If the summary resets between those points, the two maxima fall into different reporting windows. The 31 ms figure is consequently not a valid same-window measurement for that full-refresh operation.

The touch-poll metric needs its reporting window aligned with the poll interval before it is used to assert a maximum input-service gap. Physical touch-to-visible-response latency was not instrumented in this test.

## 7. Conclusions and refactor constraints

1. **Framebuffer placement is verified:** the three existing canvases allocate in PSRAM using the recorded board configuration.
2. **Internal heap headroom is finite but stable in the tested sequence:** the settled snapshot had 213,608 bytes free and a largest contiguous block of 118,772 bytes; the minimum-ever free value since boot was 110,532 bytes.
3. **No progressive loss was observed:** manual weather, forecast and combined refreshes did not produce a cumulative drop in the final internal free heap or largest internal block.
4. **Icon download is a temporary pressure point:** the tested cache miss temporarily consumed about 46 KB of internal free heap, then substantially recovered.
5. **Startup use deserves separate attribution:** settled startup differs materially from the immediate post-canvas snapshot, but this test did not identify the allocation responsible.
6. **Synchronous network work remains the responsiveness risk:** operation and loop measurements show blocking intervals; the misaligned touch-gap summary must not be used as a counterclaim.
7. **Do not allocate a full-screen framebuffer per application:** retain the existing buffer budget until a shared display/canvas ownership design is justified by measurements.

## 8. Follow-up tests

These were not completed by the memory sequence and remain useful later:

- Repeated cache misses across different valid icon codes, including a failed/short download.
- Longer soak test with several scheduled refreshes and repeated screen changes.
- Controlled HTTP failure and Wi-Fi loss/recovery.
- Instrumented touch-poll gap with a corrected reporting window and physical touch-to-response latency.
- Metric animation and tab-transition touch behaviour.

No broad memory-allocation redesign or asynchronous networking implementation is justified by this profile alone. Use these figures as the Phase 0 comparison baseline for Phase 1 extraction PRs.
