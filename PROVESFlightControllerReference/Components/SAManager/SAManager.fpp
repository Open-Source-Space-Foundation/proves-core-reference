module Svc {
module Ccsds {
    @ Security Association Manager. Implements the Ccsds.ProcessSecurity port
    @ for TC frames. Owns the SA table and performs SPI lookup + anti-replay
    @ checks per CCSDS 355.0-B-2 §3.3, delegating header parsing and MAC
    @ verification to a project-supplied Svc::Ccsds::ISecurityProvider
    @ (a pure C++ interface declared in ISecurityProvider.hpp).
    @
    @ Keeping crypto behind ISecurityProvider lets upstream F Prime stay free of
    @ any specific crypto library dependency. SAManager itself ships zero crypto.
    passive component SAManager {

        @ Synchronous ProcessSecurity entry point. Typically connected from
        @ Svc.Ccsds.TcSecurityDeframer.processSecurityOut.
        guarded input port processSecurityIn: Ccsds.ProcessSecurity

        ###############################################################################
        # Telemetry                                                                   #
        ###############################################################################

        @ Number of Security Associations currently registered
        telemetry SACount: U16 update on change

        @ Total number of frames accepted across all SAs
        telemetry AcceptedTotal: U32 update on change

        @ Total number of frames rejected across all SAs
        telemetry RejectedTotal: U32 update on change

        ###############################################################################
        # Events                                                                      #
        ###############################################################################

        @ A frame referenced an SPI that is not registered in the SA table
        event SaLookupFailed(spi: U32) \
            severity warning low \
            format "No SA found for SPI={}" \
            throttle 5

        @ Anti-replay window rejected a frame
        event AntiReplayReject(spi: U32, received: U32, lastAccepted: U32) \
            severity warning low \
            format "Anti-replay reject: SPI={} received={} lastAccepted={}" \
            throttle 5

        @ ISecurityProvider returned an error code (parseHeader or verifyMac)
        event ProviderError(spi: U32, code: I32) \
            severity warning high \
            format "ISecurityProvider returned error for SPI={} code={}" \
            throttle 5

        @ An SA was registered at runtime via registerSa()
        event SaRegistered(spi: U32, scid: U16, vcid: U8) \
            severity activity low \
            format "SA registered: SPI={} SCID={} VCID={}"

        @ SA table is full; registration was rejected
        event SaTableFull(spi: U32) \
            severity warning high \
            format "Cannot register SA SPI={}: table full"

        ###############################################################################
        # Standard AC Ports                                                           #
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
}
