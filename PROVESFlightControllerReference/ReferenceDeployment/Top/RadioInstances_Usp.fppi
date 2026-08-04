# Radio instance declarations for v5e (Semtech USP path).
# Included by instances.fpp inside module ReferenceDeployment { }.
# Selected by CMakeLists.txt when CONFIG_LORA_BASICS_MODEM_DRIVERS is set.
#
# UspRadio is an ACTIVE component (deferred-handler SBand pattern).
# Queue = 20 (QUEUE_SIZE*2), stack = 4K, priority 11 (above rate groups).

  instance uspRadio: Zephyr.UspRadio base id 0x1001F000 \
    queue size Default.QUEUE_SIZE * 2 \
    stack size Default.STACK_SIZE \
    priority 11
