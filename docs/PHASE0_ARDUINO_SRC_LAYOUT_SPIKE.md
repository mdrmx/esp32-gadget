# Phase 0 — Arduino `src/` layout compile spike

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

## Pass criteria

- The sketch header include `phase0/CompileLayoutSpike.h` resolves.
- `src/phase0/CompileLayoutSpike.cpp` is compiled.
- The final firmware image links without an undefined reference to `phase0_layout_spike::linkCheck()`.
- The compile succeeds with the exact recorded board/toolchain configuration.

## Failure handling

If the header include cannot be resolved, or the source file is not compiled/linked, stop. Record the actual compiler output and adjust the proposed directory/include pattern before moving any production code.

## Cleanup

This branch is disposable test scaffolding. Once the compile result is recorded:
- Remove the spike files and include/call rather than merging the test marker into production; or
- Use the verified include/source pattern in the first real extraction PR and omit the no-op spike symbol.

Do not treat a successful compile as validation of runtime behaviour. No hardware interaction is intended by this check.
