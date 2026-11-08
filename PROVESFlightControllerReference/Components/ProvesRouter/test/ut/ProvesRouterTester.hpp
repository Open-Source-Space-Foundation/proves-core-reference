// ======================================================================
// \title  ProvesRouterTester.hpp
// \brief  hpp file for ProvesRouter component test harness implementation class
// ======================================================================

#ifndef Svc_ProvesRouterTester_HPP
#define Svc_ProvesRouterTester_HPP

#include "Fw/Test/UnitTest.hpp"
#include "PROVESFlightControllerReference/Components/ProvesRouter/ProvesRouter.hpp"
#include "PROVESFlightControllerReference/Components/ProvesRouter/ProvesRouterGTestBase.hpp"

namespace Svc {

class ProvesRouterTester final : public ProvesRouterGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Size of the packet and allocation buffers owned by the tester; larger
    // than a ComBuffer so a command packet can overflow it
    static constexpr FwSizeType BUFFER_CAPACITY = FW_COM_BUFFER_MAX_SIZE + 16;

    // Size of the test packets: descriptor (2) + opcode (4) + arguments (4)
    static constexpr FwSizeType PACKET_SIZE = 10;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object ProvesRouterTester
    ProvesRouterTester();

    //! Destroy object ProvesRouterTester
    ~ProvesRouterTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! An authenticated command packet goes out commandOut with the packet bytes
    void testAuthenticatedCommandIsRouted();

    //! An authenticated file packet goes out fileOut as a copy in a new buffer
    void testAuthenticatedFileIsRoutedAsCopy();

    //! An authenticated packet with an unknown APID goes out unknownDataOut as a copy
    void testAuthenticatedUnknownIsRoutedAsCopy();

    //! An unauthenticated command packet with a bypass opcode is routed and counted as bypassed
    void testUnauthenticatedBypassCommandIsRouted();

    //! An unauthenticated command packet with any other opcode is rejected
    void testUnauthenticatedCommandIsRejected();

    //! An unauthenticated command packet too short to hold an opcode is rejected
    void testUnauthenticatedShortCommandIsRejected();

    //! An unauthenticated file packet whose bytes 2-5 match a bypass opcode is rejected (#620)
    void testUnauthenticatedFileWithBypassOpCodeIsRejected();

    //! An unauthenticated unknown-APID packet whose bytes 2-5 match a bypass opcode is rejected (#620)
    void testUnauthenticatedUnknownWithBypassOpCodeIsRejected();

    //! A command packet larger than a ComBuffer emits SerializationError and is not routed
    void testOversizeCommandEmitsSerializationError();

    //! A failed allocation for a file packet emits AllocationError(FILE_UPLINK) and is not routed
    void testFileAllocationFailure();

    //! A failed allocation for an unknown packet emits AllocationError(USER_BUFFER) and is not routed
    void testUnknownAllocationFailure();

    //! An allocation smaller than the packet emits SerializationError and frees the allocation
    void testShortAllocationIsDeallocated();

    //! A buffer returned on fileBufferReturnIn is deallocated
    void testFileBufferReturnIsDeallocated();

    //! Routed, bypassed and rejected counts accumulate across packets
    void testCountersAccumulate();

  private:
    // ----------------------------------------------------------------------
    // Handler overrides for typed from ports
    // ----------------------------------------------------------------------

    //! Override for from_bufferAllocate: records history and serves m_allocStorage per m_allocMode
    Fw::Buffer from_bufferAllocate_handler(FwIndexType portNum,  //!< The port number
                                           FwSizeType size       //!< The requested size
                                           ) override;

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Fill m_packet with a packet of `size` bytes carrying `opCode` at bytes [2, 6)
    Fw::Buffer makePacket(U32 opCode, FwSizeType size);

    //! Build a frame context with the given APID and authentication flag
    static ComCfg::FrameContext makeContext(ComCfg::Apid::T apid, bool authenticated);

    //! Send `packet` with `context` on dataIn and check it came straight back on dataReturnOut
    void sendAndExpectReturn(Fw::Buffer& packet, const ComCfg::FrameContext& context);

    //! Check that the packet was rejected: counted, returned, and sent nowhere else
    void assertRejected(U32 rejectedCount);

    //! Check that a buffer holds exactly the first `size` bytes of m_packet
    void assertHoldsPacket(const U8* data, FwSizeType size) const;

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! How from_bufferAllocate answers a request
    enum class AllocMode {
        FULL,   //!< A buffer of the requested size
        SHORT,  //!< A valid buffer one byte smaller than requested
        FAIL,   //!< An invalid buffer
    };

    //! The component under test
    ProvesRouter component;

    //! Bytes of the packet sent on dataIn
    U8 m_packet[BUFFER_CAPACITY] = {};

    //! Bytes handed out by from_bufferAllocate
    U8 m_allocStorage[BUFFER_CAPACITY] = {};

    //! How the next from_bufferAllocate call answers
    AllocMode m_allocMode = AllocMode::FULL;
};

}  // namespace Svc

#endif
