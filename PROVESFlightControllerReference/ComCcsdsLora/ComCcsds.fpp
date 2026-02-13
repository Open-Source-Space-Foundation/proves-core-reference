module ComCcsdsLora {

    # ----------------------------------------------------------------------
    # Active Components
    # ----------------------------------------------------------------------
    instance comQueue: Svc.ComQueue base id ComCcsdsConfig.BASE_ID_LORA + 0x00000 \
        queue size ComCcsdsConfig.QueueSizes.comQueue \
        stack size ComCcsdsConfig.StackSizes.comQueue \
        priority ComCcsdsConfig.Priorities.comQueue \
    {
        phase Fpp.ToCpp.Phases.configComponents """
        using namespace ComCcsdsLora;
        Svc::ComQueue::QueueConfigurationTable configurationTableLora;

        // Events (highest-priority)
        configurationTableLora.entries[ComCcsds::Ports_ComPacketQueue::EVENTS].depth = ComCcsdsConfig::QueueDepths::events;
        configurationTableLora.entries[ComCcsds::Ports_ComPacketQueue::EVENTS].priority = ComCcsdsConfig::QueuePriorities::events;

        // Telemetry
        configurationTableLora.entries[ComCcsds::Ports_ComPacketQueue::TELEMETRY].depth = ComCcsdsConfig::QueueDepths::tlm;
        configurationTableLora.entries[ComCcsds::Ports_ComPacketQueue::TELEMETRY].priority = ComCcsdsConfig::QueuePriorities::tlm;

        // File Downlink Queue (buffer queue using NUM_CONSTANTS offset)
        configurationTableLora.entries[ComCcsds::Ports_ComPacketQueue::NUM_CONSTANTS + ComCcsds::Ports_ComBufferQueue::FILE].depth = ComCcsdsConfig::QueueDepths::file;
        configurationTableLora.entries[ComCcsds::Ports_ComPacketQueue::NUM_CONSTANTS + ComCcsds::Ports_ComBufferQueue::FILE].priority = ComCcsdsConfig::QueuePriorities::file;

        // Allocation identifier is 0 as the MallocAllocator discards it
        ComCcsdsLora::comQueue.configure(configurationTableLora, 0, ComCcsds::Allocation::memAllocator);
        """
        phase Fpp.ToCpp.Phases.tearDownComponents """
        ComCcsdsLora::comQueue.cleanup();
        """
    }

    # ----------------------------------------------------------------------
    # Passive Components
    # ----------------------------------------------------------------------
    instance frameAccumulator: Svc.FrameAccumulator base id ComCcsdsConfig.BASE_ID_LORA + 0x01000 \
    {

        phase Fpp.ToCpp.Phases.configObjects """
        Svc::FrameDetectors::CcsdsTcFrameDetector frameDetector;
        """
        phase Fpp.ToCpp.Phases.configComponents """
        ComCcsdsLora::frameAccumulator.configure(
            ConfigObjects::ComCcsdsLora_frameAccumulator::frameDetector,
            1,
            ComCcsds::Allocation::memAllocator,
            ComCcsdsConfig::BuffMgr::frameAccumulatorSize
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        ComCcsdsLora::frameAccumulator.cleanup();
        """
    }

    instance commsBufferManager: Svc.BufferManager base id ComCcsdsConfig.BASE_ID_LORA + 0x02000 \
    {
        phase Fpp.ToCpp.Phases.configObjects """
        Svc::BufferManager::BufferBins bins;
        """

        phase Fpp.ToCpp.Phases.configComponents """
        memset(&ConfigObjects::ComCcsdsLora_commsBufferManager::bins, 0, sizeof(ConfigObjects::ComCcsdsLora_commsBufferManager::bins));
        ConfigObjects::ComCcsdsLora_commsBufferManager::bins.bins[0].bufferSize = ComCcsdsConfig::BuffMgr::commsBuffSize;
        ConfigObjects::ComCcsdsLora_commsBufferManager::bins.bins[0].numBuffers = ComCcsdsConfig::BuffMgr::commsBuffCount;
        ConfigObjects::ComCcsdsLora_commsBufferManager::bins.bins[1].bufferSize = ComCcsdsConfig::BuffMgr::commsFileBuffSize;
        ConfigObjects::ComCcsdsLora_commsBufferManager::bins.bins[1].numBuffers = ComCcsdsConfig::BuffMgr::commsFileBuffCount;
        ComCcsdsLora::commsBufferManager.setup(
            ComCcsdsConfig::BuffMgr::commsBuffMgrId,
            0,
            ComCcsds::Allocation::memAllocator,
            ConfigObjects::ComCcsdsLora_commsBufferManager::bins
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        ComCcsdsLora::commsBufferManager.cleanup();
        """
    }

    instance authenticationRouter: Svc.AuthenticationRouter base id ComCcsdsConfig.BASE_ID_LORA + 0x03500

    instance tcDeframer: Svc.Ccsds.TcDeframer base id ComCcsdsConfig.BASE_ID_LORA + 0x04000

    instance spacePacketDeframer: Svc.Ccsds.SpacePacketDeframer base id ComCcsdsConfig.BASE_ID_LORA + 0x05000

    instance aggregator: Svc.ComAggregator base id ComCcsdsConfig.BASE_ID_LORA + 0x06000 \
        queue size ComCcsdsConfig.QueueSizes.aggregator \
        stack size ComCcsdsConfig.StackSizes.aggregator \
        priority ComCcsdsConfig.Priorities.aggregator

    # NOTE: name 'framer' is used for the framer that connects to the Com Adapter Interface for better subtopology interoperability
    instance framer: Svc.Ccsds.TmFramer base id ComCcsdsConfig.BASE_ID_LORA + 0x07000

    instance spacePacketFramer: Svc.Ccsds.SpacePacketFramer base id ComCcsdsConfig.BASE_ID_LORA + 0x08000

    instance apidManager: Svc.Ccsds.ApidManager base id ComCcsdsConfig.BASE_ID_LORA + 0x09000

    instance authenticatelora: Components.Authenticate base id ComCcsdsConfig.BASE_ID_LORA + 0x0B000

    topology Subtopology {
        # Usage Note:
        #
        # When importing this subtopology, users shall establish 5 port connections with a component implementing
        # the Svc.Com (Svc/Interfaces/Com.fpp) interface. They are as follows:
        #
        # 1) Outputs:
        #     - ComCcsdsLora.framer.dataOut                 -> [Svc.Com].dataIn
        #     - ComCcsdsLora.frameAccumulator.dataReturnOut -> [Svc.Com].dataReturnIn
        # 2) Inputs:
        #     - [Svc.Com].dataReturnOut -> ComCcsdsLora.framer.dataReturnIn
        #     - [Svc.Com].comStatusOut  -> ComCcsdsLora.framer.comStatusIn
        #     - [Svc.Com].dataOut       -> ComCcsdsLora.frameAccumulator.dataIn


        # Active Components
        instance comQueue

        # Passive Components
        instance commsBufferManager
        instance frameAccumulator
        instance authenticationRouter
        instance tcDeframer
        instance spacePacketDeframer
        instance framer
        instance spacePacketFramer
        instance apidManager
        instance aggregator
        instance authenticatelora

        connections Downlink {
            # ComQueue <-> SpacePacketFramer
            comQueue.dataOut                -> spacePacketFramer.dataIn
            spacePacketFramer.dataReturnOut -> comQueue.dataReturnIn
            # SpacePacketFramer buffer and APID management
            spacePacketFramer.bufferAllocate   -> commsBufferManager.bufferGetCallee
            spacePacketFramer.bufferDeallocate -> commsBufferManager.bufferSendIn
            spacePacketFramer.getApidSeqCount  -> apidManager.getApidSeqCountIn
            # SpacePacketFramer <-> TmFramer
            spacePacketFramer.dataOut -> aggregator.dataIn
            aggregator.dataOut        -> framer.dataIn

            framer.dataReturnOut      -> aggregator.dataReturnIn
            aggregator.dataReturnOut    -> spacePacketFramer.dataReturnIn

            # ComStatus
            framer.comStatusOut            -> aggregator.comStatusIn
            aggregator.comStatusOut        -> spacePacketFramer.comStatusIn
            spacePacketFramer.comStatusOut -> comQueue.comStatusIn
            # (Outgoing) Framer <-> ComInterface connections shall be established by the user
        }

        connections Uplink {
            # (Incoming) ComInterface <-> FrameAccumulator connections shall be established by the user
            # FrameAccumulator buffer allocations
            frameAccumulator.bufferDeallocate -> commsBufferManager.bufferSendIn
            frameAccumulator.bufferAllocate   -> commsBufferManager.bufferGetCallee
            # FrameAccumulator <-> TcDeframer
            frameAccumulator.dataOut -> tcDeframer.dataIn
            tcDeframer.dataReturnOut -> frameAccumulator.dataReturnIn
            # Authenticate <-> SpacePacketDeframer
            authenticatelora.dataOut -> spacePacketDeframer.dataIn
            spacePacketDeframer.dataReturnOut -> authenticatelora.dataReturnIn
            # TcDeframer <-> Authenticate
            tcDeframer.dataOut                -> authenticatelora.dataIn
            authenticatelora.dataReturnOut -> tcDeframer.dataReturnIn
            # SpacePacketDeframer APID validation
            spacePacketDeframer.validateApidSeqCount -> apidManager.validateApidSeqCountIn
            # SpacePacketDeframer <-> AuthenticationRouter (routes both commands and files)
            spacePacketDeframer.dataOut -> authenticationRouter.dataIn
            authenticationRouter.dataReturnOut  -> spacePacketDeframer.dataReturnIn
            # AuthenticationRouter buffer allocations
            authenticationRouter.bufferAllocate   -> commsBufferManager.bufferGetCallee
            authenticationRouter.bufferDeallocate -> commsBufferManager.bufferSendIn
        }
    } # end Subtopology

} # end ComCcsdsLora
