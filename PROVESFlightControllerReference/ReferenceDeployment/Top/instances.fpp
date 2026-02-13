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
    constant STACK_SIZE = 4 * 1024 # Must match prj.conf thread stack size
  }

  # ----------------------------------------------------------------------
  # Active component instances
  # ----------------------------------------------------------------------


  instance rateGroup50Hz: Svc.ActiveRateGroup base id 0x10001000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 1

  instance rateGroup10Hz: Svc.ActiveRateGroup base id 0x10002000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 2

  instance rateGroup1Hz: Svc.ActiveRateGroup base id 0x10003000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 3

  instance modeManager: Components.ModeManager base id 0x1000A000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 4

  instance cmdSeq: Svc.CmdSequencer base id 0x10006000 \
    queue size Default.QUEUE_SIZE * 2 \
    stack size Default.STACK_SIZE \
    priority 12

  instance payloadSeq: Svc.CmdSequencer base id 0x10062000 \
    queue size Default.QUEUE_SIZE * 2 \
    stack size Default.STACK_SIZE \
    priority 13

  instance safeModeSeq: Svc.CmdSequencer base id 0x10066000 \
    queue size Default.QUEUE_SIZE * 2 \
    stack size Default.STACK_SIZE \
    priority 13

  instance payload: Components.PayloadCom base id 0x10008000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 13


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

  instance bootloaderTrigger: Components.BootloaderTrigger base id 0x1001A000

  instance burnwire: Components.Burnwire base id 0x1001B000

  instance gpioBurnwire0: Zephyr.ZephyrGpioDriver base id 0x1001C000

  instance gpioBurnwire1: Zephyr.ZephyrGpioDriver base id 0x1001D000

  instance downlinkDelay: Components.ComDelay base id 0x1001E000

  instance lora: Zephyr.LoRa base id 0x1001F000

  instance comSplitterEvents: Svc.ComSplitter base id 0x10020000

  instance comSplitterTelemetry: Svc.ComSplitter base id 0x10021000

  instance antennaDeployer: Components.AntennaDeployer base id 0x10022000

  instance gpioface0LS: Zephyr.ZephyrGpioDriver base id 0x10023000

  instance gpioface1LS: Zephyr.ZephyrGpioDriver base id 0x10024000

  instance gpioface2LS: Zephyr.ZephyrGpioDriver base id 0x10025000

  instance gpioface3LS: Zephyr.ZephyrGpioDriver base id 0x10026000

  instance gpioface4LS: Zephyr.ZephyrGpioDriver base id 0x10027000

  instance gpioface5LS: Zephyr.ZephyrGpioDriver base id 0x10028000

  instance gpioPayloadPowerLS: Zephyr.ZephyrGpioDriver base id 0x10029000

  instance gpioPayloadBatteryLS: Zephyr.ZephyrGpioDriver base id 0x1002A000

  instance cameraHandler: Components.CameraHandler base id 0x1002B000

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

  instance amateurRadio: Components.AmateurRadio base id 0x10065000

  # Thermal Management System
  instance thermalManager: Components.ThermalManager base id 0x10041000
  instance tmp112Face0Manager: Drv.Tmp112Manager base id 0x10042000
  instance tmp112Face1Manager: Drv.Tmp112Manager base id 0x10043000
  instance tmp112Face2Manager: Drv.Tmp112Manager base id 0x10044000
  instance tmp112Face3Manager: Drv.Tmp112Manager base id 0x10045000
  instance tmp112Face4Manager: Drv.Tmp112Manager base id 0x10046000
  instance tmp112Face5Manager: Drv.Tmp112Manager base id 0x10047000
  instance tmp112BattCell1Manager: Drv.Tmp112Manager base id 0x10048000
  instance tmp112BattCell2Manager: Drv.Tmp112Manager base id 0x10049000
  instance tmp112BattCell3Manager: Drv.Tmp112Manager base id 0x1004A000
  instance tmp112BattCell4Manager: Drv.Tmp112Manager base id 0x1004B000

  # Attitude Determination and Control System (ADCS)
  instance adcs: Components.ADCS base id 0x1004C000
  instance veml6031Face0Manager: Drv.Veml6031Manager base id 0x1004D000
  instance veml6031Face1Manager: Drv.Veml6031Manager base id 0x1004E000
  instance veml6031Face2Manager: Drv.Veml6031Manager base id 0x1004F000
  instance veml6031Face3Manager: Drv.Veml6031Manager base id 0x10050000
  instance veml6031Face4Manager: Drv.Veml6031Manager base id 0x10051000
  instance veml6031Face5Manager: Drv.Veml6031Manager base id 0x10052000
  instance veml6031Face6Manager: Drv.Veml6031Manager base id 0x10053000
  instance veml6031Face7Manager: Drv.Veml6031Manager base id 0x10054000
  instance drv2605Face0Manager: Drv.Drv2605Manager base id 0x10055000
  instance drv2605Face1Manager: Drv.Drv2605Manager base id 0x10056000
  instance drv2605Face2Manager: Drv.Drv2605Manager base id 0x10057000
  instance drv2605Face3Manager: Drv.Drv2605Manager base id 0x10058000
  instance drv2605Face5Manager: Drv.Drv2605Manager base id 0x10059000

  instance detumbleManager: Components.DetumbleManager base id 0x1005A000
  instance fileUplinkCollector: Utilities.BufferCollector base id 0x10060000
  instance telemetryDelay: Utilities.RateDelay base id 0x10061000

  instance loraRetry: Svc.ComRetry base id 0x10063000

  instance downlinkRepeater: Utilities.BufferRepeater base id 0x10064000

  instance comDelaySband: Components.ComDelay base id 0x10070000

  instance spiDriver: Zephyr.ZephyrSpiDriver base id 0x10071000

  #instance sband : Components.SBand base id 0x10072000 \
  #  queue size Default.QUEUE_SIZE \
  #  stack size Default.STACK_SIZE \
  #  priority 10

  #instance gpioSbandNrst: Zephyr.ZephyrGpioDriver base id 0x10073000

  #instance gpioSbandRxEn: Zephyr.ZephyrGpioDriver base id 0x10074000

  #instance gpioSbandTxEn: Zephyr.ZephyrGpioDriver base id 0x10075000

  #instance gpioSbandIRQ: Zephyr.ZephyrGpioDriver base id 0x10076000

  instance dropDetector: Utilities.DropDetector base id 0x10077000

  instance fsFormat: Components.FsFormat base id 0x10078000

}
