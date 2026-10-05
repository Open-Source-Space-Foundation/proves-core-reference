module CdhCore{

    # Uncomment the following block and comment the above block to use TlmPacketizer instead of TlmChan
    instance tlmSend: Svc.TlmPacketizer base id CdhCoreConfig.BASE_ID + 0x06000 \
       queue size CdhCoreConfig.QueueSizes.tlmSend \
       stack size CdhCoreConfig.StackSizes.tlmSend \
       priority CdhCoreConfig.Priorities.tlmSend \
    {
       # NOTE: Packet list name matches NiclaDeploymentPackets in NiclaDeployment.
       phase Fpp.ToCpp.Phases.configComponents """
       CdhCore::tlmSend.setPacketList(
           NiclaDeployment::NiclaDeployment_NiclaDeploymentPacketsTlmPackets::packetList,
           NiclaDeployment::NiclaDeployment_NiclaDeploymentPacketsTlmPackets::omittedChannels,
           1
       );
       """
    }
}
