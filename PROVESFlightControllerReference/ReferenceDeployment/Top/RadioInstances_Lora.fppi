# Radio instance declarations for v5c / v5d (Zephyr LoRa legacy path).
# Included by instances.fpp inside module ReferenceDeployment { }.
# Selected by CMakeLists.txt when CONFIG_LORA_BASICS_MODEM_DRIVERS is not set.

  instance lora: Zephyr.LoRa base id 0x1001F000

  instance loraRetry: Svc.ComRetry base id 0x10063000
