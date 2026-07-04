module ReferenceDeployment {

  # ----------------------------------------------------------------------
  # Radio topology: Zephyr::UspRadio (v5e+ — Semtech USP path)
  #
  # Wires the `uspRadio` active component to the ComCcsdsLora subtopology's
  # Svc.Com interface (framer / frameAccumulator / commsBufferManager).
  # No ComRetry shim: UspRadio handles its own back-pressure via the
  # active-component queue.
  # Also registers the run port on rateGroup1Hz (slot 20 — unused by LoRa path)
  # and preserves startup-sequence and cancel-sequence wiring.
  #
  # Selected by CMakeLists.txt when CONFIG_LORA_BASICS_MODEM_DRIVERS is set.
  # ----------------------------------------------------------------------

  topology ReferenceDeployment {

    instance uspRadio

    connections CommunicationsRadio {
      uspRadio.allocate   -> ComCcsdsLora.commsBufferManager.bufferGetCallee
      uspRadio.deallocate -> ComCcsdsLora.commsBufferManager.bufferSendIn

      # UspRadio <-> FrameAccumulator (Uplink)
      uspRadio.dataOut -> ComCcsdsLora.frameAccumulator.dataIn
      ComCcsdsLora.frameAccumulator.dataReturnOut -> uspRadio.dataReturnIn

      # UspRadio <-> Framer (Downlink — direct, no retry shim)
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
      # UspRadio run port: revert-deadline tick + telemetry flush (1 Hz)
      # Slot 20 is free in the legacy LoRa topology.
      rateGroup1Hz.RateGroupMemberOut[20] -> uspRadio.run
    }

  }

}
