zephyr_library()
zephyr_library_sources_ifdef(CONFIG_KLEO_INA260
  drivers/ina260_core.c drivers/ina260_zephyr.c)
