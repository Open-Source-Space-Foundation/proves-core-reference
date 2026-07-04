module ReferenceDeployment {

  # ----------------------------------------------------------------------
  # Radio instance: Zephyr::UspRadio (v5e+ — Semtech USP path)
  #
  # Selected by CMakeLists.txt when CONFIG_LORA_BASICS_MODEM_DRIVERS is set
  # (i.e. boards with the SX1262 USP driver enabled in their defconfig).
  #
  # UspRadio is an ACTIVE component (deferred-handler SBand pattern).
  # Queue / stack sizes match the component's expected message depth.
  # Priority 11 (above rate groups, below sequencer).
  # ----------------------------------------------------------------------

  instance uspRadio: Zephyr.UspRadio base id 0x1001F000 \
    queue size Default.QUEUE_SIZE * 2 \
    stack size Default.STACK_SIZE \
    priority 11

}
