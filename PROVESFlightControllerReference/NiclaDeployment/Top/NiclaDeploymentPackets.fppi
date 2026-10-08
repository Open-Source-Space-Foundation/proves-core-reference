telemetry packets NiclaDeploymentPackets {

  packet Health id 1 group 1 {
cmdDisp.CommandsDispatched
cmdDisp.CommandsDropped
    ComCcsdsNicla.comQueue.comQueueDepth
    ComCcsdsNicla.commsBufferManager.HiBuffs
rateGroup1Hz.RgMaxTime
watchdog.WatchdogTransitions
  }

  packet HealthWarnings id 2 group 1 {
$health.PingLateWarnings
    ComCcsdsNicla.commsBufferManager.NoBuffs
    ComCcsdsNicla.commsBufferManager.EmptyBuffs
rateGroup1Hz.RgCycleSlips
  }


  packet HealthAuxillary id 3 group 2 {
    ComCcsdsNicla.commsBufferManager.TotalBuffs
    ComCcsdsNicla.commsBufferManager.CurrBuffs
    ComCcsdsNicla.comQueue.buffQueueDepth
tlmSend.SectionEnabled
  }

  packet Version id 4 group 3 {
version.FrameworkVersion
version.ProjectVersion
version.LibraryVersion01
  }


} omit {
  tlmSend.GroupConfigs
cmdDisp.CommandErrors
  # Only has one library, no custom versions
version.LibraryVersion02
version.LibraryVersion03
version.LibraryVersion04
version.LibraryVersion05
version.LibraryVersion06
version.LibraryVersion07
version.LibraryVersion08
version.LibraryVersion09
version.LibraryVersion10
version.CustomVersion01
version.CustomVersion02
version.CustomVersion03
version.CustomVersion04
version.CustomVersion05
version.CustomVersion06
version.CustomVersion07
version.CustomVersion08
version.CustomVersion09
version.CustomVersion10
  events.EventsDropped
}
