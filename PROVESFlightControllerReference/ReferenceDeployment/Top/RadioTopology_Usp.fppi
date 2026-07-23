# Radio topology connections for v5e (Semtech USP path).
# Included by topology.fpp inside topology ReferenceDeployment { }.
# Selected by CMakeLists.txt when CONFIG_LORA_BASICS_MODEM_DRIVERS is set.
#
# UspRadio is active; no ComRetry shim needed (active-component queue provides
# back-pressure). Direct framer -> uspRadio.dataIn path.

    instance uspRadio

    connections CommunicationsRadio {
      uspRadio.allocate   -> ComCcsdsLora.commsBufferManager.bufferGetCallee
      uspRadio.deallocate -> ComCcsdsLora.commsBufferManager.bufferSendIn

      # UspRadio <-> FrameAccumulator (Uplink)
      uspRadio.dataOut -> ComCcsdsLora.frameAccumulator.dataIn
      ComCcsdsLora.frameAccumulator.dataReturnOut -> uspRadio.dataReturnIn

      # UspRadio <-> Framer (Downlink, direct — no retry shim)
      ComCcsdsLora.framer.dataOut    -> uspRadio.dataIn
      uspRadio.dataReturnOut         -> ComCcsdsLora.framer.dataReturnIn
      uspRadio.comStatusOut          -> downlinkDelay.comStatusIn
      downlinkDelay.comStatusOut     -> ComCcsdsLora.framer.comStatusIn

      # Startup sequence wiring (identical to Lora path)
      startupManager.runSequence -> cmdSeq.seqRunIn
      cmdSeq.seqStartOut -> startupManager.sequenceStarted
      cmdSeq.seqDone -> startupManager.completeSequence

      modeManager.runSequence -> safeModeSeq.seqRunIn
      safeModeSeq.seqDone -> modeManager.completeSequence

      # RTC time change cancels running sequences
      rtcManager.cancelSequences[0] -> cmdSeq.seqCancelIn
      rtcManager.cancelSequences[1] -> payloadSeq.seqCancelIn
      rtcManager.cancelSequences[2] -> safeModeSeq.seqCancelIn
    }

    connections RadioRateGroup {
      # UspRadio run port: revert-deadline tick + telemetry flush at 1 Hz.
      # Slot 20 is the first free slot after the legacy topology's highest used (19).
      rateGroup1Hz.RateGroupMemberOut[20] -> uspRadio.run
    }
