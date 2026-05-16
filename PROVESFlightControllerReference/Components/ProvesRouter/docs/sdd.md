# Svc::ProvesRouter

The `Svc::ProvesRouter` component routes F´ packets (such as command or file packets) to other components. It is based on the FPrime Router, explained and linked later in the sdd, with one exception:

This component handles authenticated packets. It will check a list of preconfigured opcodes that do not need to be authenticated, will pass those on as well as the things that are authenticated.

After routing a packet, the component emits a `packetRouted` signal so that any interested components (such as `ModeManager`) can react to uplink activity.

The `Svc::ProvesRouter` component receives F´ packets (as Fw::Buffer objects) and routes them to other components through synchronous port calls. The input port of type `Svc.ComDataWithContext` passes this Fw.Buffer object along with optional context data which can help for routing. The current F Prime protocol does not use this context data, but is nevertheless present in the interface for compatibility with other protocols which may for example pass APIDs in the frame headers.

The `Svc::ProvesRouter` component supports `Fw::ComPacketType::FW_PACKET_COMMAND` and `Fw::ComPacketType::FW_PACKET_FILE` packet types. Unknown packet types are forwarded on the `unknownDataOut` port, which a project-specific component can connect to for custom routing.

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
| `output` | `packetRouted` | `Fw.Signal` | Emitted after each authenticated packet is routed; used to reset command loss timer in ModeManager |

## Requirements

| Name | Description | Rationale | Validation |
| ---- | ----------- | --------- | ---------- |
SVC-ROUTER-001 | `Svc::ProvesRouter` shall route packets based on their packet type as indicated by the packet header | Routing mechanism of the F´ comms protocol | Unit test |
SVC-ROUTER-002 | `Svc::ProvesRouter` shall route packets of type `Fw::ComPacketType::FW_PACKET_COMMAND` to the `commandOut` output port. | Routing command packets | Unit test |
SVC-ROUTER-003 | `Svc::ProvesRouter` shall route packets of type `Fw::ComPacketType::FW_PACKET_FILE` to the `fileOut` output port. | Routing file packets | Unit test |
SVC-ROUTER-004 | `Svc::ProvesRouter` shall route data that is neither `Fw::ComPacketType::FW_PACKET_COMMAND` nor `Fw::ComPacketType::FW_PACKET_FILE` to the `unknownDataOut` output port. | Allows for projects to provide custom routing for additional (project-specific) uplink data types | Unit test |
SVC-ROUTER-005 | `Svc::ProvesRouter` shall emit warning events if serialization errors occur during processing of incoming packets | Aid in diagnosing uplink issues | Unit test |
SVC-ROUTER-005 | `Svc::ProvesRouter` shall make a copy of buffers that represent a `FW_PACKET_FILE` | Aid in memory management of file buffers | Unit test |
SVC-ROUTER-006 | `Svc::ProvesRouter` shall return ownership of all buffers received on `dataIn` through `dataReturnOut` | Memory management | Unit test |
SVC-ROUTER-007 | `Svc::ProvesRouter` shall emit the `packetRouted` signal after each successfully routed packet | Allows interested components to track uplink activity without command loss logic in this component | Unit test |
SVC-ROUTER-008 | `Svc::ProvesRouter` shall use the 0/1 setting in `authenticateCfg.hpp` to enable or disable authentication functionality in this router component | Authentication configuration | Unit test |
SVC-ROUTER-009 | `Svc::ProvesRouter` shall check a file for OpCodes that do not require authentication | OpCode exemption list | Unit test |
SVC-ROUTER-010 | `Svc::ProvesRouter` shall only pass authenticated commands and files that are not on the list of opcodes to be executed | Authentication filtering | Unit test |
SVC-ROUTER-011 | `Svc::ProvesRouter` shall pass non-authenticated commands and events backwards to `dataOut` | Non-authenticated packet routing | Unit test |
SVC-ROUTER-012 | `Svc::ProvesRouter` shall emit events for authenticated commands and non-authenticated commands and whether they passed through the router | Authentication event reporting | Unit test |

## Events

| Name | Severity | Parameters | Description |
|---|---|---|---|
| SerializationError | Warning High | status: U32 | Emitted when serializing a com buffer fails |
| DeserializationError | Warning High | status: U32 | Emitted when deserializing a packet type fails |
| AllocationError | Warning High | reason: AllocationReason | Emitted when buffer allocation fails |
| FileOpenError | Warning High | openStatus: U8 | Emitted when the bypass opcodes file cannot be opened |

## Telemetry Channels

| Name | Type | Description |
|---|---|---|
| ByPassedRouter | U64 | Number of Packets that have bypassed the router |
| PassedRouter | U64 | Number of Packets that have passed the router |
| FailedRouter | U64 | Number of Packets that have not passed the Router |
