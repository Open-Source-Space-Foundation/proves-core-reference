# How to Start a Zephyr Driver

A Zephyr driver needs **four pieces** so the build finds it, validates the devicetree, and links the code.

## 1. Devicetree binding (YAML)

**Where:** `dts/bindings/<category>/<compatible>.yaml`
Example: `dts/bindings/memory-controllers/aps,aps1604m.yaml`

**Purpose:** Describes the devicetree node (which properties it has, required vs optional). The build uses this to validate your `.dts`/`.dtsi` and to generate `DT_*` macros.

**Minimal content:**
- `compatible: "vendor,model"` (must match the node’s `compatible`)
- `include: base.yaml`
- `properties:` for `reg`, and any other props (e.g. `spi-max-frequency`)

## 2. Kconfig option

**Where:** `drivers/<subsystem>/Kconfig` (or a file you `rsource` from the app `Kconfig`)

**Purpose:** Lets you enable the driver (`CONFIG_PSRAM_APS1604M=y`) and gates compilation.

**Pattern:**
```kconfig
config PSRAM_APS1604M
	bool "APMemory APS1604M PSRAM"
	default y
	depends on DT_HAS_APS_APS1604M_ENABLED
	help
	  Enable APS1604M QSPI PSRAM driver.
```

`DT_HAS_APS_APS1604M_ENABLED` is generated from the binding when a node has `compatible = "aps,aps1604m"` and `status = "okay"`.

## 3. CMakeLists.txt (driver build)

**Where:** Either in the app’s top-level `CMakeLists.txt` or in `drivers/<subsystem>/CMakeLists.txt` if you use a Zephyr module.

**Purpose:** Compile the driver and link it into the app.

**Application-local driver (simplest):** In the app’s top-level `CMakeLists.txt`, after `find_package(Zephyr)` and `project(...)`:
```cmake
target_sources(app PRIVATE drivers/psram/aps1604m.c)
```

**As a Zephyr module:** Use `drivers/psram/CMakeLists.txt` with `zephyr_library()` and `zephyr_library_sources(...)`, then add the module via `EXTRA_ZEPHYR_MODULES` or `add_subdirectory` and `target_link_libraries(app PRIVATE psram)`. See [Zephyr modules](https://docs.zephyrproject.org/latest/guides/modules.html).

## 4. Driver implementation (C)

**Where:** `drivers/<subsystem>/aps1604m.c` (or similar)

**Purpose:** Implement init and the driver API (read/write, get size, etc.).

**Pattern:**
- `#define DT_DRV_COMPAT aps_aps1604m` (matches `compatible` with commas → underscores)
- Config struct filled from devicetree: `DT_INST_PROP(inst, reg)`, `DT_INST_PROP(inst, spi_max_frequency)`, etc.
- Init function: e.g. configure hardware, verify device ID.
- `DEVICE_DT_INST_DEFINE(inst, init, ...)` and `DT_INST_FOREACH_STATUS_OKAY(MACRO)` to instantiate for every `status = "okay"` node.

**APIs:** Use an existing subsystem (e.g. `drivers/flash.h`, `drivers/eeprom.h`) or define a custom API and implement it in the driver.

## Order to add them

1. **Binding** – so the node is valid and `DT_HAS_*_ENABLED` exists.
2. **Kconfig** – so you can turn the driver on.
3. **CMakeLists.txt** – so the driver is compiled and linked.
4. **C implementation** – start with init and one API (e.g. “get size”); add read/write using the hardware (QSPI, SPI, or memory-mapped) next.

## References

- [Zephyr Devicetree bindings](https://docs.zephyrproject.org/latest/build/dts/bindings-intro.html)
- [Zephyr driver model](https://docs.zephyrproject.org/latest/kernel/drivers/index.html)
- In-tree examples: `drivers/eeprom/eeprom_mb85rsxx.c`, `drivers/flash/flash_rpi_pico.c`
