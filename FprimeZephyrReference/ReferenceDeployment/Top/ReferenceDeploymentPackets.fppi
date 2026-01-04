telemetry packets ReferenceDeploymentPackets {

  ########### Packet Group Key ###########
  # 1 - Beacon
  # 2 - Live Satellite Sensor Data
  # 3 - Satellite Meta Data
  # 4 - Payload Meta Data
  # 5 - Health and Status
  # 6 - Parameters
  ########################################

  packet Beacon id 1 group 1 {
    # Satellite Meta Data
    ReferenceDeployment.startupManager.BootCount
    ReferenceDeployment.modeManager.CurrentMode

    # Essential Sensor Data
    ReferenceDeployment.imuManager.AngularVelocity

    # Power Data
    ReferenceDeployment.ina219SysManager.Voltage
    ReferenceDeployment.ina219SysManager.Power
    ReferenceDeployment.ina219SolManager.Power
    ReferenceDeployment.powerMonitor.TotalPowerConsumption
    ReferenceDeployment.powerMonitor.TotalPowerGenerated

    # Communications Data
    # ComCcsdsSband.authenticatesband.CurrentSequenceNumber
    ComCcsdsLora.authenticatelora.CurrentSequenceNumber
    ComCcsdsUart.authenticate.CurrentSequenceNumber

    lora.BytesReceived

  }

  ### Live Satellite Sensor Data Packets ###

  packet Imu id 7 group 2 {
    ReferenceDeployment.imuManager.AxisOrientation
    ReferenceDeployment.imuManager.Acceleration
    ReferenceDeployment.imuManager.AngularVelocity
    ReferenceDeployment.imuManager.MagneticField
    ReferenceDeployment.imuManager.AccelerometerSamplingFrequency
    ReferenceDeployment.imuManager.GyroscopeSamplingFrequency
    ReferenceDeployment.imuManager.MagnetometerSamplingFrequency
  }

  packet Radio id 8 group 2 {
    lora.LastRssi
    lora.LastSnr
    lora.BytesSent
#    sband.LastRssi
#    sband.LastSnr
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

  packet Detumble id 15 group 3 {
    ReferenceDeployment.detumbleManager.Mode
    ReferenceDeployment.detumbleManager.State
    ReferenceDeployment.detumbleManager.DetumbleStrategy
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
    ComCcsdsLora.comQueue.comQueueDepth
    ComCcsdsLora.commsBufferManager.HiBuffs
#    ComCcsdsSband.comQueue.comQueueDepth
#    ComCcsdsSband.commsBufferManager.HiBuffs
    CdhCore.cmdDisp.CommandsDispatched
    CdhCore.cmdDisp.CommandsDropped
    ReferenceDeployment.rateGroup50Hz.RgMaxTime
    ReferenceDeployment.rateGroup10Hz.RgMaxTime
    ReferenceDeployment.rateGroup1Hz.RgMaxTime
    ReferenceDeployment.startupManager.QuiescenceEndTime
    ReferenceDeployment.modeManager.CurrentMode
    ReferenceDeployment.modeManager.SafeModeEntryCount
    ReferenceDeployment.modeManager.CurrentSafeModeReason
  }

  packet HealthWarnings id 3 group 5 {
    CdhCore.$health.PingLateWarnings
    ComCcsdsLora.commsBufferManager.NoBuffs
    #ComCcsdsSband.commsBufferManager.NoBuffs
    ComCcsdsUart.commsBufferManager.NoBuffs
    payloadBufferManager.NoBuffs
    ReferenceDeployment.rateGroup50Hz.RgCycleSlips
    ReferenceDeployment.rateGroup10Hz.RgCycleSlips
    ReferenceDeployment.rateGroup1Hz.RgCycleSlips
  }


  packet HealthAuxiliary id 4 group 5 {
    ComCcsdsLora.comQueue.buffQueueDepth
#    ComCcsdsSband.comQueue.buffQueueDepth
    payloadBufferManager.HiBuffs
    CdhCore.tlmSend.SendLevel
  }

  packet FileSystem id 5 group 5 {
    fsSpace.FreeSpace
    fsSpace.TotalSpace
  }

  packet Authenticate id 6 group 5 {
    #ComCcsdsSband.authenticatesband.AuthenticatedPacketsCount
    ComCcsdsLora.authenticatelora.AuthenticatedPacketsCount
    ComCcsdsUart.authenticate.AuthenticatedPacketsCount

    #ComCcsdsSband.authenticatesband.RejectedPacketsCount
    ComCcsdsLora.authenticatelora.RejectedPacketsCount
    ComCcsdsUart.authenticate.RejectedPacketsCount

    #ComCcsdsSband.authenticationRouter.ByPassedRouter
    ComCcsdsLora.authenticationRouter.ByPassedRouter
    ComCcsdsUart.authenticationRouter.ByPassedRouter

    #ComCcsdsSband.authenticationRouter.PassedRouter
    ComCcsdsLora.authenticationRouter.PassedRouter
    ComCcsdsUart.authenticationRouter.PassedRouter

    #ComCcsdsSband.authenticationRouter.FailedRouter
    ComCcsdsLora.authenticationRouter.FailedRouter
    ComCcsdsUart.authenticationRouter.FailedRouter

    ReferenceDeployment.amateurRadio.count_names

  }

  packet DetumblePerformance id 16 group 5 {
    ReferenceDeployment.detumbleManager.TorqueDuration
    ReferenceDeployment.detumbleManager.TimeBetweenMagneticFieldReadings
  }

  packet DetumbleParams id 17 group 6 {
    ReferenceDeployment.detumbleManager.GainParam
    ReferenceDeployment.detumbleManager.BdotMaxThresholdParam
    ReferenceDeployment.detumbleManager.DeadbandUpperThresholdParam
    ReferenceDeployment.detumbleManager.DeadbandLowerThresholdParam
    ReferenceDeployment.detumbleManager.CooldownDurationParam
    ReferenceDeployment.detumbleManager.TorqueDurationParam
    ReferenceDeployment.detumbleManager.HysteresisAxisParam
  }

  packet DetumbleXPlusCoilParams id 18 group 6 {
    # X+ coil parameters
    ReferenceDeployment.detumbleManager.XPlusVoltageParam
    ReferenceDeployment.detumbleManager.XPlusResistanceParam
    ReferenceDeployment.detumbleManager.XPlusTurnsParam
    ReferenceDeployment.detumbleManager.XPlusLengthParam
    ReferenceDeployment.detumbleManager.XPlusWidthParam
    ReferenceDeployment.detumbleManager.XPlusShapeParam
  }

  packet DetumbleXMinusCoilParams id 19 group 6 {
    # X- coil parameters
    ReferenceDeployment.detumbleManager.XMinusVoltageParam
    ReferenceDeployment.detumbleManager.XMinusResistanceParam
    ReferenceDeployment.detumbleManager.XMinusTurnsParam
    ReferenceDeployment.detumbleManager.XMinusLengthParam
    ReferenceDeployment.detumbleManager.XMinusWidthParam
    ReferenceDeployment.detumbleManager.XMinusShapeParam
  }

  packet DetumbleYPlusCoilParams id 20 group 6 {
    # Y+ coil parameters
    ReferenceDeployment.detumbleManager.YPlusVoltageParam
    ReferenceDeployment.detumbleManager.YPlusResistanceParam
    ReferenceDeployment.detumbleManager.YPlusTurnsParam
    ReferenceDeployment.detumbleManager.YPlusLengthParam
    ReferenceDeployment.detumbleManager.YPlusWidthParam
    ReferenceDeployment.detumbleManager.YPlusShapeParam
  }

  packet DetumbleYMinusCoilParams id 21 group 6 {
    # Y- coil parameters
    ReferenceDeployment.detumbleManager.YMinusVoltageParam
    ReferenceDeployment.detumbleManager.YMinusResistanceParam
    ReferenceDeployment.detumbleManager.YMinusTurnsParam
    ReferenceDeployment.detumbleManager.YMinusLengthParam
    ReferenceDeployment.detumbleManager.YMinusWidthParam
    ReferenceDeployment.detumbleManager.YMinusShapeParam
  }

  packet DetumbleZMinusCoilParams id 22 group 6 {
    # Z- coil parameters
    ReferenceDeployment.detumbleManager.ZMinusVoltageParam
    ReferenceDeployment.detumbleManager.ZMinusResistanceParam
    ReferenceDeployment.detumbleManager.ZMinusTurnsParam
    ReferenceDeployment.detumbleManager.ZMinusDiameterParam
    ReferenceDeployment.detumbleManager.ZMinusShapeParam
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
  ReferenceDeployment.payloadSeq.CS_LoadCommands
  ReferenceDeployment.payloadSeq.CS_CancelCommands
  ReferenceDeployment.payloadSeq.CS_CommandsExecuted
  ReferenceDeployment.payloadSeq.CS_SequencesCompleted
  ReferenceDeployment.payloadSeq.CS_Errors
  ReferenceDeployment.safeModeSeq.CS_LoadCommands
  ReferenceDeployment.safeModeSeq.CS_CancelCommands
  ReferenceDeployment.safeModeSeq.CS_CommandsExecuted
  ReferenceDeployment.safeModeSeq.CS_SequencesCompleted
  ReferenceDeployment.safeModeSeq.CS_Errors
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

  ComCcsdsLora.commsBufferManager.EmptyBuffs
#  ComCcsdsSband.commsBufferManager.EmptyBuffs
  ComCcsdsUart.commsBufferManager.EmptyBuffs
  payloadBufferManager.EmptyBuffs

  ComCcsdsLora.commsBufferManager.TotalBuffs
  ComCcsdsLora.commsBufferManager.CurrBuffs
#  ComCcsdsSband.commsBufferManager.TotalBuffs
#  ComCcsdsSband.commsBufferManager.CurrBuffs

  payloadBufferManager.TotalBuffs
  payloadBufferManager.CurrBuffs

  CdhCore.version.FrameworkVersion
  CdhCore.version.ProjectVersion
  CdhCore.version.LibraryVersion01

  ReferenceDeployment.watchdog.WatchdogTransitions

}
