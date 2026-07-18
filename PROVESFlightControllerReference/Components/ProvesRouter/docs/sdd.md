# Svc::ProvesRouter

The `Svc::ProvesRouter` component routes F´ packets (such as command or file packets) to other components. It is based on the FPrime Router, explained and linked later in the sdd, with one distinction:

This component reads the packet type from the `ComCfg::FrameContext` APID field (via `context.get_apid()`) rather than deserializing the type from the packet buffer header. After routing each packet, the component emits a `packetRouted` signal so that any interested components (such as `ModeManager`) can react to uplink activity.

The `Svc::ProvesRouter` component receives F´ packets (as Fw::Buffer objects) and routes them to other components through synchronous port calls. The input port of type `Svc.ComDataWithContext` passes this Fw.Buffer object along with optional context data which can help for routing. The current F Prime protocol does not use this context data, but is nevertheless present in the interface for compatibility with other protocols which may for example pass APIDs in the frame headers.

The `Svc::ProvesRouter` component supports `Fw::ComPacketType::FW_PACKET_COMMAND` and `Fw::ComPacketType::FW_PACKET_FILE` packet types. Unknown packet types are forwarded on the `unknownDataOut` port, which a project-specific component can connect to for custom routing.

## Security Policy Enforcement

`Svc::ProvesRouter` is the policy point of the uplink security design. Upstream, `Components::TcSecurityDeframer` verifies each frame (SPI, anti-replay sequence number, MAC) and records the result in the `ComCfg::FrameContext` `authenticated` flag without dropping anything. The router then enforces:

- Packets with `authenticated == true` are routed normally.
- Unauthenticated packets are routed only if their opcode is on the hardcoded bypass allowlist (`Components::PacketBypasser::bypassPacket` in `Bypasser.cpp`), which permits public commands such as `CMD_NO_OP`, `GET_SEQ_NUM`, and `TELL_JOKE`.
- All other unauthenticated packets are rejected: ownership is returned via `dataReturnOut` and the packet is not routed.

Because bypassed packets never pass through the authenticated-accept path in TcSecurityDeframer, they cannot advance the anti-replay sequence number.

About memory management, all buffers sent by `Svc::ProvesRouter` on the `fileOut` and `unknownDataOut` ports are expected to be returned to the router through the `fileBufferReturnIn` port for deallocation.

## Custom Routing

The `Svc::ProvesRouter` component is designed to be extensible through the use of a project-specific router. The `unknownDataOut` port can be connected to a project-specific component that can receive all unknown packet types. This component can then implement custom handling of these unknown packets. After processing, the project-specific component shall return the received buffer to the `Svc::ProvesRouter` component through the `fileBufferReturnIn` port (named this way as it only receives file packets in the common use-case), which will deallocate the buffer.

## Usage Examples

The `Svc::FprimeRouter` component is used in the uplink stack of many reference F´ application such as [the tutorials source code](https://github.com/fprime-community#tutorials).

### Typical Usage

In the canonical uplink communications stack, `Svc::FprimeRouter` is connected to Svc::CmdDispatcher and Svc::FileUplink components, to receive Command and File packets respectively. See the [F Prime documentation](https://nasa.github.io/fprime/) for more details on these components.

## Port Descriptions

| Kind | Name | Type | Description |
|---|---|---|---|
| `sync input` | `dataIn` | `Svc.ComDataWithContext` | Receiving Fw::Buffer with context buffer from Deframer |
| `output` | `dataReturnOut` | `Svc.ComDataWithContext` | Returning ownership of buffer received on `dataIn` |
| `output` | `commandOut` | `Fw.Com` | Port for sending command packets as Fw::ComBuffers |
| `output` | `fileOut` | `Fw.BufferSend` | Port for sending file packets as Fw::Buffer (ownership passed to receiver) |
| `sync input` | `fileBufferReturnIn` | `Fw.BufferSend` | Receiving back ownership of buffer sent on `fileOut` and `unknownDataOut` |
| `sync input` | `cmdResponseIn` | `Fw.CmdResponse` | Port for receiving command responses from a command dispatcher (can be a no-op) |
| `output` | `unknownDataOut` | `Svc.ComDataWithContext` | Port forwarding unknown data (useful for adding custom routing rules with a project-defined router) |
| `output` | `bufferAllocate` | `Fw.BufferGet` | Port for allocating buffers, allowing copy of received data |
| `output` | `bufferDeallocate` | `Fw.BufferSend` | Port for deallocating buffers |
| `output` | `packetRouted` | `Fw.Signal` | Emitted after each received packet is processed; used to reset command loss timer in ModeManager |

## Requirements

| Name | Description | Rationale | Validation |
| ---- | ----------- | --------- | ---------- |
SVC-ROUTER-001 | `Svc::ProvesRouter` shall route packets based on their packet type as read from the `ComCfg::FrameContext` APID field (`context.get_apid()`) | Routing mechanism of the F´ comms protocol | Unit test |
SVC-ROUTER-002 | `Svc::ProvesRouter` shall route packets of type `Fw::ComPacketType::FW_PACKET_COMMAND` to the `commandOut` output port. | Routing command packets | Unit test |
SVC-ROUTER-003 | `Svc::ProvesRouter` shall route packets of type `Fw::ComPacketType::FW_PACKET_FILE` to the `fileOut` output port. | Routing file packets | Unit test |
SVC-ROUTER-004 | `Svc::ProvesRouter` shall route data that is neither `Fw::ComPacketType::FW_PACKET_COMMAND` nor `Fw::ComPacketType::FW_PACKET_FILE` to the `unknownDataOut` output port. | Allows for projects to provide custom routing for additional (project-specific) uplink data types | Unit test |
SVC-ROUTER-005 | `Svc::ProvesRouter` shall emit a `SerializationError` warning event if copying a command packet into a `Fw::ComBuffer` fails | Aid in diagnosing uplink issues | Unit test |
SVC-ROUTER-006 | `Svc::ProvesRouter` shall emit an `AllocationError` warning event and skip forwarding if buffer allocation fails for a file or unknown packet | Memory management safety | Unit test |
SVC-ROUTER-007 | `Svc::ProvesRouter` shall make a copy of buffers that represent a `FW_PACKET_FILE` or unknown packet type before forwarding, so that the original buffer can be returned immediately via `dataReturnOut` | Memory management | Unit test |
SVC-ROUTER-008 | `Svc::ProvesRouter` shall return ownership of all buffers received on `dataIn` through `dataReturnOut` | Memory management | Unit test |
SVC-ROUTER-009 | `Svc::ProvesRouter` shall emit the `packetRouted` signal after processing each received packet | Allows interested components (e.g., `ModeManager`) to track uplink activity | Unit test |
SVC-ROUTER-010 | `Svc::ProvesRouter` shall reject packets whose frame context is not marked authenticated unless their opcode is on the bypass allowlist | Enforces uplink security policy at the routing edge | Integration test |
SVC-ROUTER-011 | `Svc::ProvesRouter` shall telemeter counts of routed, bypassed, and rejected packets | Operator visibility into uplink security decisions | Inspection |

## Telemetry Channels

| Name | Type | Description |
|---|---|---|
| RoutedPackets | U32 | Count of packets routed (authenticated or bypassed) |
| BypassedPackets | U32 | Count of unauthenticated packets routed via the opcode bypass allowlist |
| RejectedPackets | U32 | Count of unauthenticated packets rejected |

## Events

| Name | Severity | Parameters | Description |
|---|---|---|---|
| SerializationError | Warning High | status: U32 | Emitted when copying a command packet into a com buffer fails (`com.setBuff`) |
| AllocationError | Warning High | reason: AllocationReason | Emitted when buffer allocation fails for file uplink (`FILE_UPLINK`) or unknown packet (`USER_BUFFER`) |
