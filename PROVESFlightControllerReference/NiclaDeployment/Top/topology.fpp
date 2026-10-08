module NiclaDeployment {

  # ----------------------------------------------------------------------
  # Symbolic constants for port numbers
  # ----------------------------------------------------------------------

  enum Ports_RateGroups {
    rateGroup1Hz
  }

  deployment topology NiclaDeployment {

  # ----------------------------------------------------------------------
  # Subtopology imports
  # ----------------------------------------------------------------------
    import ComCcsdsNicla.Subtopology

  # ----------------------------------------------------------------------
  # Instances used in the topology
  # ----------------------------------------------------------------------
    instance chronoTime
    instance rateGroup1Hz
    instance rateGroupDriver
    instance timer
    instance comDriver

    instance cmdDisp
    instance events
    instance $health
    instance tlmSend
    instance version
    instance textLogger
    instance fatalAdapter
    instance fatalHandler

    instance gpioWatchdog
    instance watchdog

  # ----------------------------------------------------------------------
  # Pattern graph specifiers
  # ----------------------------------------------------------------------

    command connections instance cmdDisp
    event connections instance events
    text event connections instance textLogger
    health connections instance $health
    time connections instance chronoTime
    telemetry connections instance tlmSend

  # ----------------------------------------------------------------------
  # Telemetry packets (only used when TlmPacketizer is used)
  # ----------------------------------------------------------------------

  include "NiclaDeploymentPackets.fppi"

  # ----------------------------------------------------------------------
  # Direct graph specifiers
  # ----------------------------------------------------------------------

    connections ComCcsds_CdhCore {
      # Core events and telemetry to communication queue
      events.PktSend -> ComCcsdsNicla.comQueue.comPacketQueueIn[ComCcsdsNicla.Ports_ComPacketQueue.EVENTS]
      tlmSend.PktSend -> ComCcsdsNicla.comQueue.comPacketQueueIn[ComCcsdsNicla.Ports_ComPacketQueue.TELEMETRY]

      # Router to Command Dispatcher
      ComCcsdsNicla.fprimeRouter.commandOut -> cmdDisp.seqCmdBuff
      cmdDisp.seqCmdStatus -> ComCcsdsNicla.fprimeRouter.cmdResponseIn

    }

    connections Communications {
      # ComDriver buffer allocations
      comDriver.allocate      -> ComCcsdsNicla.commsBufferManager.bufferGetCallee
      comDriver.deallocate    -> ComCcsdsNicla.commsBufferManager.bufferSendIn

      # ComDriver <-> ComStub (Uplink)
      comDriver.$recv                     -> ComCcsdsNicla.comStub.drvReceiveIn
      ComCcsdsNicla.comStub.drvReceiveReturnOut -> comDriver.recvReturnIn

      # ComStub <-> ComDriver (Downlink)
      ComCcsdsNicla.comStub.drvSendOut      -> comDriver.$send
      comDriver.ready         -> ComCcsdsNicla.comStub.drvConnected
    }

    connections RateGroups {
      # timer to drive rate group
      timer.CycleOut -> rateGroupDriver.CycleIn

      # All rate group activity is now on the 1Hz group
      rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup1Hz] -> rateGroup1Hz.CycleIn
      rateGroup1Hz.RateGroupMemberOut[6] -> comDriver.schedIn
      rateGroup1Hz.RateGroupMemberOut[0] -> ComCcsdsNicla.comQueue.run
      rateGroup1Hz.RateGroupMemberOut[1] -> $health.Run
      rateGroup1Hz.RateGroupMemberOut[2] -> ComCcsdsNicla.commsBufferManager.schedIn
      rateGroup1Hz.RateGroupMemberOut[3] -> tlmSend.Run
      rateGroup1Hz.RateGroupMemberOut[4] -> ComCcsdsNicla.aggregator.timeout
      rateGroup1Hz.RateGroupMemberOut[5] -> watchdog.run
    }

    connections Watchdog {
      watchdog.gpioSet -> gpioWatchdog.gpioWrite
    }

    connections NiclaDeployment {

    }

    connections FatalHandler {
      events.FatalAnnounce -> fatalHandler.FatalReceive
      fatalHandler.stopWatchdog -> watchdog.stop

    }

  }

}
