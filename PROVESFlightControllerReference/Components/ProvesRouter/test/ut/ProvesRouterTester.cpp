// ======================================================================
// \title  ProvesRouterTester.cpp
// \brief  cpp file for ProvesRouter component test harness implementation class
// ======================================================================

#include "ProvesRouterTester.hpp"

namespace Svc {

namespace {

//! CdhCore.cmdDisp.CMD_NO_OP, on the bypass allowlist
constexpr U32 kBypassOpCode = 0x01000000;

//! An opcode that is not on the bypass allowlist
constexpr U32 kOtherOpCode = 0x01000001;

}  // namespace

// gtest binds these by reference, so they need definitions before C++17
constexpr FwSizeType ProvesRouterTester::BUFFER_CAPACITY;
constexpr FwSizeType ProvesRouterTester::PACKET_SIZE;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

ProvesRouterTester ::ProvesRouterTester()
    : ProvesRouterGTestBase("ProvesRouterTester", ProvesRouterTester::MAX_HISTORY_SIZE), component("ProvesRouter") {
    this->initComponents();
    this->connectPorts();
}

ProvesRouterTester ::~ProvesRouterTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void ProvesRouterTester ::testAuthenticatedCommandIsRouted() {
    REQUIREMENT("SVC-ROUTER-001");
    REQUIREMENT("SVC-ROUTER-002");
    REQUIREMENT("SVC-ROUTER-008");
    REQUIREMENT("SVC-ROUTER-009");
    Fw::Buffer packet = this->makePacket(kOtherOpCode, PACKET_SIZE);
    this->sendAndExpectReturn(packet, makeContext(ComCfg::Apid::FW_PACKET_COMMAND, true));

    // The packet bytes go out commandOut as a ComBuffer
    ASSERT_from_commandOut_SIZE(1);
    const Fw::ComBuffer& com = this->fromPortHistory_commandOut->at(0).data;
    this->assertHoldsPacket(com.getBuffAddr(), com.getSize());
    ASSERT_EQ(this->fromPortHistory_commandOut->at(0).context, 0U);

    // Routed once, signalled once, nothing allocated or sent elsewhere
    ASSERT_from_packetRouted_SIZE(1);
    ASSERT_TLM_SIZE(1);
    ASSERT_TLM_RoutedPackets(0, 1);
    ASSERT_from_bufferAllocate_SIZE(0);
    ASSERT_from_fileOut_SIZE(0);
    ASSERT_from_unknownDataOut_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void ProvesRouterTester ::testAuthenticatedFileIsRoutedAsCopy() {
    REQUIREMENT("SVC-ROUTER-003");
    REQUIREMENT("SVC-ROUTER-007");
    REQUIREMENT("SVC-ROUTER-008");
    Fw::Buffer packet = this->makePacket(kOtherOpCode, PACKET_SIZE);
    this->sendAndExpectReturn(packet, makeContext(ComCfg::Apid::FW_PACKET_FILE, true));

    // A buffer of the packet's size is allocated and the copy goes out fileOut
    ASSERT_from_bufferAllocate_SIZE(1);
    ASSERT_from_bufferAllocate(0, PACKET_SIZE);
    ASSERT_from_fileOut_SIZE(1);
    const Fw::Buffer& copy = this->fromPortHistory_fileOut->at(0).fwBuffer;
    ASSERT_EQ(copy.getData(), this->m_allocStorage);
    this->assertHoldsPacket(copy.getData(), copy.getSize());

    ASSERT_from_packetRouted_SIZE(1);
    ASSERT_TLM_SIZE(1);
    ASSERT_TLM_RoutedPackets(0, 1);
    ASSERT_from_commandOut_SIZE(0);
    ASSERT_from_unknownDataOut_SIZE(0);
    ASSERT_from_bufferDeallocate_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void ProvesRouterTester ::testAuthenticatedUnknownIsRoutedAsCopy() {
    REQUIREMENT("SVC-ROUTER-004");
    REQUIREMENT("SVC-ROUTER-007");
    REQUIREMENT("SVC-ROUTER-008");
    Fw::Buffer packet = this->makePacket(kOtherOpCode, PACKET_SIZE);
    const ComCfg::FrameContext context = makeContext(ComCfg::Apid::FW_PACKET_UNKNOWN, true);
    this->sendAndExpectReturn(packet, context);

    // The copy goes out unknownDataOut with the original context
    ASSERT_from_bufferAllocate_SIZE(1);
    ASSERT_from_bufferAllocate(0, PACKET_SIZE);
    ASSERT_from_unknownDataOut_SIZE(1);
    const Fw::Buffer& copy = this->fromPortHistory_unknownDataOut->at(0).data;
    ASSERT_EQ(copy.getData(), this->m_allocStorage);
    this->assertHoldsPacket(copy.getData(), copy.getSize());
    ASSERT_EQ(this->fromPortHistory_unknownDataOut->at(0).context, context);

    ASSERT_from_packetRouted_SIZE(1);
    ASSERT_TLM_SIZE(1);
    ASSERT_TLM_RoutedPackets(0, 1);
    ASSERT_from_commandOut_SIZE(0);
    ASSERT_from_fileOut_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void ProvesRouterTester ::testUnauthenticatedBypassCommandIsRouted() {
    REQUIREMENT("SVC-ROUTER-010");
    REQUIREMENT("SVC-ROUTER-011");
    Fw::Buffer packet = this->makePacket(kBypassOpCode, PACKET_SIZE);
    this->sendAndExpectReturn(packet, makeContext(ComCfg::Apid::FW_PACKET_COMMAND, false));

    ASSERT_from_commandOut_SIZE(1);
    const Fw::ComBuffer& com = this->fromPortHistory_commandOut->at(0).data;
    this->assertHoldsPacket(com.getBuffAddr(), com.getSize());
    ASSERT_from_packetRouted_SIZE(1);

    // Counted as bypassed, then as routed
    ASSERT_TLM_SIZE(2);
    ASSERT_TLM_BypassedPackets_SIZE(1);
    ASSERT_TLM_BypassedPackets(0, 1);
    ASSERT_TLM_RoutedPackets_SIZE(1);
    ASSERT_TLM_RoutedPackets(0, 1);
    ASSERT_TLM_RejectedPackets_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void ProvesRouterTester ::testUnauthenticatedCommandIsRejected() {
    REQUIREMENT("SVC-ROUTER-008");
    REQUIREMENT("SVC-ROUTER-010");
    REQUIREMENT("SVC-ROUTER-011");
    Fw::Buffer packet = this->makePacket(kOtherOpCode, PACKET_SIZE);
    this->sendAndExpectReturn(packet, makeContext(ComCfg::Apid::FW_PACKET_COMMAND, false));
    this->assertRejected(1);
}

void ProvesRouterTester ::testUnauthenticatedShortCommandIsRejected() {
    REQUIREMENT("SVC-ROUTER-010");
    // Five bytes cut the opcode at bytes [2, 6) short by one
    Fw::Buffer packet = this->makePacket(kBypassOpCode, 5);
    this->sendAndExpectReturn(packet, makeContext(ComCfg::Apid::FW_PACKET_COMMAND, false));
    this->assertRejected(1);
}

void ProvesRouterTester ::testUnauthenticatedFileWithBypassOpCodeIsRejected() {
    REQUIREMENT("SVC-ROUTER-010");
    Fw::Buffer packet = this->makePacket(kBypassOpCode, PACKET_SIZE);
    this->sendAndExpectReturn(packet, makeContext(ComCfg::Apid::FW_PACKET_FILE, false));
    this->assertRejected(1);
}

void ProvesRouterTester ::testUnauthenticatedUnknownWithBypassOpCodeIsRejected() {
    REQUIREMENT("SVC-ROUTER-010");
    Fw::Buffer packet = this->makePacket(kBypassOpCode, PACKET_SIZE);
    this->sendAndExpectReturn(packet, makeContext(ComCfg::Apid::FW_PACKET_UNKNOWN, false));
    this->assertRejected(1);
}

void ProvesRouterTester ::testOversizeCommandEmitsSerializationError() {
    REQUIREMENT("SVC-ROUTER-005");
    REQUIREMENT("SVC-ROUTER-008");
    Fw::Buffer packet = this->makePacket(kOtherOpCode, FW_COM_BUFFER_MAX_SIZE + 1);
    this->sendAndExpectReturn(packet, makeContext(ComCfg::Apid::FW_PACKET_COMMAND, true));

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_SerializationError_SIZE(1);
    ASSERT_EVENTS_SerializationError(0, Fw::FW_SERIALIZE_NO_ROOM_LEFT);

    // Nothing is routed or counted
    ASSERT_from_commandOut_SIZE(0);
    ASSERT_from_packetRouted_SIZE(0);
    ASSERT_TLM_SIZE(0);
}

void ProvesRouterTester ::testFileAllocationFailure() {
    REQUIREMENT("SVC-ROUTER-006");
    REQUIREMENT("SVC-ROUTER-008");
    this->m_allocMode = AllocMode::FAIL;
    Fw::Buffer packet = this->makePacket(kOtherOpCode, PACKET_SIZE);
    this->sendAndExpectReturn(packet, makeContext(ComCfg::Apid::FW_PACKET_FILE, true));

    ASSERT_from_bufferAllocate_SIZE(1);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_AllocationError_SIZE(1);
    ASSERT_EVENTS_AllocationError(0, ProvesRouter_AllocationReason::FILE_UPLINK);

    ASSERT_from_fileOut_SIZE(0);
    ASSERT_from_bufferDeallocate_SIZE(0);
    ASSERT_from_packetRouted_SIZE(0);
    ASSERT_TLM_SIZE(0);
}

void ProvesRouterTester ::testUnknownAllocationFailure() {
    REQUIREMENT("SVC-ROUTER-006");
    REQUIREMENT("SVC-ROUTER-008");
    this->m_allocMode = AllocMode::FAIL;
    Fw::Buffer packet = this->makePacket(kOtherOpCode, PACKET_SIZE);
    this->sendAndExpectReturn(packet, makeContext(ComCfg::Apid::FW_PACKET_UNKNOWN, true));

    ASSERT_from_bufferAllocate_SIZE(1);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_AllocationError_SIZE(1);
    ASSERT_EVENTS_AllocationError(0, ProvesRouter_AllocationReason::USER_BUFFER);

    ASSERT_from_unknownDataOut_SIZE(0);
    ASSERT_from_bufferDeallocate_SIZE(0);
    ASSERT_from_packetRouted_SIZE(0);
    ASSERT_TLM_SIZE(0);
}

void ProvesRouterTester ::testShortAllocationIsDeallocated() {
    this->m_allocMode = AllocMode::SHORT;
    Fw::Buffer packet = this->makePacket(kOtherOpCode, PACKET_SIZE);
    this->sendAndExpectReturn(packet, makeContext(ComCfg::Apid::FW_PACKET_FILE, true));

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_SerializationError_SIZE(1);
    ASSERT_EVENTS_SerializationError(0, Fw::FW_SERIALIZE_NO_ROOM_LEFT);

    // The allocation is handed back, and nothing goes out fileOut
    ASSERT_from_bufferDeallocate_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_bufferDeallocate->at(0).fwBuffer.getData(), this->m_allocStorage);
    ASSERT_from_fileOut_SIZE(0);
    ASSERT_from_packetRouted_SIZE(0);
    ASSERT_TLM_SIZE(0);
}

void ProvesRouterTester ::testFileBufferReturnIsDeallocated() {
    Fw::Buffer returned(this->m_allocStorage, PACKET_SIZE);
    this->invoke_to_fileBufferReturnIn(0, returned);

    ASSERT_from_bufferDeallocate_SIZE(1);
    ASSERT_from_bufferDeallocate(0, returned);
    ASSERT_FROM_PORT_HISTORY_SIZE(1);
    ASSERT_EVENTS_SIZE(0);
    ASSERT_TLM_SIZE(0);
}

void ProvesRouterTester ::testCountersAccumulate() {
    REQUIREMENT("SVC-ROUTER-011");
    Fw::Buffer packet = this->makePacket(kBypassOpCode, PACKET_SIZE);

    // Authenticated, bypassed, rejected (bypass opcode on a file APID), authenticated
    this->invoke_to_dataIn(0, packet, makeContext(ComCfg::Apid::FW_PACKET_COMMAND, true));
    this->invoke_to_dataIn(0, packet, makeContext(ComCfg::Apid::FW_PACKET_COMMAND, false));
    this->invoke_to_dataIn(0, packet, makeContext(ComCfg::Apid::FW_PACKET_FILE, false));
    this->invoke_to_dataIn(0, packet, makeContext(ComCfg::Apid::FW_PACKET_COMMAND, true));

    ASSERT_from_dataReturnOut_SIZE(4);
    ASSERT_from_commandOut_SIZE(3);
    ASSERT_from_packetRouted_SIZE(3);
    ASSERT_TLM_RoutedPackets_SIZE(3);
    ASSERT_TLM_RoutedPackets(0, 1);
    ASSERT_TLM_RoutedPackets(1, 2);
    ASSERT_TLM_RoutedPackets(2, 3);
    ASSERT_TLM_BypassedPackets_SIZE(1);
    ASSERT_TLM_BypassedPackets(0, 1);
    ASSERT_TLM_RejectedPackets_SIZE(1);
    ASSERT_TLM_RejectedPackets(0, 1);
}

// ----------------------------------------------------------------------
// Handler overrides for typed from ports
// ----------------------------------------------------------------------

Fw::Buffer ProvesRouterTester ::from_bufferAllocate_handler(FwIndexType portNum, FwSizeType size) {
    this->pushFromPortEntry_bufferAllocate(size);
    EXPECT_LE(size, BUFFER_CAPACITY);
    switch (this->m_allocMode) {
        case AllocMode::FULL:
            return Fw::Buffer(this->m_allocStorage, size);
        case AllocMode::SHORT:
            return Fw::Buffer(this->m_allocStorage, size - 1);
        case AllocMode::FAIL:
            return Fw::Buffer();
    }
    return Fw::Buffer();
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

Fw::Buffer ProvesRouterTester ::makePacket(U32 opCode, FwSizeType size) {
    EXPECT_LE(size, BUFFER_CAPACITY);
    for (FwSizeType i = 0; i < BUFFER_CAPACITY; i++) {
        this->m_packet[i] = static_cast<U8>(0xA0 + (i % 16));
    }
    // Command descriptor at bytes [0, 2), big-endian opcode at bytes [2, 6)
    this->m_packet[0] = 0x00;
    this->m_packet[1] = 0x00;
    this->m_packet[2] = static_cast<U8>(opCode >> 24);
    this->m_packet[3] = static_cast<U8>(opCode >> 16);
    this->m_packet[4] = static_cast<U8>(opCode >> 8);
    this->m_packet[5] = static_cast<U8>(opCode);
    return Fw::Buffer(this->m_packet, size);
}

ComCfg::FrameContext ProvesRouterTester ::makeContext(ComCfg::Apid::T apid, bool authenticated) {
    ComCfg::FrameContext context;
    context.set_apid(apid);
    context.set_authenticated(authenticated);
    return context;
}

void ProvesRouterTester ::sendAndExpectReturn(Fw::Buffer& packet, const ComCfg::FrameContext& context) {
    this->invoke_to_dataIn(0, packet, context);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_from_dataReturnOut(0, packet, context);
}

void ProvesRouterTester ::assertRejected(U32 rejectedCount) {
    ASSERT_TLM_SIZE(1);
    ASSERT_TLM_RejectedPackets_SIZE(1);
    ASSERT_TLM_RejectedPackets(0, rejectedCount);

    // Only the buffer return goes out: no routing, no allocation, no signal
    ASSERT_FROM_PORT_HISTORY_SIZE(1);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_EVENTS_SIZE(0);
}

void ProvesRouterTester ::assertHoldsPacket(const U8* data, FwSizeType size) const {
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(size, PACKET_SIZE);
    for (FwSizeType i = 0; i < size; i++) {
        ASSERT_EQ(data[i], this->m_packet[i]) << "byte " << i;
    }
}

}  // namespace Svc
