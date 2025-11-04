# Components::Authenticate

The Authenticate component verifies the integrity and authenticity of CCSDS command packets using HMAC (Hash-Based Message Authentication Code) in accordance with CCSDS Space Data Link Security Protocol, CCSDS 355.0-B-2 (July 2022).

## Overview

The Authenticate component sits in the uplink communications path between the `TcDeframer` and `SpacePacketDeframer` components. It filters out unauthorized or tampered command packets before they are deframed and routed to the command dispatcher.  The component implements security features including:

- HMAC-based authentication per CCSDS 355.0-B-2
- Security Association (SA) management with SPI-based key selection
- Sequence number validation and anti-replay protection
- APID-based filtering for command authentication requirements

## Topology Integration

The component is integrated into the uplink path as follows:

```
TcDeframer.dataOut -> Authenticate.dataIn
Authenticate.dataOut -> SpacePacketDeframer.dataIn
SpacePacketDeframer.dataReturnOut -> Authenticate.dataReturnIn
Authenticate.dataReturnOut -> TcDeframer.dataReturnIn
```

Authenticate is positioned before SpacePacketDeframer because CCSDS 355.0-B-2 specifies HMAC computation over the actual frame header bytes, which requires the header to be present

The component forwards only authenticated packets (or non-authenticated packets that don't require authentication) to the `SpacePacketDeframer`. Invalid or unauthorized packets are returned via `dataReturnOut` back to the upstream component for buffer deallocation.

## Packet Format

### CCSDS Protocol Stack Context

The security protocol operates at the Space Packet level per CCSDS 355.0-B-2. The security headers and trailers are embedded within the Space Packet Data Field, which is itself encapsulated in a TC Transfer Frame. The complete protocol stack is:

1. 

First the TC Transfer Frame is removed by TcDeframer
[TC Header 5B] [Data Field] [TC Trailer 2B]            


2. 
THIS IS WHAT GOES THROUGH AUTHENTICATE

Now the Security Header and Security Trailer must be inside the data field, as specificed by CCSDS 355.0-B-2 

Space Packet (received by Authenticate)
[Space Packet Primary Header 6B]                          
[Space Packet Data Field]                                 
    [Security Header 8B]
    [F Prime Command Packet] 
    [Security Trailer 8B]                                   

The output from Authenticate:

Space Packet 
   [Space Packet Primary Header 6B]                          
   [F Prime Command Packet] (security fields removed)        

**Total packet structure:**
- Space Packet Primary Header: 6 bytes
- Space Packet Data Field: 16 + command size bytes
  - Security Header: 8 bytes (SPI + Sequence Number + Reserved)
  - Security Trailer: 8 bytes (HMAC)
  - F Prime Command Packet: Variable (up to 225 bytes)

Packets NOT requiring authentication are forwarded unchanged to `dataOut. Later we have to get the FPrime router to route radio packets

### HMAC Computation

The HMAC is computed over the following fields in order (per CCSDS 355.0-B-2 section 2.3.2.3.1):
1. **Frame Header**: The CCSDS Space Packet Primary Header (6 bytes) - Extracted directly from the buffer at bytes 0-5
2. **Security Header**: SPI (2 bytes) + Sequence Number (4 bytes) + Reserved (2 bytes) = 8 bytes (bytes 6-13 of data field)
3. **Frame Data Field**: The F Prime command packet payload (bytes 22-N)

**Key Selection**: The secret key is selected based on the SPI value from the security header. Each SPI maps to a Security Association containing:
- A secret key (shared with ground station)
- Current sequence number
- Sequence number window size
- APID that you can validate

### Sequence Number Validation

- The component shall compare the received sequence number against the stored sequence number for the SA identified by SPI
- If the received sequence number is greater than the stored value (and within the sequence number window), the component shall update the stored sequence number
- If the received sequence number differs from the expected value by more than the sequence number window, the packet shall be rejected
- Sequence number rollover (from 0xFFFFFFFF to 0x00000000) shall be handled correctly

### APID Extraction and Validation

The component extracts the APID directly from the Space Packet Primary Header (bytes 0-1, bottom 11 bits of Packet Identification field). This APID is used to:
- Determine if the packet requires authentication (matches command APID list)
- Validate that the APID matches the Security Association's associated APIDs (security requirement AUTH015)

## Requirements

| Name | Description | Validation |
|---|---|---|
| AUTH001 | The component shall only apply HMAC authentication to packets where the APID (extracted from the Space Packet Primary Header) matches a configured list of command APIDs requiring authentication. | Unit Test, Inspection |
| AUTH002 | The component shall forward packets with APIDs matching a configured list of radio amateur command APIDs directly to `dataOut` without authentication. | Unit Test, Inspection |
| AUTH003 | The component shall validate that the SPI value corresponds to a configured Security Association. If the SPI is not recognized, the packet shall be rejected and returned via `dataReturnOut` and emit an event. | Unit Test |
| AUTH004 | The component shall validate the received sequence number against the stored sequence number for the Security Association identified by SPI. The sequence number must be greater than the stored value and within the configured sequence number window. It should also be able to deal with rollover. If validation fails, the packet shall be rejected and returned via `dataReturnOut`. | Unit Test |
| AUTH05 | The component shall compute the expected HMAC over: (a) the Space Packet Primary Header (bytes 0-5, actual header bytes), (b) the Security Header (bytes 6-13: SPI + Sequence Number + Reserved), and (c) the Frame Data Field (bytes 22-N: F Prime command packet payload). The HMAC shall be computed using HMAC-SHA256 with the secret key associated with the SPI, truncated to 64 bits. | Unit Test |
| AUTH06 | The component shall compare the computed HMAC with the received HMAC. If they match, authentication succeeds. If they do not match, authentication fails. | Unit Test |
| AUTH07| If authentication succeeds, the component shall update the stored sequence number for the Security Association to the received sequence number. | Unit Test |
| AUTH08 | If authentication succeeds, the component shall emit a ValidHash event containing the opcode (if extractable) and hash value for telemetry/logging purposes. | Unit Test |
| AUTH09 | If authentication succeeds, the component shall remove the Security Header  and Security Trailer from the Space Packe. The modified buffer then goes to the data out. | Inspection |
| AUTH015 | If authentication fails (invalid HMAC, invalid SPI, sequence number out of window), the component shall emit an InvalidHash event containing the opcode (if extractable) and hash value, then return the buffer via dataReturnOut for deallocation. | Unit Test |
| AUTH010 | The component shall handle sequence number rollover correctly (when sequence number transitions from 0xFFFFFFFF to 0x00000000). | Unit Test |
| AUTH011 | The component shall support multiple Security Associations (in our case, potentially one HMAC or just several keys for once HMAC), each identified by a unique SPI value and containing its own secret key, sequence number, and associated APIDs. | Unit Test, Inspection |
| AUTH012 | The component shall provide a command and telemetry channel to report the current sequence number for a given Security Association (SPI) to enable ground station synchronization. | Unit Test, Inspection (TODO NOTE DO WE JUST WANT ONE???)|

## Port Descriptions

| Name | Direction | Type | Description |
|------|-----------|------|-------------|
| dataIn | Input (guarded) | Svc.ComDataWithContext | Port receiving Space Packets from TcDeframer. For authenticated packets, contains Space Packet Primary Header (6 bytes), Security Header (8 bytes), HMAC (8 bytes), and F Prime command payload. |
| dataOut | Output | Svc.ComDataWithContext | Port forwarding authenticated or non-authenticated packets to SpacePacketDeframer. For authenticated packets, contains Space Packet with clean data field (security headers/trailers removed, but Space Packet header preserved). |
| dataReturnOut | Output | Svc.ComDataWithContext | Port returning ownership of invalid/unauthorized packets back to upstream component (TcDeframer) for buffer deallocation. |
| dataReturnIn | Input (sync) | Svc.ComDataWithContext | Port receiving back ownership of buffers sent to dataOut, to be forwarded to the upstream component. |

## Events

| Name | Severity | Description |
|---|---|---|
| ValidHash | Activity High | Emitted when a packet successfully passes HMAC authentication. Contains the extracted opcode (if available) and the computed hash value for telemetry/logging. Format: "Authenticated packet: APID={}, SPI={}, SeqNum={}, Opcode={}, Hash={}" |
| InvalidHash | Warning High | Emitted when a packet fails HMAC authentication, has an invalid SPI, or has a sequence number outside the acceptable window. Contains the extracted opcode (if available) and the received hash value. Format: "Authentication failed: APID={}, SPI={}, SeqNum={}, Opcode={}, Hash={}, Reason={}" |
| SequenceNumberOutOfWindow | Warning High | Emitted when a packet is rejected due to sequence number being outside the configured window. Format: "Sequence number out of window: SPI={}, Expected={}, Received={}, Window={}" |
| InvalidSPI | Warning High | Emitted when a packet contains an SPI value that does not correspond to any configured Security Association. Format: "Invalid SPI received: SPI={}, APID={}" |
| APIDMismatch | Warning High | Emitted when the APID in FrameContext does not match the APIDs associated with the Security Association identified by the SPI. Format: "APID mismatch: SPI={}, Packet APID={}, SA APIDs={}" |

## Telemetry Channels

TODO (remove if not needed)
| Name | Type | Description |
|---|---|---|
| AuthenticatedPacketsCount | U64 | Total count of successfully authenticated packets |
| RejectedPacketsCount | U64 | Total count of rejected packets (authentication failures, invalid SPI, sequence number issues) |
| CurrentSequenceNumber | U32 | Current sequence number for the primary Security Association (or most recently used SA) |

## Commands

| Name | Parameters | Description |
|---|---|---|
| GET_SEQ_NUM | U16 | Command to retrieve the current sequence number |
| SET_SEQ_NUM | U16 | Command to set the current sequence number |

## Configuration

The component requires the following configuration (set in `Authenticate.hpp` (note should some of these be set in parameters?)):

1. **Command APIDs List**: List of APIDs that do not require authentication (default: none)
2. **Radio Amateur APIDs List**: List of APIDs for radio amateur commands that bypass authentication
3. **Security Associations Table**: For each SPI:
   - SPI value (U16)
   - Secret key (256-bit key for HMAC-SHA256)
   - Start sequence number (U32)
   - Sequence number window (U32)
4. Start Sequence number

## Unit Tests
