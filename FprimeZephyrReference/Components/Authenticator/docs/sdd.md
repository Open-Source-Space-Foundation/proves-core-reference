# Components::Authenticator

The Authenticator component adds HMAC (Hash-Based Message Authentication Code) authentication to CCSDS Space Packets on downlink in accordance with CCSDS Space Data Link Security Protocol, CCSDS 355.0-B-2 (July 2022). This component is the counterpart to the Authenticate component: while Authenticate verifies and removes security fields on uplink, Authenticator adds security fields to packets on downlink.

## Overview

The Authenticator component sits in the downlink communications path between the `SpacePacketFramer` and the `ComAggregator` (or `TmFramer`) components. It adds security headers and HMAC authentication to Space Packets before they are transmitted to the ground station. The component implements security features including:

- HMAC-based authentication per CCSDS 355.0-B-2
- Security Association (SA) management with SPI-based key selection
- Sequence number generation and management per Security Association
- APID-based filtering to determine which packets require authentication

The security fields added by Authenticator are the exact fields that Authenticate expects to verify on the ground.

## Topology Integration

The component is integrated into the downlink path as follows:

```
SpacePacketFramer.dataOut -> Authenticator.dataIn
Authenticator.dataOut -> ComAggregator.dataIn (or directly to TmFramer)
ComAggregator.dataReturnOut -> Authenticator.dataReturnIn
Authenticator.dataReturnOut -> SpacePacketFramer.dataReturnIn

### HMAC Computation

The HMAC is computed over the following fields in order (per CCSDS 355.0-B-2 section 2.3.2.3.1):
1. **Frame Header**: The CCSDS Space Packet Primary Header (6 bytes) - Bytes 0-5 of the Space Packet
2. **Security Header**: SPI (2 bytes) + Sequence Number (4 bytes) + Reserved (2 bytes) = 8 bytes (bytes 6-13 of new data field)
3. **Frame Data Field**: The F Prime telemetry/event packet payload (original payload, bytes 22-N of new data field)

**HMAC Algorithm**: HMAC-SHA256, truncated to 64 bits (8 bytes) for the security trailer.

**Key Selection**: The secret key is selected based on the SPI value. Each SPI maps to a Security Association containing:
- A secret key (shared with ground station)
- Current sequence number
- Sequence number window size
- Associated APID(s)

### APID Extraction and Filtering

The component extracts the APID directly from the Space Packet Primary Header (bytes 0-1, bottom 11 bits of Packet Identification field). This APID is used to determine if the packet requires authentication (matches configured telemetry/event APID list)

## Requirements

| Name | Description | Validation |
|---|---|---|
| AUTHENTICATOR001 | The component shall only apply HMAC authentication to packets where the APID (extracted from the Space Packet Primary Header) matches a configured list of telemetry/event APIDs requiring authentication. | Unit Test, Inspection |
| AUTHENTICATOR002 | The component shall forward packets with APIDs not in the authentication list directly to `dataOut` without authentication. | Unit Test, Inspection |
| AUTHENTICATOR005 | The component shall increment and use the current sequence number for the selected Security Association every time it encodes a message. | Unit Test |
| AUTHENTICATOR006 | The component shall insert the Security Header (8 bytes: SPI + Sequence Number + Reserved) at the beginning of the Space Packet data field. | Unit Test |
| AUTHENTICATOR007 | The component shall compute the HMAC over: (a) the Space Packet Primary Header (bytes 0-5), (b) the Security Header (SPI + Sequence Number + Reserved), and (c) the Frame Data Field (original payload). The HMAC shall be computed using HMAC-SHA256 with the secret key associated with the SPI, truncated to 64 bits. | Unit Test |
| AUTHENTICATOR008 | The component shall insert the computed HMAC (8 bytes) as the Security Trailer at the end of the Space Packet data field (before the payload ends). | Unit Test |
| AUTHENTICATOR009 | The component shall update the Space Packet Primary Header's Packet Data Length field to reflect the increased data field size (original length + 16 bytes). | Unit Test |
| AUTHENTICATOR011 | After successful authentication field insertion, the component shall emit an `AuthenticatedPacket` event containing the APID, SPI, and sequence number for telemetry/logging purposes. | Unit Test |
| AUTHENTICATOR012 | The component shall allocate a new buffer for the authenticated packet and forward it to `dataOut`. The original input buffer shall be returned via `dataReturnOut`. | Unit Test |
| AUTHENTICATOR013 | The component shall handle sequence number rollover correctly (when sequence number transitions from 0xFFFFFFFF to 0x00000000). | Unit Test |
| AUTHENTICATOR015 | The component shall support multiple Security Associations, each identified by a unique SPI value and containing its own secret key, sequence number, and associated APIDs (TOD FOR US PROB HAMC). | Unit Test, Inspection |
| AUTHENTICATOR017 | The component shall provide commands to get and set the current sequence number for a given Security Association (SPI) to enable ground station synchronization. | Unit Test, Inspection |
| AUTHENTICATOR018 | If an invalid SPI is requested (not configured), the component shall emit an `InvalidSPI` event and handle the error gracefully. | Unit Test |

## Port Descriptions

| Name | Direction | Type | Description |
|------|-----------|------|-------------|
| dataIn | Input (guarded) | `Svc.ComDataWithContext` | Port receiving Space Packets from SpacePacketFramer. For packets requiring authentication, contains Space Packet Primary Header (6 bytes) and F Prime telemetry/event payload. |
| dataOut | Output | `Svc.ComDataWithContext` | Port forwarding authenticated or non-authenticated packets to ComAggregator/TmFramer. For authenticated packets, contains Space Packet with security headers and trailer inserted in data field. |
| dataReturnOut | Output | `Svc.ComDataWithContext` | Port returning ownership of input buffers back to upstream component (SpacePacketFramer) for buffer deallocation. |
| dataReturnIn | Input (sync) | `Svc.ComDataWithContext` | Port receiving back ownership of buffers sent to dataOut, to be forwarded to the upstream component. |
| bufferAllocate | Output | `Fw.BufferGet` | Port to allocate buffers for authenticated packets (new buffers with security fields inserted). |
| bufferDeallocate | Output | `Fw.BufferSend` | Port to deallocate buffers once authenticated packets are sent downstream. |
| comStatusIn | Input (sync) | `Fw.SuccessCondition` | Port receiving status from downstream component indicating readiness for more data. |
| comStatusOut | Output | `Fw.SuccessCondition` | Port indicating status of Authenticator for receiving more data from upstream. |

## Events

| Name | Severity | Description |
|---|---|---|
| AuthenticatedPacket | Activity High | Emitted when a packet successfully has security fields added. Contains the APID, SPI, and sequence number used. Format: "Authenticated packet: APID={}, SPI={}, SeqNum={}" |
| InvalidSPI | Warning High | Emitted when a command references an SPI value that does not correspond to any configured Security Association. Format: "Invalid SPI received: SPI={}, APID={}" |
| SequenceNumberRollover | Activity High | Emitted when a sequence number rollover occurs for a Security Association. Format: "Sequence number rollover: SPI={}, NewSeqNum={}" |

## Telemetry Channels

| Name | Type | Description |
|---|---|---|
| AuthenticatedPacketsCount | U64 | Total count of successfully authenticated packets (packets with security fields added) |
| CurrentSequenceNumber | U32 | Current sequence number for the primary Security Association (or most recently used SA) |

## Commands

| Name | Parameters | Description |

TODO: Maybe we only need one on one of these those components?
|---|---|---|
| GET_SEQ_NUM | SPI: U16 | Command to retrieve the current sequence number for the Security Association identified by the given SPI. Response includes SPI and current sequence number. |
| SET_SEQ_NUM | SPI: U16, SeqNum: U32 | Command to set the sequence number for the Security Association identified by the given SPI. Used for synchronization with ground station. Response confirms the SPI and new sequence number. |

## Parameters


## Configuration

The component requires the following configuration (set in `Authenticator.hpp` or via initialization):


## Unit Tests

| Name | Description | Output | Coverage |
|---|---|---|---|
