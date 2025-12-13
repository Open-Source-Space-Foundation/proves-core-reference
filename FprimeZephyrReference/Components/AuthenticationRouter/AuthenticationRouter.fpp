module Svc {
    @ Routes packets deframed by the Deframer to the rest of the system
    passive component AuthenticationRouter {

        enum AllocationReason : U8{
            FILE_UPLINK,  @< Buffer allocation for file uplink
            USER_BUFFER   @< Buffer allocation for user handled buffer
        }

        # ----------------------------------------------------------------------
        # Router interface (ports defined explicitly for passive component)
        # ----------------------------------------------------------------------
        # Receiving data (Fw::Buffer) to be routed with optional context to help with routing
        sync input port dataIn: Svc.ComDataWithContext

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


        @ An allocation error occurred
        event AllocationError(reason: AllocationReason) severity warning high \
            format "Buffer allocation for {} failed"

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

        @ Emitted to indicate whether a packet passed through the router
        event PassedRouter(passed: bool) \
            severity activity low \
            format "PassedRouter: {}" \
            throttle 2


        @ Emitted to indicate whether authentication was bypassed
        event BypassedAuthentification() \
            severity activity low \
            format "OpCode BypassedAuthentification" \
            throttle 2


        event FileOpenError(openStatus: U8) \
            severity warning high \
            format "File Open Error {} for BypassOpCodes file. No Opcodes will be Bypassed" \
            throttle 2


        @ Telemetry Channel for Number of Packets that have bypassed the router
        telemetry ByPassedRouter : U64

        @ Telemetry Channel for Number of Packets that have passed the router
        telemetry PassedRouter : U64

        @ Telemetry Channel for Number of Packets that have not passed the Router
        telemetry FailedRouter : U64

    }
}
