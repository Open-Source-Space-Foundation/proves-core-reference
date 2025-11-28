# Svc::AuthenticationRouter

The `Svc::AuthenticationRouter` component routes F´ packets (such as command or file packets) to other components. It is based on the FPrime Router, explained and linked later in the sdd, with two exceptions

1. Command Loss Time component, which checks how long since commands have last been routed through the satellite. If it has been too long (changeable parameter) it will emit an event and send out to safemode, to conserve power
As a result of this component writing to the filesystem, this component is active, while the usual FprimeRouter is passive.

2. This component handles authenticated packets. It will check a list of preconfigured opcodes that do not need to be authenticated, will pass those on as well as the things that are authenticated

The `Svc::AuthenticationRouter` component receives F´ packets (as [Fw::Buffer](../../../Fw/Buffer/docs/sdd.md) objects) and routes them to other components through synchronous port calls. The input port of type `Svc.ComDataWithContext` passes this Fw.Buffer object along with optional context data which can help for routing. The current F Prime protocol does not use this context data, but is nevertheless present in the interface for compatibility with other protocols which may for example pass APIDs in the frame headers.

The `Svc::AuthenticationRouter` component supports `Fw::ComPacketType::FW_PACKET_COMMAND` and `Fw::ComPacketType::FW_PACKET_FILE` packet types. Unknown packet types are forwarded on the `unknownDataOut` port, which a project-specific component can connect to for custom routing.

About memory management, all buffers sent by `Svc::AuthenticationRouter` on the `fileOut` and `unknownDataOut` ports are expected to be returned to the router through the `fileBufferReturnIn` port for deallocation.

## Custom Routing

The `Svc::AuthenticationRouter` component is designed to be extensible through the use of a project-specific router. The `unknownDataOut` port can be connected to a project-specific component that can receive all unknown packet types. This component can then implement custom handling of these unknown packets. After processing, the project-specific component shall return the received buffer to the `Svc::AuthenticationRouter` component through the `fileBufferReturnIn` port (named this way as it only receives file packets in the common use-case), which will deallocate the buffer.

## Usage Examples

The `Svc::FprimeRouter` component is used in the uplink stack of many reference F´ application such as [the tutorials source code](https://github.com/fprime-community#tutorials).

### Typical Usage

In the canonical uplink communications stack, `Svc::FprimeRouter` is connected to a [Svc::CmdDispatcher](../../CmdDispatcher/docs/sdd.md) and a [Svc::FileUplink](../../FileUplink/docs/sdd.md) component, to receive Command and File packets respectively.

![uplink_stack](../../FprimeDeframer/docs/img/deframer_uplink_stack.png)

## Port Descriptions

| Kind | Name | Type | Description |
|---|---|---|---|
| `async input` | `schedIn` | `Svc.Sched` | Port receiving calls from the rate group for periodic command loss time checking |
| `async input` | `dataIn` | `Svc.ComDataWithContext` | Receiving Fw::Buffer with context buffer from Deframer |
| `output` | `dataReturnOut` | `Svc.ComDataWithContext` | Returning ownership of buffer received on `dataIn` |
| `output` | `commandOut` | `Fw.Com` | Port for sending command packets as Fw::ComBuffers |
| `output` | `fileOut` | `Fw.BufferSend` | Port for sending file packets as Fw::Buffer (ownership passed to receiver) |
| `sync input` | `fileBufferReturnIn` | `Fw.BufferSend` | Receiving back ownership of buffer sent on `fileOut` and `unknownDataOut` |
| `sync input` | `cmdResponseIn` | `Fw.CmdResponse` | Port for receiving command responses from a command dispatcher (can be a no-op) |
| `output` | `unknownDataOut` | `Svc.ComDataWithContext` | Port forwarding unknown data (useful for adding custom routing rules with a project-defined router) |
| `output` | `bufferAllocate` | `Fw.BufferGet` | Port for allocating buffers, allowing copy of received data |
| `output` | `bufferDeallocate` | `Fw.BufferSend` | Port for deallocating buffers |
| `output` | `SafeModeOn` | `Fw.Signal` | Port for sending signal to safemode when command loss time expires |

## Requirements

| Name | Description | Rationale | Validation |
|---|---|---|---|
SVC-ROUTER-001 | `Svc::AuthenticationRouter` shall route packets based on their packet type as indicated by the packet header | Routing mechanism of the F´ comms protocol | Unit test |
SVC-ROUTER-002 | `Svc::AuthenticationRouter` shall route packets of type `Fw::ComPacketType::FW_PACKET_COMMAND` to the `commandOut` output port. | Routing command packets | Unit test |
SVC-ROUTER-003 | `Svc::AuthenticationRouter` shall route packets of type `Fw::ComPacketType::FW_PACKET_FILE` to the `fileOut` output port. | Routing file packets | Unit test |
SVC-ROUTER-004 | `Svc::AuthenticationRouter` shall route data that is neither `Fw::ComPacketType::FW_PACKET_COMMAND` nor `Fw::ComPacketType::FW_PACKET_FILE` to the `unknownDataOut` output port. | Allows for projects to provide custom routing for additional (project-specific) uplink data types | Unit test |
SVC-ROUTER-005 | `Svc::AuthenticationRouter` shall emit warning events if serialization errors occur during processing of incoming packets | Aid in diagnosing uplink issues | Unit test |
SVC-ROUTER-005 | `Svc::AuthenticationRouter` shall make a copy of buffers that represent a `FW_PACKET_FILE` | Aid in memory management of file buffers | Unit test |
SVC-ROUTER-006 | `Svc::AuthenticationRouter` shall return ownership of all buffers received on `dataIn` through `dataReturnOut` | Memory management | Unit test |
SVC-ROUTER-007 | `Svc::AuthenticationRouter` shall check command loss time periodically via the `schedIn` port | Command loss time monitoring | Unit test |
SVC-ROUTER-008 | `Svc::AuthenticationRouter` shall emit `CommandLossTimeExpired` event when command loss time exceeds `LOSS_MAX_TIME` parameter | Command loss time monitoring | Unit test |
SVC-ROUTER-009 | `Svc::AuthenticationRouter` shall send `SafeModeOn` signal when command loss time expires | Safe mode activation | Unit test |
SVC-ROUTER-010 | `Svc::AuthenticationRouter` shall update `LastCommandPacketTime` telemetry when a packet is received | Telemetry tracking | Unit test |
SVC-ROUTER-011 | `Svc::AuthenticationRouter` shall use the 0/1 setting in `authenticateCfg.hpp` to enable or disable authentication functionality in this router component | Authentication configuration | Unit test |
SVC-ROUTER-012 | `Svc::AuthenticationRouter` shall check a file for OpCodes that do not require authentication | OpCode exemption list | Unit test |
SVC-ROUTER-013 | `Svc::AuthenticationRouter` shall only pass authenticated commands and files that are not on the list of opcodes to be executed | Authentication filtering | Unit test |
SVC-ROUTER-014 | `Svc::AuthenticationRouter` shall pass non-authenticated commands and events backwards to `dataOut` | Non-authenticated packet routing | Unit test |
SVC-ROUTER-015 | `Svc::AuthenticationRouter` shall emit events for authenticated commands and non-authenticated commands | Authentication event reporting | Unit test |

## Events

| Name | Severity | Parameters | Description |
|---|---|---|---|
| CommandLossTimeExpired | Activity High | safemode: Fw.On | Emitted when command loss time expires. Format: "SafeModeOn: {}" |
| CurrentLossTime | Activity Low | loss_max_time: U32 | Emitted with current loss time information. Format: "Current Loss Time: {}" |

## Telemetry Channels

| Name | Type | Description |
|---|---|---|
| LastCommandPacketTime | U64 | The time of the last command packet |
| CommandLossSafeOn | bool | The status of the command loss time sending to safe mode |

## Parameters

| Name | Type | Default | Description |
|---|---|---|---|
| LOSS_MAX_TIME | U32 | 10 | The maximum amount of time (in seconds) since the last command before triggering safe mode |
