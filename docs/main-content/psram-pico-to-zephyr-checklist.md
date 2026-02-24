# PSRAM driver: Pico SDK → Zephyr porting checklist

Use this when porting or maintaining `sfe_psram_zephyr.cpp` (from Pico SDK–based `sfe_psram.c`).

---

## 1. Build / CMake

| Pico SDK | Zephyr | Notes |
|----------|--------|--------|
| Built with pico-sdk `add_executable` / `target_sources` | Zephyr `app` target or OBJECT library; sources added in deployment `CMakeLists.txt` | Ensure `sfe_psram_zephyr.cpp` is in the Zephyr app (or a library linked to `app`). |
| Pico SDK include path (e.g. `pico_sdk_import.cmake`) | HAL: `lib/zephyr-workspace/modules/hal/rpi_pico/src/.../include` **or** `PICO_SDK_PATH` | Add HAL/SDK include dirs to the target that compiles the PSRAM file (see ReferenceDeployment `CMakeLists.txt`). |
| — | Clean reconfigure after CMake changes | `rm -rf build-fprime-automatic-zephyr` then rebuild so new sources/targets are applied. |

---

## 2. Headers and APIs to replace or keep

| Item | Pico SDK | Zephyr / port | Status |
|------|----------|----------------|--------|
| **Interrupt disable/restore** | `save_and_disable_interrupts()` / `restore_interrupts()` | `irq_lock()` / `irq_unlock()` | ✅ Replaced (same PRIMASK behavior). |
| **System clock frequency** | `clock_get_hz(clk_sys)` | `sys_clock_hw_cycles_per_sec()` | ✅ Replaced. |
| **Code in RAM (no flash)** | `__no_inline_not_in_flash_func(name)` | Define `#define __no_inline_not_in_flash_func(x) x` when undefined | ✅ Macro added; Zephyr build runs from flash (OK unless you need RAM execution). |
| **QMI / XIP registers** | `hardware/structs/qmi.h`, `hardware/structs/xip_ctrl.h`, `hardware/address_mapped.h` | Same headers from **Zephyr HAL** (`hal_rpi_pico`) or **Pico SDK** | Keep; no Zephyr replacement. Include paths must point at HAL or SDK. |
| **GPIO pin function** | `gpio_set_function(pin, GPIO_FUNC_XIP_CS1)` | Same (from `hardware/gpio.h` in HAL/SDK) | Keep; RP2350-specific, no generic Zephyr API. |
| **Printf** | `printf` | `printk` (optional) | Optional; use `printk` for Zephyr logging. |

---

## 3. Things that must stay (SoC-specific)

- **`qmi_hw`**, **`xip_ctrl_hw`** – RP2350 QMI/XIP register blocks; from `qmi.h` / `xip_ctrl.h`.
- **`gpio_set_function(psram_cs_pin, GPIO_FUNC_XIP_CS1)`** – Muxes the CS pin to XIP_CS1.
- **All QMI_* and XIP_* constants** – From `hardware/regs/qmi.h` and HAL/SDK.
- **`io_rw_32`, `io_wo_32`, `io_ro_32`** – From `hardware/address_mapped.h`.

These have no generic Zephyr equivalent; they come only from Pico SDK or Zephyr HAL (hal_rpi_pico).

---

## 4. Optional / behavior

| Topic | Pico SDK | Zephyr | Suggestion |
|-------|----------|--------|------------|
| **PSRAM CS pin** | Hardcoded or config | Devicetree `psram0` `cs-gpios` | Use `DT_GPIO_PIN(DT_NODELABEL(psram0), cs_gpios)` in app and pass to `sfe_setup_psram()`. |
| **Running critical code from RAM** | `__not_in_flash_func` / `.time_critical` section | No standard macro | Either keep `__no_inline_not_in_flash_func(x) x` (run from flash) or add a custom section + linker script for RAM. |
| **Clock source for timing** | `clk_sys` | `sys_clock_hw_cycles_per_sec()` (kernel tick source) | If your system clock differs from the tick source, consider a different API or Kconfig for PSRAM clock. |

---

## 5. Replacements already done in `sfe_psram_zephyr.cpp`

- [x] `irq_lock()` / `irq_unlock()` instead of save/restore interrupts.
- [x] `sys_clock_hw_cycles_per_sec()` instead of `clock_get_hz(clk_sys)`.
- [x] `__no_inline_not_in_flash_func` defined when not present (identity macro).
- [x] `hardware/*` includes uncommented; built with HAL or SDK include paths.
- [x] Public API (`sfe_setup_psram`, `sfe_psram_update_timing`) wrapped in `extern "C"` in the .cpp so C++ callers link correctly.
- [x] Deployment links PSRAM lib: in `ReferenceDeployment/CMakeLists.txt`, `target_link_libraries(app PRIVATE sparkfun_pico_sfe_psram_zephyr)` after `register_fprime_zephyr_deployment`.

---

## 6. If you add more Pico SDK code

- **Sync/atomic** – Prefer Zephyr `irq_lock`/`irq_unlock` or `k_spin_lock`; avoid Pico `sync_*` unless you keep a small wrapper.
- **Sleep/delay** – Use `k_msleep()` / `k_usleep()` or `k_busy_wait()` instead of Pico `sleep_*`.
- **Assert** – Use `__ASSERT()` or `k_panic()` (Zephyr) instead of Pico assert.
- **Attributes** – Any `__attribute__((section(...)))` or `__not_in_flash`-style macros need a Zephyr-side definition or strip.

---

## 7. Quick reference: HAL vs Pico SDK

| Need | Zephyr HAL (hal_rpi_pico) | Pico SDK |
|------|---------------------------|----------|
| `hardware/structs/qmi.h`, `xip_ctrl.h` | Yes (if HAL has RP2350) | Yes |
| `hardware/gpio.h`, `gpio_set_function` | Yes | Yes |
| `__no_inline_not_in_flash_func` | No | Yes | Define macro in our code when building for Zephyr. |
| Include path | `modules/hal/rpi_pico/src/.../include` | `PICO_SDK_PATH/src/.../include` |

Use one of: HAL include path **or** `PICO_SDK_PATH`; no need for both.
