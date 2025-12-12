telemetry packets ReferenceDeploymentPackets {

  ########### Packet Group Key ###########
  # 1 - Beacon
  # 2 - Live Satellite Sensor Data
  # 3 - Satellite Meta Data
  # 4 - Payload Meta Data
  # 5 - Health and Status
  ########################################

  packet Beacon id 1 group 1 {
    # Satellite Meta Data
    ReferenceDeployment.startupManager.BootCount
    ReferenceDeployment.modeManager.CurrentMode

    # Essential Sensor Data
    ReferenceDeployment.lsm6dsoManager.AngularVelocity

    # Power Data
    ReferenceDeployment.ina219SysManager.Voltage
    ReferenceDeployment.ina219SysManager.Power
    ReferenceDeployment.ina219SolManager.Power
    ReferenceDeployment.powerMonitor.TotalPowerConsumption
    ReferenceDeployment.powerMonitor.TotalPowerGenerated
  }

  ### Live Satellite Sensor Data Packets ###

  packet Imu id 7 group 2 {
    ReferenceDeployment.lsm6dsoManager.Acceleration
    ReferenceDeployment.lsm6dsoManager.AngularVelocity
    ReferenceDeployment.lsm6dsoManager.Temperature
    ReferenceDeployment.lis2mdlManager.MagneticField
  }

  packet LoRa id 8 group 2 {
    lora.LastRssi
    lora.LastSnr
  }

  packet PowerMonitor id 11 group 2 {
    ReferenceDeployment.ina219SysManager.Current
    ReferenceDeployment.ina219SysManager.Power
    ReferenceDeployment.ina219SolManager.Voltage
    ReferenceDeployment.ina219SolManager.Current
    ReferenceDeployment.ina219SolManager.Power
  }

  packet LightSensor id 13 group 2 {
    ReferenceDeployment.veml6031Face0Manager.VisibleLight
    ReferenceDeployment.veml6031Face1Manager.VisibleLight
    ReferenceDeployment.veml6031Face2Manager.VisibleLight
    ReferenceDeployment.veml6031Face3Manager.VisibleLight
    ReferenceDeployment.veml6031Face5Manager.VisibleLight
    ReferenceDeployment.veml6031Face6Manager.VisibleLight
    ReferenceDeployment.veml6031Face7Manager.VisibleLight
  }

  ### Satellite Meta Data Packets ###

  packet LoadSwitches id 10 group 3 {
    ReferenceDeployment.face4LoadSwitch.IsOn
    ReferenceDeployment.face0LoadSwitch.IsOn
    ReferenceDeployment.face1LoadSwitch.IsOn
    ReferenceDeployment.face2LoadSwitch.IsOn
    ReferenceDeployment.face3LoadSwitch.IsOn
    ReferenceDeployment.face5LoadSwitch.IsOn
    ReferenceDeployment.payloadPowerLoadSwitch.IsOn
    ReferenceDeployment.payloadBatteryLoadSwitch.IsOn
    ReferenceDeployment.antennaDeployer.DeployAttemptCount
  }

  packet Thermal id 12 group 3 {
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

  ### Payload Meta Data Packets ###

  packet CameraDebug id 14 group 4 {
    ReferenceDeployment.cameraHandler.BytesReceived
    ReferenceDeployment.cameraHandler.ExpectedSize
    ReferenceDeployment.cameraHandler.IsReceiving
    ReferenceDeployment.cameraHandler.FileOpen
    ReferenceDeployment.cameraHandler.FileErrorCount
    ReferenceDeployment.cameraHandler.ImagesSaved
  }

  ### Health and Status Packets ###
  packet Health id 2 group 5 {
    ComCcsds.comQueue.comQueueDepth
    ComCcsds.commsBufferManager.HiBuffs
    CdhCore.cmdDisp.CommandsDispatched
    CdhCore.cmdDisp.CommandsDropped
    ReferenceDeployment.rateGroup10Hz.RgMaxTime
    ReferenceDeployment.rateGroup1Hz.RgMaxTime
    ReferenceDeployment.rateGroup1_6Hz.RgMaxTime
    ReferenceDeployment.rateGroup1_10Hz.RgMaxTime
    ReferenceDeployment.startupManager.QuiescenceEndTime
    ReferenceDeployment.modeManager.CurrentMode
    ReferenceDeployment.modeManager.SafeModeEntryCount
    ReferenceDeployment.modeManager.CurrentSafeModeReason
  }

  packet HealthWarnings id 3 group 5 {
    CdhCore.$health.PingLateWarnings
    ComCcsds.commsBufferManager.NoBuffs
    ComCcsdsUart.commsBufferManager.NoBuffs
    payloadBufferManager.NoBuffs
    payloadBufferManager2.NoBuffs
    ReferenceDeployment.rateGroup10Hz.RgCycleSlips
    ReferenceDeployment.rateGroup1Hz.RgCycleSlips
    ReferenceDeployment.rateGroup1_6Hz.RgCycleSlips
    ReferenceDeployment.rateGroup1_10Hz.RgCycleSlips
  }


  packet HealthAuxiliary id 4 group 5 {
    ComCcsds.comQueue.buffQueueDepth
    payloadBufferManager.HiBuffs
    payloadBufferManager2.HiBuffs
    CdhCore.tlmSend.SendLevel
  }

  packet FileSystem id 5 group 5 {
    fsSpace.FreeSpace
    fsSpace.TotalSpace
  }

  packet Parameters id 15 group 6 {
    ReferenceDeployment.bDotDetumble.Gain
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

  # Moved to omit as they are not useful in normal ops
  ComCcsdsUart.comQueue.comQueueDepth
  ComCcsdsUart.commsBufferManager.HiBuffs
  ComCcsdsUart.commsBufferManager.TotalBuffs
  ComCcsdsUart.commsBufferManager.CurrBuffs
  ComCcsdsUart.comQueue.buffQueueDepth

  ComCcsds.commsBufferManager.EmptyBuffs
  ComCcsdsUart.commsBufferManager.EmptyBuffs
  payloadBufferManager.EmptyBuffs
  payloadBufferManager2.EmptyBuffs

  ComCcsds.commsBufferManager.TotalBuffs
  ComCcsds.commsBufferManager.CurrBuffs
  payloadBufferManager.TotalBuffs
  payloadBufferManager.CurrBuffs
  payloadBufferManager2.TotalBuffs
  payloadBufferManager2.CurrBuffs

  CdhCore.version.FrameworkVersion
  CdhCore.version.ProjectVersion
  CdhCore.version.LibraryVersion01

  ReferenceDeployment.watchdog.WatchdogTransitions


  # Moved to omit as this is specific to Alcyone Build
  ReferenceDeployment.cameraHandler2.BytesReceived
  ReferenceDeployment.cameraHandler2.ExpectedSize
  ReferenceDeployment.cameraHandler2.IsReceiving
  ReferenceDeployment.cameraHandler2.FileOpen
  ReferenceDeployment.cameraHandler2.FileErrorCount
  ReferenceDeployment.cameraHandler2.ImagesSaved

}
