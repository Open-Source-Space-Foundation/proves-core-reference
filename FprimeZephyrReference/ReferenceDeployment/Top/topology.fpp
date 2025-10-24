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
    import ComCcsds.FramingSubtopology
    import ComCcsdsUart.Subtopology

  # ----------------------------------------------------------------------
  # Instances used in the topology
  # ----------------------------------------------------------------------
    instance rateGroup10Hz
    instance rateGroup1Hz
    instance rateGroupDriver
    instance timer
    instance lora
    instance gpioDriver
    instance gpioBurnwire0
    instance gpioBurnwire1
    instance watchdog
    instance prmDb
    instance rtcManager
    instance imuManager
    instance lis2mdlManager
    instance lsm6dsoManager
    instance bootloaderTrigger
    instance comDelay
    instance burnwire
    instance comSplitterEvents
    instance comSplitterTelemetry
    # For UART sideband communication
    instance comDriver

  # ----------------------------------------------------------------------
  # Pattern graph specifiers
  # ----------------------------------------------------------------------

    command connections instance CdhCore.cmdDisp
    event connections instance CdhCore.events
    text event connections instance CdhCore.textLogger
    health connections instance CdhCore.$health
    time connections instance rtcManager
    telemetry connections instance CdhCore.tlmSend
    param connections instance prmDb

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

    }

    connections Watchdog {
      watchdog.gpioSet -> gpioDriver.gpioWrite
    }

    connections BurnwireGpio {
      burnwire.gpioSet[0] -> gpioBurnwire0.gpioWrite
      burnwire.gpioSet[1] -> gpioBurnwire1.gpioWrite
    }

    connections imuManager {
      imuManager.accelerationGet -> lsm6dsoManager.accelerationGet
      imuManager.angularVelocityGet -> lsm6dsoManager.angularVelocityGet
      imuManager.magneticFieldGet -> lis2mdlManager.magneticFieldGet
      imuManager.temperatureGet -> lsm6dsoManager.temperatureGet
    }
  }
}
