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

      # --- Startup / sequence wiring (identical for both radio variants) ---
      startupManager.runSequence -> cmdSeq.seqRunIn

      # StartupManager receives sequence status from CmdSeq
      cmdSeq.seqStartOut -> startupManager.startupsequenceStarted
      cmdSeq.seqDone -> startupManager.startupCompleteSequence

      # StartupManager receives sequence status from PayloadSeq
      payloadSeq.seqStartOut -> startupManager.payloadSequenceStarted
      payloadSeq.seqDone -> startupManager.payloadCompleteSequence

      # StartupManager receives sequence status from SafeModeSeq
      # seqDone is owned by ModeManager; completion is forwarded via sequenceDoneNotify
      safeModeSeq.seqStartOut -> startupManager.safeModeSequenceStarted

      # StartupManager drives radio TX enable/disable around quiescence
      startupManager.enableTransmit  -> uspRadio.enableTransmit
      startupManager.disableTransmit -> uspRadio.disableTransmit

      # --- Radio ever enabled this boot? ---
      # UspRadio names its first-start signal radioFirstStart (radio-agnostic);
      # both ends are Fw.Signal so it binds to startupManager.loraFirstStart.
      uspRadio.radioFirstStart       -> startupManager.loraFirstStart

      modeManager.runSequence -> safeModeSeq.seqRunIn
      safeModeSeq.seqDone -> modeManager.completeSequence
      modeManager.sequenceDoneNotify -> startupManager.safeModeCompleteSequence

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
