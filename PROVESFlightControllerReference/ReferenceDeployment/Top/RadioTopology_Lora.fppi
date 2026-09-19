# Radio connections for v5c / v5d (Zephyr LoRa driver).
# Included by topology.fpp inside topology ReferenceDeployment { }.
# Selected by Top/CMakeLists.txt per board.

    instance lora
    instance loraRetry

    connections CommunicationsRadio {
      lora.allocate      -> ComCcsdsLora.commsBufferManager.bufferGetCallee
      lora.deallocate    -> ComCcsdsLora.commsBufferManager.bufferSendIn

      # ComDriver <-> FrameAccumulator (Uplink)
      lora.dataOut -> ComCcsdsLora.frameAccumulator.dataIn
      ComCcsdsLora.frameAccumulator.dataReturnOut -> lora.dataReturnIn

      # ComStub <-> ComDriver (Downlink) with ComRetry shim
      ComCcsdsLora.framer.dataOut -> loraRetry.dataIn
      loraRetry.dataOut -> lora.dataIn

      lora.dataReturnOut -> loraRetry.dataReturnIn
      loraRetry.dataReturnOut -> ComCcsdsLora.framer.dataReturnIn

      lora.comStatusOut -> loraRetry.comStatusIn
      loraRetry.comStatusOut -> downlinkDelay.comStatusIn
      downlinkDelay.comStatusOut -> ComCcsdsLora.framer.comStatusIn

      # Startup and sequence wiring (same in RadioTopology_Usp.fppi)
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
      startupManager.enableTransmit -> lora.enableTransmit
      startupManager.disableTransmit -> lora.disableTransmit

      # --- Radio ever enabled this boot? ---
      lora.loraFirstStart -> startupManager.loraFirstStart

      modeManager.runSequence -> safeModeSeq.seqRunIn
      safeModeSeq.seqDone -> modeManager.completeSequence
      modeManager.sequenceDoneNotify -> startupManager.safeModeCompleteSequence

      # RTC time change cancels running sequences
      rtcManager.cancelSequences[0] -> cmdSeq.seqCancelIn
      rtcManager.cancelSequences[1] -> payloadSeq.seqCancelIn
      rtcManager.cancelSequences[2] -> safeModeSeq.seqCancelIn
    }
