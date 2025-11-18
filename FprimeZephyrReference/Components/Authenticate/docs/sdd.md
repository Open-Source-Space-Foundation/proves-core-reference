# Components::Authenticate

The Authenticate component verifies the integrity and authenticity of CCSDS command packets using HMAC (Hash-Based Message Authentication Code) in accordance with CCSDS Space Data Link Security Protocol, CCSDS 355.0-B-2 (July 2022). It adds a parameter on the config that goes with every packet that says whether or not the component is authenticated.

## Overview

The Authenticate component sits in the uplink communications path between the `TcDeframer` and `SpacePacketDeframer` components. It adds a authenticated tag to the config of each packet before they are deframed and routed to the command dispatcher.  The component implements security features including:

- HMAC-based authentication per CCSDS 355.0-B-2
- Security Association (SA) management with SPI-based key selection
- Sequence number validation and anti-replay protection

Connections:
TcDeFramer.dataOut -> Authenticate.dataIn
Authenticate->dataOut -> SpacePacketDeframer
Authenticate->dataReturnOut -> TcDeframer.dataReturnIn
SpacePacketDeframer.dataReturnOut -> Authenticate.dataReturnIn

Authenticate is positioned before SpacePacketDeframer because CCSDS 355.0-B-2 specifies HMAC computation over the actual frame header bytes, which requires the header to be present

## Packet Format

### CCSDS Protocol Stack Context

The security protocol operates at the Space Packet level per CCSDS 355.0-B-2. The security headers and trailers are embedded within the Space Packet Data Field, which is itself in a TC Transfer Frame. The complete protocol stack is:

1.

First the TC Transfer Frame is removed by TcDeframer
[TC Header 5B] [Data Field] [TC Trailer 2B]


2.
THIS IS WHAT GOES THROUGH AUTHENTICATE

Now the Security Header and Security Trailer must be inside the data field, as specified by CCSDS 355.0-B-2

Space Packet (received by Authenticate)
[Security Header 8B]
[Space Packet Primary Header 6B]
   [Space Packet Data Field]
      [F Prime Command Packet]
[Security Trailer 8B]

The output from Authenticate:

Space Packet
   [Space Packet Primary Header 6B]
      [Space Packet Data Field]
         [F Prime Command Packet]

**Total packet structure:**
Security Header: of 6 octets in length (TM Baseline)

(SPI (2 octets + Sequence Number 4 octets))

- Space Packet Primary Header: 6 bytes
- Space Packet Data Field: 16 + command size bytes
  - F Prime Command Packet: Variable (up to 225 bytes)
- Security Trailer: 8 bytes (HMAC)

Packets NOT requiring authentication are forwarded unchanged to `dataOut. Later we have to get the FPrime router to route radio packets based on the authenticated tag on the config

### HMAC Computation

The HMAC is computed over the following fields in order (per CCSDS 355.0-B-2 section 2.3.2.3.1):
1. **Security Header**: SPI (2 bytes) + Sequence Number (4 bytes) + Reserved (2 bytes) = 6 bytes
2. **Frame Data Field**: The F Prime command packet payload (bytes 22-N)

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

## Parameters
name | type | use
--- | ------| ----
SEQ_NUM_WINDOW | U32 | default 50, how far from the hmac code number we can get

### GDS Plugin

In order to send commands to the satellite and test this component we also need a GDS plugin. The plugins are in the Framing Folder, specifically in the authenticate_plugin.py file.

You can make it by running

> make framer-plugin

and run it by running

> made gds-with-framer

### Uploads to run this code

To run the SPI selection (otherwise the encryption will be the default encryption), you should Uplink the spi_dict.txt file in the AuthenticateFiles folder

## Requirements

| Name | Description | Validation |
|---|---|---|
| AUTH001 | The component shall add a data field to the config saying whether or not the component is authenticated. | Unit Test, Inspection |
| AUTH002 | The component shall forward packets with thorough the command stack so commands can still be executed. | Unit Test, Inspection |
| AUTH003 | The component shall validate that the SPI value corresponds to a configured Security Association. If the SPI is not recognized, the packet shall use the default encryption key and type. | Unit Test |
| AUTH004 | The component shall validate the received sequence number against the stored sequence number for the Security Association identified by SPI. The sequence number must be greater than the stored value and within the configured sequence number window. | Unit Test |
| AUTH05 | The component shall compute the expected HMAC over: the Security Header bytes 6-13: SPI + Sequence Number + Reserved and the Frame Data Field (bytes 22-N: F Prime command packet payload). The HMAC shall be computed using HMAC-SHA256 with the secret key associated with the SPI, truncated to 64 bits. | Unit Test |
| AUTH06 | The component shall compare the computed HMAC with the received HMAC. If they match, authentication succeeds. If they do not match, authentication fails. | Unit Test |
| AUTH07| If authentication succeeds, the component shall update the stored sequence number for the Security Association to the received sequence number. | Unit Test |
| AUTH08 | If authentication succeeds, the component shall emit a ValidHash event containing the opcode (if extractable) and hash value for telemetry/logging purposes. | Unit Test |
| AUTH09 | The component shall remove the Security Header  and Security Trailer from the Space Packet. The modified buffer then goes to the data out. | Inspection |
| AUTH010 | The component shall handle sequence number rollover correctly (when sequence number transitions from 0xFFFFFFFF to 0x00000000). | Unit Test |
| AUTH011 | The component shall support multiple Security Associations each identified by a unique SPI value and containing its own secret key and within the spi_dict.txt file| Unit Test, Inspection |
| AUTH012 | The component shall provide a command and telemetry channel to report the current sequence number for a given Security Association (SPI) to enable ground station synchronization. | Unit Test, Inspection

## Port Descriptions

| Name | Direction | Type | Description |
|------|-----------|------|-------------|
| dataIn | Input (guarded) | Svc.ComDataWithContext | Port receiving Space Packets from TcDeframer. For authenticated packets, contains Space Packet Primary Header (6 bytes), Security Header (8 bytes), HMAC (8 bytes), and F Prime command payload. |
| dataOut | Output | Svc.ComDataWithContext | Port forwarding authenticated or non-authenticated packets to SpacePacketDeframer. For authenticated packets, contains Space Packet with clean data field (security headers/trailers removed, but Space Packet header preserved). |
| dataReturnOut | Output | Svc.ComDataWithContext | Port returning ownership of invalid/unauthorized packets back to upstream component (TcDeframer) for buffer deallocation. |
| dataReturnIn | Input (sync) | Svc.ComDataWithContext | Port receiving back ownership of buffers sent to dataOut, to be forwarded to the upstream component. |

## Events

| Name | Severity | Parameters | Description |
|---|---|---|---|
| ValidHash | Activity High | apid: U32, spi: U32, seqNum: U32 | Emitted when a packet successfully passes HMAC authentication. Contains the APID, SPI, and sequence number of the authenticated packet. Format: "Authenticated packet: APID={}, SPI={}, SeqNum={}" |
| InvalidHash | Warning High | apid: U32, spi: U32, seqNum: U32 | Emitted when a packet fails HMAC authentication. Contains the APID, SPI, and sequence number of the failed packet. Format: "Authentication failed: APID={}, SPI={}, SeqNum={}" |
| SequenceNumberOutOfWindow | Warning High | spi: U32, expected: U32, window: U32 | Emitted when a packet is rejected due to sequence number being outside the configured window. Contains the SPI, expected sequence number, and window size. Format: "Sequence number out of window: seq_num={}, Expected={}, Window={}" |
| InvalidSPI | Warning High | spi: U32 | Emitted when a packet contains an SPI value that does not correspond to any configured Security Association. Contains the invalid SPI. Format: "Invalid SPI received: SPI={}" |
| APIDMismatch | Warning High | spi: U32, packetApid: U32 | Emitted when the APID in FrameContext does not match the APIDs associated with the Security Association identified by the SPI. Contains the SPI and the mismatched packet APID. Format: "APID mismatch: SPI={}, Packet APID={}" |
| EmitSequenceNumber | Activity High | seq_num: U32 | Emitted in response to GET_SEQ_NUM command to report the current sequence number. Format: "The current sequence number is {}" |
| SetSequenceNumberSuccess | Activity High | seq_num: U32, status: bool | Emitted after SET_SEQ_NUM command execution to indicate whether the sequence number was successfully set. Format: "sequence number has been set to {}: {}" |
| InvalidAuthenticationType | Warning High | authType: U32 | Emitted when an unsupported authentication type is encountered. Contains the invalid authentication type identifier. Format: "Invalid authentication type {}" |
| EmitSpiKey | Activity High | key: HashString, authType: HashString | Emitted in response to GET_KEY_FROM_SPI command to report the key and authentication type associated with a given SPI. Format: "SPI key is {} type is {}" |
| FileOpenError | Warning High | error: U32 | Emitted when there is an error opening or reading the SPI dictionary file. Contains the error code. Format: "File Error with Error {}" |
| FoundSPIKey | Activity Low | found: bool | Emitted during SPI lookup to indicate whether a matching SPI key was found in the dictionary file. Format: "Found SPI status: {}" |
| PacketTooShort | Warning High | packet_size: U32 | Emitted when a received packet is too short to contain the required security header and trailer. Contains the actual packet size. Format: "Received packet is too short ({}) to process for authentication" |
| CryptoComputationError | Warning High | status: U32 | Emitted when there is an error during HMAC computation (e.g., PSA crypto initialization failure, hash operation failure). Contains the error status code. Format: "Crypto Computation Error: {}" |

## Telemetry Channels

| Name | Type | Description |
|---|---|---|
| AuthenticatedPacketsCount | U64 | Total count of successfully authenticated packets |
| RejectedPacketsCount | U64 | Total count of rejected packets (authentication failures, invalid SPI, sequence number issues) |
| CurrentSequenceNumber | U32 | Current sequence number for the primary Security Association (or most recently used SA) |

## Commands

| Name | Type | Parameters | Description |
|---|---|---|---|
| GET_SEQ_NUM | Sync | None | Command to retrieve the current sequence number for debugging/verification purposes. Emits EmitSequenceNumber event. |
| SET_SEQ_NUM | Sync | seq_num: U32 | Command to manually set the current sequence number (should be used with caution as it affects authentication). Emits SetSequenceNumberSuccess event. |
| GET_KEY_FROM_SPI | Sync | spi: U32 | Command to retrieve the authentication key and type associated with a given SPI value. Emits EmitSpiKey event. |

## Unit Tests


## Plugin

To Run the Plugin, first activate the fprime-venv

to build the framer plugin

> make framer-plugin

and to start the gds with the

> make gds-with-framer
