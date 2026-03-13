# WiFi LED Module Split Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split the current single-file Wi-Fi and LED implementation into focused modules without changing runtime behavior.

**Architecture:** Keep `main.c` as the orchestration layer. Move LED GPIO behavior into `led.c/.h` and Wi-Fi station logic into `wifi_manager.c/.h`, with `main.c` depending only on the public module APIs.

**Tech Stack:** ESP-IDF, FreeRTOS, ESP Wi-Fi, GPIO, Kconfig

---

## Chunk 1: Module Boundaries

### Task 1: Add LED module

**Files:**
- Create: `main/include/led.h`
- Create: `main/src/led.c`

- [ ] **Step 1: Define the public LED API**
- [ ] **Step 2: Move GPIO init/set/toggle logic into `led.c`**
- [ ] **Step 3: Keep LED state private to the module**

### Task 2: Add Wi-Fi manager module

**Files:**
- Create: `main/include/wifi_manager.h`
- Create: `main/src/wifi_manager.c`

- [ ] **Step 1: Define the public Wi-Fi API**
- [ ] **Step 2: Move station init and event handling into `wifi_manager.c`**
- [ ] **Step 3: Keep connection state private to the module**

## Chunk 2: Main Orchestration

### Task 3: Reduce `main.c` to boot flow and blink loop

**Files:**
- Modify: `main/src/main.c`

- [ ] **Step 1: Remove Wi-Fi and LED implementation details from `main.c`**
- [ ] **Step 2: Call module APIs from `app_main()`**
- [ ] **Step 3: Preserve existing blink-only-when-connected behavior**

### Task 4: Update build configuration

**Files:**
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: Add `src/led.c` and `src/wifi_manager.c` to component sources**
- [ ] **Step 2: Keep required ESP-IDF component dependencies**

## Chunk 3: Verification

### Task 5: Verify consistency

**Files:**
- Review: `main/src/main.c`
- Review: `main/src/led.c`
- Review: `main/src/wifi_manager.c`
- Review: `main/CMakeLists.txt`

- [ ] **Step 1: Confirm only one compiled `app_main()` remains**
- [ ] **Step 2: Confirm headers expose only the public APIs**
- [ ] **Step 3: Run `idf.py build` in an ESP-IDF environment**

Plan complete and saved to `docs/superpowers/plans/2026-03-12-wifi-led-module-split.md`. Ready to execute.
