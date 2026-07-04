module ReferenceDeployment {

  # ----------------------------------------------------------------------
  # Radio topology: Zephyr::LoRa (v5c / v5d — legacy path)
  #
  # Wires the `lora` passive driver to the ComCcsdsLora subtopology's
  # Svc.Com interface (framer / frameAccumulator / commsBufferManager),
  # with the ComRetry retry shim in the downlink path.
  # Also registers per-board rate-group connections for lora's schedIn.
  #
  # Selected by CMakeLists.txt when CONFIG_LORA_BASICS_MODEM_DRIVERS is not
  # set.
  # ----------------------------------------------------------------------

  topology ReferenceDeployment {

    instance lora
    instance loraRetry

    connections CommunicationsRadio {
      lora.allocate      -> ComCcsdsLora.commsBufferManager.bufferGetCallee
      lora.deallocate    -> ComCcsdsLora.commsBufferManager.bufferSendIn

      # ComDriver <-> FrameAccumulator (Uplink)
      lora.dataOut -> ComCcsdsLora.frameAccumulator.dataIn
      ComCcsdsLora.frameAccumulator.dataReturnOut -> lora.dataReturnIn

      # ComStub <-> ComDriver (Downlink) with retry shim
      ComCcsdsLora.framer.dataOut -> loraRetry.dataIn
      loraRetry.dataOut -> lora.dataIn

      lora.dataReturnOut -> loraRetry.dataReturnIn
      loraRetry.dataReturnOut -> ComCcsdsLora.framer.dataReturnIn

      lora.comStatusOut -> loraRetry.comStatusIn
      loraRetry.comStatusOut -> downlinkDelay.comStatusIn
      downlinkDelay.comStatusOut -> ComCcsdsLora.framer.comStatusIn

      # Startup sequence wiring (board-agnostic; placed here to avoid
      # duplication; identical in the USP path)
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

    # LoRa (passive) has no run port.
    # Rate-group member slots used here must match RadioTopology_Usp.fpp.
    # (No additional rate group connections needed for passive LoRa driver.)

  }

}
