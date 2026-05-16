module Components {
    @ FPP shadow-enum representing Components::PacketParser::Status
    enum PacketBypasserStatus {
        Ok,                        @< Packet was successfully parsed
        SpiParseError,             @< SPI could not be parsed from packet
        SequenceNumberParseError,  @< Sequence number could not be parsed from packet
        OpCodeParseError,          @< OpCode could not be parsed from packet
        HmacParseError,            @< HMAC could not be parsed from packet
    }

    @ Component placed between the TcDeframer component and the TcSecurityDeframer and the SpacePacketDeframer components. It checks incoming packets for whether they should bypass authentication based on their OpCode.
    passive component SecurityRouter {

        ### Telemetry ###

        @ Telemetry for the count of bypassed packets, updated on each packet that bypasses authentication based on OpCode
        telemetry BypassPacketsCount : U32

        ### Events ###

        @ ParsingFailed indicates that there was an error parsing a received packet
        event ParsingFailed() severity warning high id 3 format "Parsing failed" throttle 2

        ### Ports ###

        @ Port receiving Space Packets from TcDeframer
        guarded input port dataIn: Svc.ComDataWithContext

        @ Port receiving back ownership of buffers sent to dataOut (SpacePacketDeframer)
        sync input port dataReturnIn: Svc.ComDataWithContext

        @ Port to inform listeners that a packet has passed authentication (for command loss)
        output port packetAuthenticated: Fw.Signal

        @ Port forwarding packets that should bypass authentication to the SpacePacketDeframer
        output port bypassOut: Svc.ComDataWithContext

        @ Port forwarding packets that require authentication to the TcSecurityDeframer
        output port authenticateOut: Svc.ComDataWithContext

        @ Port returning ownership of buffers sent to either output port back to the TcDeframer
        output port dataReturnOut: Svc.ComDataWithContext

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

    }
}
