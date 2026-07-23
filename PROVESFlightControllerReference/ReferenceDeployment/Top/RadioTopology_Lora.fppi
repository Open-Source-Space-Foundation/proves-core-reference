# Radio topology connections for v5c / v5d (Zephyr LoRa legacy path).
# Included by topology.fpp inside topology ReferenceDeployment { }.
# Selected by CMakeLists.txt when CONFIG_LORA_BASICS_MODEM_DRIVERS is not set.

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

      # Startup sequence wiring
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
