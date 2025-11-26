module Svc {
    @ Routes packets deframed by the Deframer to the rest of the system
    active component AuthenticationRouter {

        # ----------------------------------------------------------------------
        # Router interface (ports defined explicitly for active component)
        # ----------------------------------------------------------------------
        # Port receiving calls from the rate group
        async input port schedIn: Svc.Sched

        # Receiving data (Fw::Buffer) to be routed with optional context to help with routing
        async input port dataIn: Svc.ComDataWithContext

        # Port for returning ownership of data (includes Fw.Buffer) received on dataIn
        output port dataReturnOut: Svc.ComDataWithContext

        # Port for sending file packets as Fw::Buffer (ownership passed to receiver)
        output port fileOut: Fw.BufferSend

        # Port for receiving ownership back of buffers sent on fileOut
        sync input port fileBufferReturnIn: Fw.BufferSend

        # Port for sending command packets as Fw::ComBuffers
        output port commandOut: Fw.Com

        # Port for receiving command responses from a command dispatcher (can be a no-op)
        sync input port cmdResponseIn: Fw.CmdResponse

        @ Port for forwarding non-recognized packet types
        @ Ownership of the buffer is retained by the AuthenticationRouter, meaning receiving
        @ components should either process data synchronously, or copy the data if needed
        output port unknownDataOut: Svc.ComDataWithContext

        @ Port for allocating buffers
        output port bufferAllocate: Fw.BufferGet

        @ Port for deallocating buffers
        output port bufferDeallocate: Fw.BufferSend

        @ An error occurred while serializing a com buffer
        event SerializationError(
                status: U32 @< The status of the operation
            ) \
            severity warning high \
            format "Serializing com buffer failed with status {}"

        @ An error occurred while deserializing a packet
        event DeserializationError(
                status: U32 @< The status of the operation
            ) \
            severity warning high \
            format "Deserializing packet type failed with status {}"


        ###############################################################################
        # Standard AC Ports for Events
        ###############################################################################
        @ Command receive port
        command recv port cmdIn

        @ Command registration port
        command reg port cmdRegOut

        @ Command response port
        command resp port cmdResponseOut

        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Telemetry port
        telemetry port tlmOut

        @ Parameter get port
        param get port prmGetOut

        @ Parameter set port
        param set port prmSetOut

        ################################################
        # Custom For This Router
        #################################

        @ Port for sending signal to safemode
        output port SafeModeOn: Fw.Signal

        @ event to emit when command loss time expires
        event CommandLossTimeExpired(safemode: Fw.On) \
            severity activity high \
            format "SafeModeOn: {}"

        @ Emits Current Loss Time
        event CurrentLossTime(loss_max_time: U32) \
            severity activity low \
            format "Current Loss Time: {}"

        @ Telemetry Channel to commit the time of the last command packet
        telemetry LastCommandPacketTime : U64

        @ loss time max parameter
        param LOSS_MAX_TIME : U32 default 10


    }
}
