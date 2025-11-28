telemetry packets ReferenceDeploymentPackets {

  packet Health id 1 group 1 {
    CdhCore.cmdDisp.CommandsDispatched
    CdhCore.cmdDisp.CommandsDropped
    ComCcsds.comQueue.comQueueDepth
    ComCcsds.commsBufferManager.HiBuffs
    ComCcsdsUart.comQueue.comQueueDepth
    ComCcsdsUart.commsBufferManager.HiBuffs
    ReferenceDeployment.rateGroup10Hz.RgMaxTime
    ReferenceDeployment.rateGroup1Hz.RgMaxTime
    # ReferenceDeployment.rateGroup1_6Hz.RgMaxTime
    ReferenceDeployment.startupManager.BootCount
    ReferenceDeployment.startupManager.QuiescenceEndTime
    ReferenceDeployment.modeManager.CurrentMode
    ReferenceDeployment.modeManager.SafeModeEntryCount
  }

  packet HealthWarnings id 2 group 1 {
    CdhCore.$health.PingLateWarnings
    ComCcsds.commsBufferManager.NoBuffs
    ComCcsds.commsBufferManager.EmptyBuffs
    ComCcsdsUart.commsBufferManager.NoBuffs
    ComCcsdsUart.commsBufferManager.EmptyBuffs
    ReferenceDeployment.rateGroup10Hz.RgCycleSlips
    ReferenceDeployment.rateGroup1Hz.RgCycleSlips
    # ReferenceDeployment.rateGroup1_6Hz.RgCycleSlips
  }


  packet HealthAuxillary id 3 group 2 {
    ComCcsds.commsBufferManager.TotalBuffs
    ComCcsds.commsBufferManager.CurrBuffs
    ComCcsds.comQueue.buffQueueDepth
    ComCcsdsUart.commsBufferManager.TotalBuffs
    ComCcsdsUart.commsBufferManager.CurrBuffs
    ComCcsdsUart.comQueue.buffQueueDepth
    CdhCore.tlmSend.SendLevel
  }

  packet Version id 4 group 3 {
    CdhCore.version.FrameworkVersion
    CdhCore.version.ProjectVersion
    CdhCore.version.LibraryVersion01
    fsSpace.FreeSpace
    fsSpace.TotalSpace
  }

  packet Led id 5 group 4 {
    ReferenceDeployment.watchdog.WatchdogTransitions
  }

  packet Imu id 6 group 4 {
    ReferenceDeployment.lsm6dsoManager.Acceleration
    ReferenceDeployment.lsm6dsoManager.AngularVelocity
    ReferenceDeployment.lsm6dsoManager.Temperature
    ReferenceDeployment.lis2mdlManager.MagneticField
  }

  packet LoRa id 7 group 4 {
    lora.LastRssi
    lora.LastSnr
  }

  packet Antenna id 8 group 4 {
    ReferenceDeployment.antennaDeployer.DeployAttemptCount
    ReferenceDeployment.antennaDeployer.LastDistance
  }

  packet LoadSwitches id 9 group 4 {
    ReferenceDeployment.face4LoadSwitch.IsOn
    ReferenceDeployment.face0LoadSwitch.IsOn
    ReferenceDeployment.face1LoadSwitch.IsOn
    ReferenceDeployment.face2LoadSwitch.IsOn
    ReferenceDeployment.face3LoadSwitch.IsOn
    ReferenceDeployment.face5LoadSwitch.IsOn
    ReferenceDeployment.payloadPowerLoadSwitch.IsOn
    ReferenceDeployment.payloadBatteryLoadSwitch.IsOn
  }

  packet PowerMonitor id 10 group 4 {
    ReferenceDeployment.ina219SysManager.Voltage
    ReferenceDeployment.ina219SysManager.Current
    ReferenceDeployment.ina219SysManager.Power
    ReferenceDeployment.ina219SolManager.Voltage
    ReferenceDeployment.ina219SolManager.Current
    ReferenceDeployment.ina219SolManager.Power
    ReferenceDeployment.powerMonitor.TotalPowerConsumption
    ReferenceDeployment.powerMonitor.TotalPowerGenerated
  }

  packet authenticate id 11 group 4 {
    ComCcsdsUart.authenticate.AuthenticatedPacketsCount
    ComCcsdsUart.authenticate.RejectedPacketsCount
    ComCcsdsUart.authenticate.CurrentSequenceNumber

  }

  packet TcaMonitor id 12 group 4 {
    ReferenceDeployment.tcaMonitor.Healthy
    ReferenceDeployment.muxChannel0Monitor.Healthy
    ReferenceDeployment.muxChannel1Monitor.Healthy
    ReferenceDeployment.muxChannel2Monitor.Healthy
    ReferenceDeployment.muxChannel3Monitor.Healthy
    ReferenceDeployment.muxChannel4Monitor.Healthy
    ReferenceDeployment.muxChannel5Monitor.Healthy
    ReferenceDeployment.muxChannel7Monitor.Healthy
  }

  packet Thermal id 13 group 4 {
    ReferenceDeployment.tmp112Face0Manager.Temperature
    ReferenceDeployment.tmp112Face1Manager.Temperature
    ReferenceDeployment.tmp112Face2Manager.Temperature
    ReferenceDeployment.tmp112Face3Manager.Temperature
    ReferenceDeployment.tmp112Face5Manager.Temperature
    ReferenceDeployment.tmp112BattCell1Manager.Temperature
    ReferenceDeployment.tmp112BattCell2Manager.Temperature
    ReferenceDeployment.tmp112BattCell3Manager.Temperature
    ReferenceDeployment.tmp112BattCell4Manager.Temperature
  }

} omit {
  CdhCore.cmdDisp.CommandErrors
  # Only has one library, no custom versions
  CdhCore.version.LibraryVersion02
  CdhCore.version.LibraryVersion03
  CdhCore.version.LibraryVersion04
  CdhCore.version.LibraryVersion05
  CdhCore.version.LibraryVersion06
  CdhCore.version.LibraryVersion07
  CdhCore.version.LibraryVersion08
  CdhCore.version.LibraryVersion09
  CdhCore.version.LibraryVersion10
  CdhCore.version.CustomVersion01
  CdhCore.version.CustomVersion02
  CdhCore.version.CustomVersion03
  CdhCore.version.CustomVersion04
  CdhCore.version.CustomVersion05
  CdhCore.version.CustomVersion06
  CdhCore.version.CustomVersion07
  CdhCore.version.CustomVersion08
  CdhCore.version.CustomVersion09
  CdhCore.version.CustomVersion10
  ReferenceDeployment.cmdSeq.CS_LoadCommands
  ReferenceDeployment.cmdSeq.CS_CancelCommands
  ReferenceDeployment.cmdSeq.CS_CommandsExecuted
  ReferenceDeployment.cmdSeq.CS_SequencesCompleted
  ReferenceDeployment.cmdSeq.CS_Errors
  FileHandling.fileUplink.FilesReceived
  FileHandling.fileUplink.PacketsReceived
  FileHandling.fileDownlink.FilesSent
  FileHandling.fileDownlink.PacketsSent
  FileHandling.fileManager.CommandsExecuted
  FileHandling.fileManager.Errors
  FileHandling.fileUplink.Warnings
  FileHandling.fileDownlink.Warnings
}
