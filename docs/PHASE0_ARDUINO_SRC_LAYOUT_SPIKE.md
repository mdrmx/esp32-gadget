# Phase 0 — Arduino `src/` layout compile spike

**Status: PASS — compile and link verified on the recorded development toolchain (2026-11-01).**

**Purpose:** verify that the recorded Arduino IDE / ESP32 board toolchain compiles a small header and implementation placed under the sketch's `src/` directory before production code is moved into modules.

**Branch:** `test/phase0-arduino-src-layout-spike`  
**Base:** `main` at the Phase 0 documentation baseline.  
**Scope:** compile and link only. No upload or runtime validation is required.

## What the spike tests

- The root `weatherDashboard.ino` can include a header from `src/phase0/`.
- Arduino's sketch build compiles the `.cpp` file under `src/phase0/`.
- The sketch can link against a function defined in that `.cpp`.
- No production Weather, touch, rendering, networking or allocation logic is moved.

The temporary sketch call to `phase0_layout_spike::linkCheck()` forces the linker to resolve the symbol from the `src/` implementation. A compile-time assertion verifies the header was included.

## Test procedure

1. Check out this branch into a local folder named **`weatherDashboard`**. The folder name must match the root sketch filename for the normal Arduino sketch workflow.
2. Preserve the existing local `secrets.h`. Do not replace it with the placeholder file and do not commit or share it.
3. Open `weatherDashboard.ino` in Arduino IDE 2.3.10.
4. Select the recorded board and exact FQBN from [Step 3](PHASE0_STEP3_BUILD_TOOLCHAIN.md):
   `esp32:esp32:esp32s3:CDCOnBoot=cdc,UploadMode=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`.
5. Run **Verify** only. Do not upload; this is a compile/link test.

## Recorded result — PASS

The supplied verbose Arduino IDE build log for this branch confirms:

- The sketch was built from `/Users/mdrmx/Documents/weatherDashboard`, matching the root `weatherDashboard.ino` name.
- FQBN: `esp32:esp32:esp32s3:CDCOnBoot=cdc,UploadMode=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`.
- ESP32 Arduino core: 3.3.12.
- GCC toolchain: 14.2.0 (ESP32-S3 Xtensa toolchain).
- esptool: 5.3.1.
- The sketch-root include `src/phase0/CompileLayoutSpike.h` resolved.
- The compiler compiled `src/phase0/CompileLayoutSpike.cpp`; the linker command included `sketch/src/phase0/CompileLayoutSpike.cpp.o`.
- The ESP32-S3 binary image was generated successfully. No compiler or linker error appears in the supplied log.

Build output:
- Program storage: **1,150,479 bytes (36%) of 3,145,728 bytes**.
- Global variables: **97,196 bytes (29%) of 327,680 bytes**.
- Remaining dynamic memory reported by the build: **230,484 bytes**.

This is a compile/link validation only. The log does not show a firmware upload or runtime test. The production code was not refactored by this spike.

## Pass criteria

- The sketch header include `src/phase0/CompileLayoutSpike.h` resolves.
- `src/phase0/CompileLayoutSpike.cpp` is compiled.
- The final firmware image links without an undefined reference to `phase0_layout_spike::linkCheck()`.
- The compile succeeds with the exact recorded board/toolchain configuration. (Verified: 2026-11-01.)

## Failure handling

If the header include cannot be resolved, or the source file is not compiled/linked, stop. Record the actual compiler output and adjust the proposed directory/include pattern before moving any production code.

## Cleanup

This branch is disposable test scaffolding. Once the compile result is recorded:
- Remove the spike files and include/call rather than merging the test marker into production; or
- Use the verified include/source pattern in the first real extraction PR and omit the no-op spike symbol.

Do not treat a successful compile as validation of runtime behaviour. No hardware interaction is intended by this check.
