module ReferenceDeployment {

  # ----------------------------------------------------------------------
  # Symbolic constants for port numbers
  # ----------------------------------------------------------------------

  enum Ports_RateGroups {
    rateGroup10Hz
    rateGroup1Hz
  }

  topology ReferenceDeployment {

  # ----------------------------------------------------------------------
  # Subtopology imports
  # ----------------------------------------------------------------------
    import CdhCore.Subtopology
    import ComCcsdsLora.Subtopology
    import ComCcsdsUart.Subtopology
    import FileHandling.Subtopology
    import ComCcsdsSband.Subtopology
    import Update.Subtopology

  # ----------------------------------------------------------------------
  # Instances used in the topology
  # ----------------------------------------------------------------------
    instance rateGroup10Hz
    instance rateGroup1Hz
    instance rateGroupDriver
    instance timer
    instance lora
    instance loraRetry
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
    instance comDelaySband
    instance downlinkDelay
    instance telemetryDelay
    instance burnwire
    instance antennaDeployer
    instance comSplitterEvents
    instance comSplitterTelemetry
    instance amateurRadio
    # For UART sideband communication
    instance comDriver
    instance spiDriver
    instance sband
    instance gpioSbandNrst
    instance gpioSbandRxEn
    instance gpioSbandTxEn
    instance gpioSbandIRQ
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
    instance payloadSeq
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
    instance downlinkRepeater

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
      comSplitterEvents.comOut-> ComCcsdsLora.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.EVENTS]
      comSplitterEvents.comOut-> ComCcsdsUart.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.EVENTS]

      CdhCore.tlmSend.PktSend -> comSplitterTelemetry.comIn
      comSplitterTelemetry.comOut -> ComCcsdsLora.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.TELEMETRY]
      comSplitterTelemetry.comOut -> ComCcsdsUart.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.TELEMETRY]

      # Router to Command Dispatcher
      ComCcsdsLora.authenticationRouter.commandOut -> CdhCore.cmdDisp.seqCmdBuff
      CdhCore.cmdDisp.seqCmdStatus -> ComCcsdsLora.authenticationRouter.cmdResponseIn

      ComCcsdsSband.authenticationRouter.commandOut -> CdhCore.cmdDisp.seqCmdBuff
      CdhCore.cmdDisp.seqCmdStatus -> ComCcsdsSband.authenticationRouter.cmdResponseIn

      ComCcsdsUart.authenticationRouter.commandOut -> CdhCore.cmdDisp.seqCmdBuff
      CdhCore.cmdDisp.seqCmdStatus -> ComCcsdsUart.authenticationRouter.cmdResponseIn

      cmdSeq.comCmdOut -> CdhCore.cmdDisp.seqCmdBuff
      CdhCore.cmdDisp.seqCmdStatus -> cmdSeq.cmdResponseIn


      # Sband connections
      comSplitterEvents.comOut-> ComCcsdsSband.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.EVENTS]
      comSplitterTelemetry.comOut -> ComCcsdsSband.comQueue.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.TELEMETRY]

      payloadSeq.comCmdOut -> CdhCore.cmdDisp.seqCmdBuff
      CdhCore.cmdDisp.seqCmdStatus -> payloadSeq.cmdResponseIn

      telemetryDelay.runOut -> CdhCore.tlmSend.Run

    }


      connections CommunicationsSBandRadio {
        sband.allocate      -> ComCcsdsSband.commsBufferManager.bufferGetCallee
        sband.deallocate    -> ComCcsdsSband.commsBufferManager.bufferSendIn

        # ComDriver <-> ComStub (Uplink)
        sband.dataOut -> ComCcsdsSband.frameAccumulator.dataIn
        ComCcsdsSband.frameAccumulator.dataReturnOut -> sband.dataReturnIn

        # ComStub <-> ComDriver (Downlink)
        ComCcsdsSband.framer.dataOut -> sband.dataIn
        sband.dataReturnOut -> ComCcsdsSband.framer.dataReturnIn
        sband.comStatusOut -> comDelaySband.comStatusIn
        comDelaySband.comStatusOut -> ComCcsdsSband.framer.comStatusIn


    }

    connections CommunicationsRadio {
      lora.allocate      -> ComCcsdsLora.commsBufferManager.bufferGetCallee
      lora.deallocate    -> ComCcsdsLora.commsBufferManager.bufferSendIn

      # ComDriver <-> FrameAccumulator (Uplink)
      lora.dataOut -> ComCcsdsLora.frameAccumulator.dataIn
      ComCcsdsLora.frameAccumulator.dataReturnOut -> lora.dataReturnIn

      # ComStub <-> ComDriver (Downlink)
      ComCcsdsLora.framer.dataOut -> loraRetry.dataIn
      loraRetry.dataOut -> lora.dataIn

      lora.dataReturnOut -> loraRetry.dataReturnIn
      loraRetry.dataReturnOut -> ComCcsdsLora.framer.dataReturnIn

      lora.comStatusOut -> loraRetry.comStatusIn
      loraRetry.comStatusOut -> downlinkDelay.comStatusIn
      downlinkDelay.comStatusOut ->ComCcsdsLora.framer.comStatusIn


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
      rateGroup10Hz.RateGroupMemberOut[2] -> ComCcsdsLora.aggregator.timeout
      rateGroup10Hz.RateGroupMemberOut[3] -> ComCcsdsSband.aggregator.timeout
      rateGroup10Hz.RateGroupMemberOut[4] -> peripheralUartDriver.schedIn
      rateGroup10Hz.RateGroupMemberOut[5] -> peripheralUartDriver2.schedIn
      rateGroup10Hz.RateGroupMemberOut[6] -> FileHandling.fileManager.schedIn
      rateGroup10Hz.RateGroupMemberOut[7] -> cmdSeq.schedIn
      rateGroup10Hz.RateGroupMemberOut[8] -> payloadSeq.schedIn
      rateGroup10Hz.RateGroupMemberOut[9] -> drv2605Face0Manager.run
      rateGroup10Hz.RateGroupMemberOut[10] -> drv2605Face1Manager.run
      rateGroup10Hz.RateGroupMemberOut[11] -> drv2605Face2Manager.run
      rateGroup10Hz.RateGroupMemberOut[12] -> drv2605Face3Manager.run
      rateGroup10Hz.RateGroupMemberOut[13] -> drv2605Face5Manager.run
      rateGroup10Hz.RateGroupMemberOut[14] -> downlinkDelay.run
      rateGroup10Hz.RateGroupMemberOut[15] -> sband.run

      # Slow rate (1Hz) rate group
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup1Hz] -> rateGroup1Hz.CycleIn
      rateGroup1Hz.RateGroupMemberOut[0] -> ComCcsdsLora.comQueue.run
      rateGroup1Hz.RateGroupMemberOut[1] -> ComCcsdsSband.comQueue.run
      rateGroup1Hz.RateGroupMemberOut[2] -> CdhCore.$health.Run
      rateGroup1Hz.RateGroupMemberOut[3] -> ComCcsdsLora.commsBufferManager.schedIn
      rateGroup1Hz.RateGroupMemberOut[4] -> ComCcsdsSband.commsBufferManager.schedIn
      rateGroup1Hz.RateGroupMemberOut[5] -> watchdog.run
      rateGroup1Hz.RateGroupMemberOut[6] -> imuManager.run
      rateGroup1Hz.RateGroupMemberOut[7] -> telemetryDelay.runIn
      rateGroup1Hz.RateGroupMemberOut[8] -> burnwire.schedIn
      rateGroup1Hz.RateGroupMemberOut[9] -> antennaDeployer.schedIn
      rateGroup1Hz.RateGroupMemberOut[10] -> fsSpace.run
      rateGroup1Hz.RateGroupMemberOut[11] -> payloadBufferManager.schedIn
      rateGroup1Hz.RateGroupMemberOut[12] -> payloadBufferManager2.schedIn
      rateGroup1Hz.RateGroupMemberOut[13] -> FileHandling.fileDownlink.Run
      rateGroup1Hz.RateGroupMemberOut[14] -> startupManager.run
      rateGroup1Hz.RateGroupMemberOut[15] -> powerMonitor.run
      rateGroup1Hz.RateGroupMemberOut[16] -> modeManager.run
      rateGroup1Hz.RateGroupMemberOut[17] -> imuManager.run
      rateGroup1Hz.RateGroupMemberOut[18] -> adcs.run
      rateGroup1Hz.RateGroupMemberOut[19] -> thermalManager.run
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

    connections MyConnectionGraph {
      sband.spiSend -> spiDriver.SpiReadWrite
      sband.resetSend -> gpioSbandNrst.gpioWrite
      sband.txEnable -> gpioSbandTxEn.gpioWrite
      sband.rxEnable -> gpioSbandRxEn.gpioWrite
      sband.getIRQLine -> gpioSbandIRQ.gpioRead
    }

    connections ComCcsds_FileHandling {
      # File Downlink <-> ComQueue
      FileHandling.fileDownlink.bufferSendOut -> downlinkRepeater.singleIn
      downlinkRepeater.singleOut -> FileHandling.fileDownlink.bufferReturn

      downlinkRepeater.multiOut[0] -> ComCcsdsUart.comQueue.bufferQueueIn[ComCcsds.Ports_ComBufferQueue.FILE]
      downlinkRepeater.multiOut[1] -> ComCcsdsLora.comQueue.bufferQueueIn[ComCcsds.Ports_ComBufferQueue.FILE]
      downlinkRepeater.multiOut[2] -> ComCcsdsSband.comQueue.bufferQueueIn[ComCcsds.Ports_ComBufferQueue.FILE]

      ComCcsdsUart.comQueue.bufferReturnOut[ComCcsds.Ports_ComBufferQueue.FILE] -> downlinkRepeater.multiIn[0]
      ComCcsdsLora.comQueue.bufferReturnOut[ComCcsds.Ports_ComBufferQueue.FILE] -> downlinkRepeater.multiIn[1]
      ComCcsdsSband.comQueue.bufferReturnOut[ComCcsds.Ports_ComBufferQueue.FILE] -> downlinkRepeater.multiIn[2]

    }

    connections FileUplinkCollecting {
      # Router <-> FileUplink
      fileUplinkCollector.singleOut -> FileHandling.fileUplink.bufferSendIn
      FileHandling.fileUplink.bufferSendOut -> fileUplinkCollector.singleIn

      ComCcsdsSband.authenticationRouter.fileOut     -> fileUplinkCollector.multiIn[2]
      fileUplinkCollector.multiOut[2] -> ComCcsdsSband.authenticationRouter.fileBufferReturnIn
      ComCcsdsUart.authenticationRouter.fileOut     -> fileUplinkCollector.multiIn[1]
      fileUplinkCollector.multiOut[1] -> ComCcsdsUart.authenticationRouter.fileBufferReturnIn
      ComCcsdsLora.authenticationRouter.fileOut     -> fileUplinkCollector.multiIn[0]
      fileUplinkCollector.multiOut[0] -> ComCcsdsLora.authenticationRouter.fileBufferReturnIn
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
