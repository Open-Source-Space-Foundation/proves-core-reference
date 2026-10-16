# Radio connections for v5e (Semtech USP driver).
# Included by topology.fpp inside topology ReferenceDeployment { }.
# Selected by Top/CMakeLists.txt per board.
#
# UspRadio is active, so its queue provides back-pressure and no ComRetry shim is needed.

    instance uspRadio

    connections CommunicationsRadio {
      uspRadio.allocate   -> ComCcsdsLora.commsBufferManager.bufferGetCallee
      uspRadio.deallocate -> ComCcsdsLora.commsBufferManager.bufferSendIn

      # UspRadio <-> FrameAccumulator (Uplink)
      uspRadio.dataOut -> ComCcsdsLora.frameAccumulator.dataIn
      ComCcsdsLora.frameAccumulator.dataReturnOut -> uspRadio.dataReturnIn

      # UspRadio <-> Framer (Downlink)
      ComCcsdsLora.framer.dataOut    -> uspRadio.dataIn
      uspRadio.dataReturnOut         -> ComCcsdsLora.framer.dataReturnIn
      uspRadio.comStatusOut          -> downlinkDelay.comStatusIn
      downlinkDelay.comStatusOut     -> ComCcsdsLora.framer.comStatusIn

      # Startup and sequence wiring (same in RadioTopology_Lora.fppi)
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

      # Both ports are Fw.Signal, so the names may differ.
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
      # 1 Hz tick for the revert deadline and telemetry flush. Slot 20 is the first unused rate-group slot.
      rateGroup1Hz.RateGroupMemberOut[20] -> uspRadio.run
    }
