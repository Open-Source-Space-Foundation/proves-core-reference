telemetry packets ReferenceDeploymentPackets {

  packet Health id 1 group 1 {
    CdhCore.cmdDisp.CommandsDispatched
    CdhCore.cmdDisp.CommandsDropped
    ComCcsds.comQueue.comQueueDepth
    ComCcsds.commsBufferManager.HiBuffs
    ReferenceDeployment.rateGroup10Hz.RgMaxTime
    ReferenceDeployment.rateGroup1Hz.RgMaxTime
  }

  packet HealthWarnings id 2 group 1 {
    CdhCore.$health.PingLateWarnings
    ComCcsds.commsBufferManager.NoBuffs
    ComCcsds.commsBufferManager.EmptyBuffs
    ReferenceDeployment.rateGroup10Hz.RgCycleSlips
    ReferenceDeployment.rateGroup1Hz.RgCycleSlips
  }


  packet HealthAuxillary id 3 group 2 {
    ComCcsds.commsBufferManager.TotalBuffs
    ComCcsds.commsBufferManager.CurrBuffs
    ComCcsds.comQueue.buffQueueDepth
    CdhCore.tlmSend.SendLevel
  }

  packet Version id 4 group 3 {
    CdhCore.version.FrameworkVersion
    CdhCore.version.ProjectVersion
    CdhCore.version.LibraryVersion01
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
}
