zephyr_include_directories(include)

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_PROVES_GPIO_LATCH_SIM gpio_latch_sim.c)
zephyr_library_sources_ifdef(CONFIG_PROVES_LORA_SIM lora_sim.c)
zephyr_library_sources_ifdef(CONFIG_PROVES_RTC_SIM rtc_sim.c)
zephyr_library_sources_ifdef(CONFIG_PROVES_SENSOR_SIM sensor_sim.c)
zephyr_library_sources_ifdef(CONFIG_PROVES_WATCHDOG_SIM watchdog_sim.c)
# rtc_sim.c is a copy of Zephyr's rtc_emul.c and uses that directory's private headers
zephyr_library_include_directories(${ZEPHYR_BASE}/drivers/rtc)

# native_sim links in one stage, but fprime-zephyr's deployment target waits on zephyr_final
add_custom_target(zephyr_final)
add_dependencies(zephyr_final native_runner_executable)
