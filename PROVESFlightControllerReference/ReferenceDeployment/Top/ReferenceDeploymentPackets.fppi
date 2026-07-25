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
    startupManager.BootCount
    modeManager.CurrentMode

    # Essential Sensor Data
    detumbleManager.AngularVelocityMagnitude

    # Power Data
    ina219SysManager.Voltage
    ina219SysManager.Power
    ina219SolManager.Power
    powerMonitor.TotalPowerConsumption
    powerMonitor.TotalPowerGenerated

    # Communications Data
    # ComCcsdsSband.tcSecurityDeframer.CurrentSequenceNumber
    ComCcsdsLora.tcSecurityDeframer.CurrentSequenceNumber
    ComCcsdsUart.tcSecurityDeframer.CurrentSequenceNumber

    lora.BytesReceived

  }

  ### Live Satellite Sensor Data Packets ###

  packet Imu id 7 group 2 {
    imuManager.AxisOrientation
    imuManager.Acceleration
    imuManager.AngularVelocity
    imuManager.MagneticField
    imuManager.AccelerometerSamplingFrequency
    imuManager.GyroscopeSamplingFrequency
    imuManager.MagnetometerSamplingFrequency
  }

  packet Radio id 8 group 2 {
    lora.LastRssi
    lora.LastSnr
    lora.BytesSent
#    sband.LastRssi
#    sband.LastSnr
  }

  packet PowerMonitor id 11 group 2 {
    ina219SysManager.Current
    ina219SysManager.Power
    ina219SolManager.Voltage
    ina219SolManager.Current
    ina219SolManager.Power
  }

  packet LightSensor id 13 group 2 {
    veml6031Face0Manager.VisibleLight
    veml6031Face1Manager.VisibleLight
    veml6031Face2Manager.VisibleLight
    veml6031Face3Manager.VisibleLight
    veml6031Face5Manager.VisibleLight
    veml6031Face6Manager.VisibleLight
    veml6031Face7Manager.VisibleLight
  }

  ### Satellite Meta Data Packets ###

  packet LoadSwitches id 10 group 3 {
    face4LoadSwitch.IsOn
    face0LoadSwitch.IsOn
    face1LoadSwitch.IsOn
    face2LoadSwitch.IsOn
    face3LoadSwitch.IsOn
    face5LoadSwitch.IsOn
    payloadPowerLoadSwitch.IsOn
    payloadBatteryLoadSwitch.IsOn
    antennaDeployer.DeployAttemptCount
  }

  packet Thermal id 12 group 3 {
    tmp112Face0Manager.Temperature
    tmp112Face1Manager.Temperature
    tmp112Face2Manager.Temperature
    tmp112Face3Manager.Temperature
    tmp112Face5Manager.Temperature
    tmp112BattCell1Manager.Temperature
    tmp112BattCell2Manager.Temperature
    tmp112BattCell3Manager.Temperature
    tmp112BattCell4Manager.Temperature
    picoTempManager.PicoTemperature
  }

  packet Detumble id 15 group 3 {
    detumbleManager.Mode
    detumbleManager.State
    detumbleManager.DetumbleStrategy
  }

  ### Payload Meta Data Packets ###

  packet CameraDebug id 14 group 4 {
    cameraHandler.BytesReceived
    cameraHandler.ExpectedSize
    cameraHandler.IsReceiving
    cameraHandler.FileOpen
    cameraHandler.FileErrorCount
    cameraHandler.ImagesSaved
  }

  ### Health and Status Packets ###
  packet Health id 2 group 5 {
    ComCcsdsLora.comQueue.comQueueDepth
    ComCcsdsLora.commsBufferManager.HiBuffs
#    ComCcsdsSband.comQueue.comQueueDepth
#    ComCcsdsSband.commsBufferManager.HiBuffs
    CdhCore.cmdDisp.CommandsDispatched
    CdhCore.cmdDisp.CommandsDropped
    rateGroup50Hz.RgMaxTime
    rateGroup10Hz.RgMaxTime
    rateGroup1Hz.RgMaxTime
    startupManager.QuiescenceEndTime
    modeManager.CurrentMode
    modeManager.SafeModeEntryCount
    modeManager.CurrentSafeModeReason
  }

  packet HealthWarnings id 3 group 5 {
    CdhCore.$health.PingLateWarnings
    ComCcsdsLora.commsBufferManager.NoBuffs
    #ComCcsdsSband.commsBufferManager.NoBuffs
    ComCcsdsUart.commsBufferManager.NoBuffs
    payloadBufferManager.NoBuffs
    rateGroup50Hz.RgCycleSlips
    rateGroup10Hz.RgCycleSlips
    rateGroup1Hz.RgCycleSlips
  }


  packet HealthAuxiliary id 4 group 5 {
    ComCcsdsLora.comQueue.buffQueueDepth
#    ComCcsdsSband.comQueue.buffQueueDepth
    payloadBufferManager.HiBuffs
    CdhCore.tlmSend.SectionEnabled
  }

  packet FileSystem id 5 group 5 {
    fsSpace.FreeSpace
    fsSpace.TotalSpace
  }

  packet Security id 6 group 5 {
    #ComCcsdsSband.provesRouter.RoutedPackets
    ComCcsdsLora.provesRouter.RoutedPackets
    ComCcsdsUart.provesRouter.RoutedPackets

    #ComCcsdsSband.provesRouter.BypassedPackets
    ComCcsdsLora.provesRouter.BypassedPackets
    ComCcsdsUart.provesRouter.BypassedPackets

    #ComCcsdsSband.provesRouter.RejectedPackets
    ComCcsdsLora.provesRouter.RejectedPackets
    ComCcsdsUart.provesRouter.RejectedPackets

    amateurRadio.count_names

  }

  packet DetumblePerformance id 16 group 5 {
    detumbleManager.TorqueDuration
    detumbleManager.TimeBetweenMagneticFieldReadings
  }

  packet DetumbleParams id 17 group 6 {
    detumbleManager.GainParam
    detumbleManager.BdotMaxThresholdParam
    detumbleManager.DeadbandUpperThresholdParam
    detumbleManager.DeadbandLowerThresholdParam
    detumbleManager.CooldownDurationParam
    detumbleManager.TorqueDurationParam
    detumbleManager.HysteresisAxisParam
  }

  packet DetumbleXPlusCoilParams id 18 group 6 {
    # X+ coil parameters
    detumbleManager.XPlusVoltageParam
    detumbleManager.XPlusResistanceParam
    detumbleManager.XPlusTurnsParam
    detumbleManager.XPlusLengthParam
    detumbleManager.XPlusWidthParam
    detumbleManager.XPlusShapeParam
  }

  packet DetumbleXMinusCoilParams id 19 group 6 {
    # X- coil parameters
    detumbleManager.XMinusVoltageParam
    detumbleManager.XMinusResistanceParam
    detumbleManager.XMinusTurnsParam
    detumbleManager.XMinusLengthParam
    detumbleManager.XMinusWidthParam
    detumbleManager.XMinusShapeParam
    detumbleManager.XTurnsParam
  }

  packet DetumbleYPlusCoilParams id 20 group 6 {
    # Y+ coil parameters
    detumbleManager.YPlusVoltageParam
    detumbleManager.YPlusResistanceParam
    detumbleManager.YPlusTurnsParam
    detumbleManager.YPlusLengthParam
    detumbleManager.YPlusWidthParam
    detumbleManager.YPlusShapeParam
  }

  packet DetumbleYMinusCoilParams id 21 group 6 {
    # Y- coil parameters
    detumbleManager.YMinusVoltageParam
    detumbleManager.YMinusResistanceParam
    detumbleManager.YMinusTurnsParam
    detumbleManager.YMinusLengthParam
    detumbleManager.YMinusWidthParam
    detumbleManager.YMinusShapeParam
    detumbleManager.YTurnsParam
  }

  packet DetumbleZMinusCoilParams id 22 group 6 {
    # Z- coil parameters
    detumbleManager.ZMinusVoltageParam
    detumbleManager.ZMinusResistanceParam
    detumbleManager.ZMinusTurnsParam
    detumbleManager.ZMinusDiameterParam
    detumbleManager.ZMinusShapeParam
    detumbleManager.ZTurnsParam
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
  cmdSeq.CS_LoadCommands
  cmdSeq.CS_CancelCommands
  cmdSeq.CS_CommandsExecuted
  cmdSeq.CS_SequencesCompleted
  cmdSeq.CS_Errors
  payloadSeq.CS_LoadCommands
  payloadSeq.CS_CancelCommands
  payloadSeq.CS_CommandsExecuted
  payloadSeq.CS_SequencesCompleted
  payloadSeq.CS_Errors
  safeModeSeq.CS_LoadCommands
  safeModeSeq.CS_CancelCommands
  safeModeSeq.CS_CommandsExecuted
  safeModeSeq.CS_SequencesCompleted
  safeModeSeq.CS_Errors
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

  CdhCore.tlmSend.GroupConfigs
  CdhCore.version.FrameworkVersion
  CdhCore.version.ProjectVersion
  CdhCore.version.LibraryVersion01

  watchdog.WatchdogTransitions

}
