module ReferenceDeployment {

  # ----------------------------------------------------------------------
  # Symbolic constants for port numbers
  # ----------------------------------------------------------------------

  enum Ports_RateGroups {
    rateGroup10Hz
    rateGroup1Hz
    rateGroup1_6Hz
    rateGroup1_10Hz
  }

  topology ReferenceDeployment {

  # ----------------------------------------------------------------------
  # Subtopology imports
  # ----------------------------------------------------------------------
    import CdhCore.Subtopology
    import ComCcsds.FramingSubtopology
    import ComCcsdsUart.Subtopology
    import FileHandling.Subtopology
    import Update.Subtopology

  # ----------------------------------------------------------------------
  # Instances used in the topology
  # ----------------------------------------------------------------------
    instance rateGroup10Hz
    instance rateGroup1Hz
    instance rateGroup1_6Hz
    instance rateGroup1_10Hz
    instance rateGroupDriver
    instance timer
    instance lora
    instance gpioWatchdog
    instance gpioBurnwire0
    instance gpioBurnwire1
    instance gpioface0LS
    instance gpioface1LS
    instance gpioface2LS
    instance gpioface3LS
    instance gpioface4LS
    instance gpioface5LS
    instance gpioPayloadPowerLS
    instance gpioPayloadBatteryLS
    instance watchdog
    instance rtcManager
    instance imuManager
    instance bootloaderTrigger
    instance comDelay
    instance burnwire
    instance antennaDeployer
    instance comSplitterEvents
    instance comSplitterTelemetry
    # For UART sideband communication
    instance comDriver

    instance face4LoadSwitch
    instance face0LoadSwitch
    instance face1LoadSwitch
    instance face2LoadSwitch
    instance face3LoadSwitch
    instance face5LoadSwitch
    instance payloadPowerLoadSwitch
    instance payloadBatteryLoadSwitch
    instance fsSpace
    instance payload
    instance payload2
    instance cameraHandler
    instance peripheralUartDriver
    instance cameraHandler2
    instance peripheralUartDriver2
    instance payloadBufferManager
    instance payloadBufferManager2
    instance cmdSeq
    instance startupManager
    instance powerMonitor
    instance ina219SysManager
    instance ina219SolManager
    instance resetManager
    instance fileUplinkCollector
    instance modeManager
    instance adcs

    # Face Board Instances
    instance thermalManager
    instance tmp112Face0Manager
    instance tmp112Face1Manager
    instance tmp112Face2Manager
    instance tmp112Face3Manager
    instance tmp112Face5Manager
    instance tmp112BattCell1Manager
    instance tmp112BattCell2Manager
    instance tmp112BattCell3Manager
    instance tmp112BattCell4Manager
    instance veml6031Face0Manager
    instance veml6031Face1Manager
    instance veml6031Face2Manager
    instance veml6031Face3Manager
    instance veml6031Face5Manager
    instance veml6031Face6Manager
    instance veml6031Face7Manager
    instance drv2605Face0Manager
    instance drv2605Face1Manager
    instance drv2605Face2Manager
    instance drv2605Face3Manager
    instance drv2605Face5Manager

  # ----------------------------------------------------------------------
  # Pattern graph specifiers
  # ----------------------------------------------------------------------

    command connections instance CdhCore.cmdDisp
    event connections instance CdhCore.events
    text event connections instance CdhCore.textLogger
    health connections instance CdhCore.$health
    time connections instance rtcManager
    telemetry connections instance CdhCore.tlmSend
    param connections instance FileHandling.prmDb

  # ----------------------------------------------------------------------
  # Telemetry packets (only used when TlmPacketizer is used)
  # ----------------------------------------------------------------------

  include "ReferenceDeploymentPackets.fppi"

  # ----------------------------------------------------------------------
  # Direct graph specifiers
  # ----------------------------------------------------------------------

    connections ComCcsds_CdhCore {
      # Core events and telemetry to communication queue
      CdhCore.events.PktSend -> comSplitterEvents.comIn
      comSplitterEvents.comOut-> ComCcsds.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.EVENTS]
      comSplitterEvents.comOut-> ComCcsdsUart.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.EVENTS]

      CdhCore.tlmSend.PktSend -> comSplitterTelemetry.comIn
      comSplitterTelemetry.comOut -> ComCcsds.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.TELEMETRY]
      comSplitterTelemetry.comOut -> ComCcsdsUart.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.TELEMETRY]

      # Router to Command Dispatcher
      ComCcsds.fprimeRouter.commandOut -> CdhCore.cmdDisp.seqCmdBuff
      CdhCore.cmdDisp.seqCmdStatus -> ComCcsds.fprimeRouter.cmdResponseIn

      ComCcsdsUart.fprimeRouter.commandOut -> CdhCore.cmdDisp.seqCmdBuff
      CdhCore.cmdDisp.seqCmdStatus -> ComCcsdsUart.fprimeRouter.cmdResponseIn

      cmdSeq.comCmdOut -> CdhCore.cmdDisp.seqCmdBuff
      CdhCore.cmdDisp.seqCmdStatus -> cmdSeq.cmdResponseIn
    }

    connections CommunicationsRadio {
      lora.allocate      -> ComCcsds.commsBufferManager.bufferGetCallee
      lora.deallocate    -> ComCcsds.commsBufferManager.bufferSendIn

      # ComDriver <-> ComStub (Uplink)
      lora.dataOut -> ComCcsds.frameAccumulator.dataIn
      ComCcsds.frameAccumulator.dataReturnOut -> lora.dataReturnIn

      # ComStub <-> ComDriver (Downlink)
      ComCcsds.framer.dataOut -> lora.dataIn
      lora.dataReturnOut -> ComCcsds.framer.dataReturnIn
      lora.comStatusOut -> comDelay.comStatusIn
      comDelay.comStatusOut ->ComCcsds.framer.comStatusIn


      startupManager.runSequence -> cmdSeq.seqRunIn
      cmdSeq.seqDone -> startupManager.completeSequence
    }

    connections CommunicationsUart {
      # ComDriver buffer allocations
      comDriver.allocate      -> ComCcsdsUart.commsBufferManager.bufferGetCallee
      comDriver.deallocate    -> ComCcsdsUart.commsBufferManager.bufferSendIn

      # ComDriver <-> ComStub (Uplink)
      comDriver.$recv                     -> ComCcsdsUart.comStub.drvReceiveIn
      ComCcsdsUart.comStub.drvReceiveReturnOut -> comDriver.recvReturnIn

      # ComStub <-> ComDriver (Downlink)
      ComCcsdsUart.comStub.drvSendOut      -> comDriver.$send
      comDriver.ready         -> ComCcsdsUart.comStub.drvConnected
    }

    connections RateGroups {
      # timer to drive rate group
      timer.CycleOut -> rateGroupDriver.CycleIn

      # High rate (10Hz) rate group
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup10Hz] -> rateGroup10Hz.CycleIn
      rateGroup10Hz.RateGroupMemberOut[0] -> comDriver.schedIn
      rateGroup10Hz.RateGroupMemberOut[1] -> ComCcsdsUart.aggregator.timeout
      rateGroup10Hz.RateGroupMemberOut[2] -> ComCcsds.aggregator.timeout
      rateGroup10Hz.RateGroupMemberOut[3] -> peripheralUartDriver.schedIn
      rateGroup10Hz.RateGroupMemberOut[4] -> peripheralUartDriver2.schedIn
      rateGroup10Hz.RateGroupMemberOut[5] -> FileHandling.fileManager.schedIn
      rateGroup10Hz.RateGroupMemberOut[6] -> cmdSeq.schedIn
      rateGroup10Hz.RateGroupMemberOut[7] -> drv2605Face0Manager.run
      rateGroup10Hz.RateGroupMemberOut[8] -> drv2605Face1Manager.run
      rateGroup10Hz.RateGroupMemberOut[9] -> drv2605Face2Manager.run
      rateGroup10Hz.RateGroupMemberOut[10] -> drv2605Face3Manager.run
      rateGroup10Hz.RateGroupMemberOut[11] -> drv2605Face5Manager.run

      # Slow rate (1Hz) rate group
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup1Hz] -> rateGroup1Hz.CycleIn
      rateGroup1Hz.RateGroupMemberOut[0] -> ComCcsds.comQueue.run
      rateGroup1Hz.RateGroupMemberOut[1] -> CdhCore.$health.Run
      rateGroup1Hz.RateGroupMemberOut[2] -> ComCcsds.commsBufferManager.schedIn
      rateGroup1Hz.RateGroupMemberOut[3] -> CdhCore.tlmSend.Run
      rateGroup1Hz.RateGroupMemberOut[4] -> watchdog.run
      rateGroup1Hz.RateGroupMemberOut[5] -> imuManager.run
      rateGroup1Hz.RateGroupMemberOut[6] -> comDelay.run
      rateGroup1Hz.RateGroupMemberOut[7] -> burnwire.schedIn
      rateGroup1Hz.RateGroupMemberOut[8] -> antennaDeployer.schedIn
      rateGroup1Hz.RateGroupMemberOut[9] -> fsSpace.run
      rateGroup1Hz.RateGroupMemberOut[10] -> payloadBufferManager.schedIn
      rateGroup1Hz.RateGroupMemberOut[11] -> payloadBufferManager2.schedIn
      rateGroup1Hz.RateGroupMemberOut[12] -> FileHandling.fileDownlink.Run
      rateGroup1Hz.RateGroupMemberOut[13] -> startupManager.run
      rateGroup1Hz.RateGroupMemberOut[14] -> powerMonitor.run
      rateGroup1Hz.RateGroupMemberOut[15] -> modeManager.run

      # Slower rate (1/6Hz) rate group
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup1_6Hz] -> rateGroup1_6Hz.CycleIn
      rateGroup1_6Hz.RateGroupMemberOut[0] -> imuManager.run
      rateGroup1_6Hz.RateGroupMemberOut[1] -> adcs.run
      rateGroup1_6Hz.RateGroupMemberOut[2] -> thermalManager.run

      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup1_10Hz] -> rateGroup1_10Hz.CycleIn
    }


    connections Watchdog {
      watchdog.gpioSet -> gpioWatchdog.gpioWrite
    }

    connections LoadSwitches {
      face4LoadSwitch.gpioSet -> gpioface4LS.gpioWrite
      face4LoadSwitch.gpioGet -> gpioface4LS.gpioRead

      face0LoadSwitch.gpioSet -> gpioface0LS.gpioWrite
      face0LoadSwitch.gpioGet -> gpioface0LS.gpioRead
      face0LoadSwitch.loadSwitchStateChanged[0] -> tmp112Face0Manager.loadSwitchStateChanged
      face0LoadSwitch.loadSwitchStateChanged[1] -> veml6031Face0Manager.loadSwitchStateChanged
      face0LoadSwitch.loadSwitchStateChanged[2] -> drv2605Face0Manager.loadSwitchStateChanged

      face1LoadSwitch.gpioSet -> gpioface1LS.gpioWrite
      face1LoadSwitch.gpioGet -> gpioface1LS.gpioRead
      face1LoadSwitch.loadSwitchStateChanged[0] -> tmp112Face1Manager.loadSwitchStateChanged
      face1LoadSwitch.loadSwitchStateChanged[1] -> veml6031Face1Manager.loadSwitchStateChanged
      face1LoadSwitch.loadSwitchStateChanged[2] -> drv2605Face1Manager.loadSwitchStateChanged

      face2LoadSwitch.gpioSet -> gpioface2LS.gpioWrite
      face2LoadSwitch.gpioGet -> gpioface2LS.gpioRead
      face2LoadSwitch.loadSwitchStateChanged[0] -> tmp112Face2Manager.loadSwitchStateChanged
      face2LoadSwitch.loadSwitchStateChanged[1] -> veml6031Face2Manager.loadSwitchStateChanged
      face2LoadSwitch.loadSwitchStateChanged[2] -> drv2605Face2Manager.loadSwitchStateChanged

      face3LoadSwitch.gpioSet -> gpioface3LS.gpioWrite
      face3LoadSwitch.gpioGet -> gpioface3LS.gpioRead
      face3LoadSwitch.loadSwitchStateChanged[0] -> tmp112Face3Manager.loadSwitchStateChanged
      face3LoadSwitch.loadSwitchStateChanged[1] -> veml6031Face3Manager.loadSwitchStateChanged
      face3LoadSwitch.loadSwitchStateChanged[2] -> drv2605Face3Manager.loadSwitchStateChanged

      face5LoadSwitch.gpioSet -> gpioface5LS.gpioWrite
      face5LoadSwitch.gpioGet -> gpioface5LS.gpioRead
      face5LoadSwitch.loadSwitchStateChanged[0] -> tmp112Face5Manager.loadSwitchStateChanged
      face5LoadSwitch.loadSwitchStateChanged[1] -> veml6031Face5Manager.loadSwitchStateChanged
      face5LoadSwitch.loadSwitchStateChanged[2] -> drv2605Face5Manager.loadSwitchStateChanged

      payloadPowerLoadSwitch.gpioSet -> gpioPayloadPowerLS.gpioWrite
      payloadPowerLoadSwitch.gpioGet -> gpioPayloadPowerLS.gpioRead

      payloadBatteryLoadSwitch.gpioSet -> gpioPayloadBatteryLS.gpioWrite
      payloadBatteryLoadSwitch.gpioGet -> gpioPayloadBatteryLS.gpioRead
    }

    connections BurnwireGpio {
      burnwire.gpioSet[0] -> gpioBurnwire0.gpioWrite
      burnwire.gpioSet[1] -> gpioBurnwire1.gpioWrite
    }

    connections AntennaDeployment {
      antennaDeployer.burnStart -> burnwire.burnStart
      antennaDeployer.burnStop -> burnwire.burnStop
    }

    connections PayloadCom {
      # PayloadCom <-> UART Driver
      payload.uartForward -> peripheralUartDriver.$send
      peripheralUartDriver.$recv -> payload.uartDataIn

      # Buffer return path (critical! - matches ComStub pattern)
      payload.bufferReturn -> peripheralUartDriver.recvReturnIn

      # PayloadCom <-> CameraHandler data flow
      payload.uartDataOut -> cameraHandler.dataIn
      cameraHandler.commandOut -> payload.commandIn

      # UART driver allocates/deallocates from BufferManager
      peripheralUartDriver.allocate -> payloadBufferManager.bufferGetCallee
      peripheralUartDriver.deallocate -> payloadBufferManager.bufferSendIn
    }

     connections PayloadCom2 {
      # PayloadCom <-> UART Driver
      payload2.uartForward -> peripheralUartDriver2.$send
      peripheralUartDriver2.$recv -> payload2.uartDataIn

      # Buffer return path (critical! - matches ComStub pattern)
      payload2.bufferReturn -> peripheralUartDriver2.recvReturnIn

      # PayloadCom <-> CameraHandler data flow
      payload2.uartDataOut -> cameraHandler2.dataIn
      cameraHandler2.commandOut -> payload2.commandIn

      # UART driver allocates/deallocates from BufferManager
      peripheralUartDriver2.allocate -> payloadBufferManager.bufferGetCallee
      peripheralUartDriver2.deallocate -> payloadBufferManager.bufferSendIn
    }

    connections ComCcsds_FileHandling {
      # File Downlink <-> ComQueue
      FileHandling.fileDownlink.bufferSendOut -> ComCcsdsUart.comQueue.bufferQueueIn[ComCcsds.Ports_ComBufferQueue.FILE]
      ComCcsdsUart.comQueue.bufferReturnOut[ComCcsds.Ports_ComBufferQueue.FILE] -> FileHandling.fileDownlink.bufferReturn
    }

    connections FileUplinkCollecting {
      # Router <-> FileUplink
      fileUplinkCollector.singleOut -> FileHandling.fileUplink.bufferSendIn
      FileHandling.fileUplink.bufferSendOut -> fileUplinkCollector.singleIn

      ComCcsdsUart.fprimeRouter.fileOut     -> fileUplinkCollector.multiIn[1]
      fileUplinkCollector.multiOut[1] -> ComCcsdsUart.fprimeRouter.fileBufferReturnIn
      ComCcsds.fprimeRouter.fileOut     -> fileUplinkCollector.multiIn[0]
      fileUplinkCollector.multiOut[0] -> ComCcsds.fprimeRouter.fileBufferReturnIn
    }


    connections sysPowerMonitor {
      powerMonitor.sysVoltageGet -> ina219SysManager.voltageGet
      powerMonitor.sysCurrentGet -> ina219SysManager.currentGet
      powerMonitor.sysPowerGet -> ina219SysManager.powerGet
      powerMonitor.solVoltageGet -> ina219SolManager.voltageGet
      powerMonitor.solCurrentGet -> ina219SolManager.currentGet
      powerMonitor.solPowerGet -> ina219SolManager.powerGet
    }

    connections thermalManager {
      thermalManager.faceTempGet[0] -> tmp112Face0Manager.temperatureGet
      thermalManager.faceTempGet[1] -> tmp112Face1Manager.temperatureGet
      thermalManager.faceTempGet[2] -> tmp112Face2Manager.temperatureGet
      thermalManager.faceTempGet[3] -> tmp112Face3Manager.temperatureGet
      thermalManager.faceTempGet[4] -> tmp112Face5Manager.temperatureGet
      thermalManager.battCellTempGet[0] -> tmp112BattCell1Manager.temperatureGet
      thermalManager.battCellTempGet[1] -> tmp112BattCell2Manager.temperatureGet
      thermalManager.battCellTempGet[2] -> tmp112BattCell3Manager.temperatureGet
      thermalManager.battCellTempGet[3] -> tmp112BattCell4Manager.temperatureGet
    }

    connections adcs {
      adcs.visibleLightGet[0] -> veml6031Face0Manager.visibleLightGet
      adcs.visibleLightGet[1] -> veml6031Face1Manager.visibleLightGet
      adcs.visibleLightGet[2] -> veml6031Face2Manager.visibleLightGet
      adcs.visibleLightGet[3] -> veml6031Face3Manager.visibleLightGet
      adcs.visibleLightGet[4] -> veml6031Face5Manager.visibleLightGet
      adcs.visibleLightGet[5] -> veml6031Face6Manager.visibleLightGet
    }

    connections ModeManager {
      # Voltage monitoring from system power manager
      modeManager.voltageGet -> ina219SysManager.voltageGet

      # Connection for clean shutdown notification from ResetManager
      # Allows ModeManager to detect unintended reboots
      resetManager.prepareForReboot -> modeManager.prepareForReboot

      # Load switch control connections
      # The load switch index mapping below is non-sequential because it matches the physical board layout and wiring order.
      # This ordering ensures that software indices correspond to the hardware arrangement for easier maintenance and debugging.
      # face4 = index 0, face0 = index 1, face1 = index 2, face2 = index 3
      # face3 = index 4, face5 = index 5, payloadPower = index 6, payloadBattery = index 7
      # TODO(ALLTEAMS): Configure the faces you want to automatically turn on
      # modeManager.loadSwitchTurnOn[0] -> face4LoadSwitch.turnOn
      # modeManager.loadSwitchTurnOn[1] -> face0LoadSwitch.turnOn
      # modeManager.loadSwitchTurnOn[2] -> face1LoadSwitch.turnOn
      # modeManager.loadSwitchTurnOn[3] -> face2LoadSwitch.turnOn
      # modeManager.loadSwitchTurnOn[4] -> face3LoadSwitch.turnOn
      # modeManager.loadSwitchTurnOn[5] -> face5LoadSwitch.turnOn
      # modeManager.loadSwitchTurnOn[6] -> payloadPowerLoadSwitch.turnOn
      # modeManager.loadSwitchTurnOn[7] -> payloadBatteryLoadSwitch.turnOn

      modeManager.loadSwitchTurnOff[0] -> face4LoadSwitch.turnOff
      modeManager.loadSwitchTurnOff[1] -> face0LoadSwitch.turnOff
      modeManager.loadSwitchTurnOff[2] -> face1LoadSwitch.turnOff
      modeManager.loadSwitchTurnOff[3] -> face2LoadSwitch.turnOff
      modeManager.loadSwitchTurnOff[4] -> face3LoadSwitch.turnOff
      modeManager.loadSwitchTurnOff[5] -> face5LoadSwitch.turnOff
      modeManager.loadSwitchTurnOff[6] -> payloadPowerLoadSwitch.turnOff
      modeManager.loadSwitchTurnOff[7] -> payloadBatteryLoadSwitch.turnOff
    }

  }
}
