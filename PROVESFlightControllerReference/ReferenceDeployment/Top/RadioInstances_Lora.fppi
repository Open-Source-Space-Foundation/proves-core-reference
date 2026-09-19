# Radio instances for v5c / v5d (Zephyr LoRa driver).
# Included by instances.fpp inside module ReferenceDeployment { }.
# Selected by Top/CMakeLists.txt per board.

  instance lora: Zephyr.LoRa base id 0x1001F000

  instance loraRetry: Svc.ComRetry base id 0x10063000
