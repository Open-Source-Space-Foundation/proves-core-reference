module NiclaDeployment {

  # ----------------------------------------------------------------------
  # Base ID Convention
  # ----------------------------------------------------------------------
  #
  # All Base IDs follow the 8-digit hex format: 0xDSSCCxxx
  #
  # Where:
  #   D   = Deployment digit (1 for this deployment)
  #   SS  = Subtopology digits (00 for main topology, 01-05 for subtopologies)
  #   CC  = Component digits (00, 01, 02, etc.)
  #   xxx = Reserved for internal component items (events, commands, telemetry)
  #

  # ----------------------------------------------------------------------
  # Defaults
  # ----------------------------------------------------------------------

  module Default {
    constant QUEUE_SIZE = 10
    constant STACK_SIZE = 4 * 1024 # Must match CONFIG_DYNAMIC_THREAD_STACK_SIZE in prj.conf
  }

  # ----------------------------------------------------------------------
  # Active component instances
  # ----------------------------------------------------------------------

  instance rateGroup1Hz: Svc.ActiveRateGroup base id 0xF00CC000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 4

  # Same core as the working deployment: TlmPacketizer, same queue and stack sizes.
  instance cmdDisp: Svc.CommandDispatcher base id 0xF1000000 \
    queue size CdhCoreConfig.QueueSizes.cmdDisp \
    stack size CdhCoreConfig.StackSizes.cmdDisp \
    priority CdhCoreConfig.Priorities.cmdDisp \
    cpu CdhCoreConfig.CpuAffinities.cmdDisp

  instance events: Svc.EventManager base id 0xF1010000 \
    queue size CdhCoreConfig.QueueSizes.events \
    stack size CdhCoreConfig.StackSizes.events \
    priority CdhCoreConfig.Priorities.events \
    cpu CdhCoreConfig.CpuAffinities.events

  instance tlmSend: Svc.TlmPacketizer base id 0xF1060000 \
    queue size CdhCoreConfig.QueueSizes.tlmSend \
    stack size CdhCoreConfig.StackSizes.tlmSend \
    priority CdhCoreConfig.Priorities.tlmSend \
    cpu CdhCoreConfig.CpuAffinities.tlmSend \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    NiclaDeployment::tlmSend.setPacketList(
        NiclaDeployment::NiclaDeployment_NiclaDeploymentPacketsTlmPackets::packetList,
        NiclaDeployment::NiclaDeployment_NiclaDeploymentPacketsTlmPackets::omittedChannels,
        1
    );
    """
  }

  # ----------------------------------------------------------------------
  # Queued component instances
  # ----------------------------------------------------------------------

  instance $health: Svc.Health base id 0xF1020000 \
    queue size CdhCoreConfig.QueueSizes.$health \
  {
    phase Fpp.ToCpp.Phases.configConstants """
    enum {
        HEALTH_WATCHDOG_CODE = 0x123
    };
    """
    phase Fpp.ToCpp.Phases.configComponents """
    NiclaDeployment::health.setPingEntries(
        ConfigObjects::NiclaDeployment_health::pingEntries,
        FW_NUM_ARRAY_ELEMENTS(ConfigObjects::NiclaDeployment_health::pingEntries),
        ConfigConstants::NiclaDeployment_health::HEALTH_WATCHDOG_CODE
    );
    """
  }

  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------

  instance version: Svc.Version base id 0xF1030000 \
  {
    phase Fpp.ToCpp.Phases.configComponents """
    NiclaDeployment::version.config(true);
    """
  }

  instance textLogger: Svc.PassiveTextLogger base id 0xF1040000

  instance fatalAdapter: Svc.AssertFatalAdapter base id 0xF1050000

  instance fatalHandler: Components.FatalHandler base id 0xF1070000


  instance chronoTime: Svc.ChronoTime base id 0xF00CD000

  instance rateGroupDriver: Svc.RateGroupDriver base id 0xF00CE000

  instance timer: Zephyr.ZephyrRateDriver base id 0xF00CF000

  instance comDriver: Zephyr.ZephyrUartDriver base id 0xF00D0000

  # Wathcdog Components
  instance gpioWatchdog: Zephyr.ZephyrGpioDriver base id 0xF00D1000
  instance watchdog: Components.Watchdog base id 0xF00D2000

}
