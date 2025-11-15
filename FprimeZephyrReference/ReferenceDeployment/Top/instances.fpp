module ReferenceDeployment {


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
    constant STACK_SIZE = 8 * 1024 # Must match prj.conf thread stack size
  }

  # ----------------------------------------------------------------------
  # Active component instances
  # ----------------------------------------------------------------------

  instance rateGroup10Hz: Svc.ActiveRateGroup base id 0x10001000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 2

  instance rateGroup1Hz: Svc.ActiveRateGroup base id 0x10002000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 3

  instance cmdSeq: Svc.CmdSequencer base id 0x10003000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 15

  instance prmDb: Svc.PrmDb base id 0x10004000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 14

  # ----------------------------------------------------------------------
  # Queued component instances
  # ----------------------------------------------------------------------


  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------
  instance rateGroupDriver: Svc.RateGroupDriver base id 0x10010000

  instance version: Svc.Version base id 0x10011000

  instance timer: Zephyr.ZephyrRateDriver base id 0x10012000

  instance comDriver: Zephyr.ZephyrUartDriver base id 0x10013000

  instance gpioWatchdog: Zephyr.ZephyrGpioDriver base id 0x10014000

  instance watchdog: Components.Watchdog base id 0x10015000

  instance rtcManager: Drv.RtcManager base id 0x10016000

  instance imuManager: Components.ImuManager base id 0x10017000

  instance lis2mdlManager: Drv.Lis2mdlManager base id 0x10018000

  instance lsm6dsoManager: Drv.Lsm6dsoManager base id 0x10019000

  instance bootloaderTrigger: Components.BootloaderTrigger base id 0x1001A000

  instance burnwire: Components.Burnwire base id 0x1001B000

  instance gpioBurnwire0: Zephyr.ZephyrGpioDriver base id 0x1001C000

  instance gpioBurnwire1: Zephyr.ZephyrGpioDriver base id 0x1001D000

  instance comDelay: Components.ComDelay base id 0x1001E000

  instance lora: Zephyr.LoRa base id 0x1001F000

  instance comSplitterEvents: Svc.ComSplitter base id 0x10020000

  instance comSplitterTelemetry: Svc.ComSplitter base id 0x10021000

  instance antennaDeployer: Components.AntennaDeployer base id 0x10022000

  instance gpioface4LS: Zephyr.ZephyrGpioDriver base id 0x10023000

  instance gpioface0LS: Zephyr.ZephyrGpioDriver base id 0x10024000

  instance gpioface1LS: Zephyr.ZephyrGpioDriver base id 0x10025000

  instance gpioface2LS: Zephyr.ZephyrGpioDriver base id 0x10026000

  instance gpioface3LS: Zephyr.ZephyrGpioDriver base id 0x10027000

  instance gpioface5LS: Zephyr.ZephyrGpioDriver base id 0x10028000

  instance gpioPayloadPowerLS: Zephyr.ZephyrGpioDriver base id 0x10029000

  instance gpioPayloadBatteryLS: Zephyr.ZephyrGpioDriver base id 0x1002A000

  instance payload: Components.PayloadHandler base id 0x1002B000

  instance peripheralUartDriver: Zephyr.ZephyrUartDriver base id 0x1002C000

  instance payloadBufferManager: Svc.BufferManager base id 0x1002D000 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    Svc::BufferManager::BufferBins bins;
    """
    phase Fpp.ToCpp.Phases.configComponents """
    memset(&ConfigObjects::ReferenceDeployment_payloadBufferManager::bins, 0, sizeof(ConfigObjects::ReferenceDeployment_payloadBufferManager::bins));
    // UART RX buffers for camera data streaming (4 KB, 2 buffers for ping-pong)
    ConfigObjects::ReferenceDeployment_payloadBufferManager::bins.bins[0].bufferSize = 4 * 1024;
    ConfigObjects::ReferenceDeployment_payloadBufferManager::bins.bins[0].numBuffers = 2;
    ReferenceDeployment::payloadBufferManager.setup(
        1,  // manager ID
        0,  // store ID
        ComCcsds::Allocation::memAllocator,  // Reuse existing allocator from ComCcsds subtopology
        ConfigObjects::ReferenceDeployment_payloadBufferManager::bins
    );
    """
    phase Fpp.ToCpp.Phases.tearDownComponents """
    ReferenceDeployment::payloadBufferManager.cleanup();
    """
  }
  instance fsSpace: Components.FsSpace base id 0x1002E000

  instance face4LoadSwitch: Components.LoadSwitch base id 0x1002F000

  instance face0LoadSwitch: Components.LoadSwitch base id 0x10030000

  instance face1LoadSwitch: Components.LoadSwitch base id 0x10031000

  instance face2LoadSwitch: Components.LoadSwitch base id 0x10032000

  instance face3LoadSwitch: Components.LoadSwitch base id 0x10033000

  instance face5LoadSwitch: Components.LoadSwitch base id 0x10034000

  instance payloadPowerLoadSwitch: Components.LoadSwitch base id 0x10035000

  instance payloadBatteryLoadSwitch: Components.LoadSwitch base id 0x10036000

  instance resetManager: Components.ResetManager base id 0x10037000

  instance powerMonitor: Components.PowerMonitor base id 0x10038000

  instance ina219SysManager: Drv.Ina219Manager base id 0x10039000

  instance ina219SolManager: Drv.Ina219Manager base id 0x1003A000

  instance startupManager: Components.StartupManager base id 0x1003B000

}
