module CdhCore{

    # Uncomment the following block and comment the above block to use TlmPacketizer instead of TlmChan
    instance tlmSend: Svc.TlmPacketizer base id CdhCoreConfig.BASE_ID + 0x06000 \
       queue size CdhCoreConfig.QueueSizes.tlmSend \
       stack size CdhCoreConfig.StackSizes.tlmSend \
       priority CdhCoreConfig.Priorities.tlmSend \
    {
       # NOTE: The Name Ref is specific to the Reference deployment, Ref
       # This name will need to be updated if wishing to use this in a custom deployment
       # The packet list is installed in the initComponents phase, right after tlmSend.init(), rather
       # than in configComponents: any component whose init() writes telemetry (e.g. an FPP state
       # machine announcing its initial state) would otherwise hit TlmPacketizer's FW_ASSERT(m_configured).
       # initComponents runs in instance order, and CdhCore::tlmSend precedes every other telemetering instance.
       phase Fpp.ToCpp.Phases.initComponents """
       CdhCore::tlmSend.init(QueueSizes::CdhCore_tlmSend, InstanceIds::CdhCore_tlmSend);
       CdhCore::tlmSend.setPacketList(
           ReferenceDeployment::ReferenceDeployment_ReferenceDeploymentPacketsTlmPackets::packetList,
           ReferenceDeployment::ReferenceDeployment_ReferenceDeploymentPacketsTlmPackets::omittedChannels,
           1
       );
       """
    }
}
