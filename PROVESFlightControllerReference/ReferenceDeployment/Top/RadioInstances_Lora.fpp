module ReferenceDeployment {

  # ----------------------------------------------------------------------
  # Radio instance: Zephyr::LoRa (v5c / v5d — legacy path)
  #
  # Selected by CMakeLists.txt when CONFIG_LORA_BASICS_MODEM_DRIVERS is not
  # set (i.e. any board that uses the Zephyr in-tree LoRa driver).
  # ----------------------------------------------------------------------

  instance lora: Zephyr.LoRa base id 0x1001F000

  instance loraRetry: Svc.ComRetry base id 0x10063000

}
