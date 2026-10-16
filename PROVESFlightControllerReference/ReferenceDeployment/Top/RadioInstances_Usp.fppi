# Radio instances for v5e (Semtech USP driver).
# Included by instances.fpp inside module ReferenceDeployment { }.
# Selected by Top/CMakeLists.txt per board.
#
# UspRadio is an active component. Priority 11 is above the rate groups.

  instance uspRadio: Zephyr.UspRadio base id 0x1001F000 \
    queue size Default.QUEUE_SIZE * 2 \
    stack size Default.STACK_SIZE \
    priority 11
