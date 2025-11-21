# Payload Handler Design Guide

## Overview

The `PayloadCom` component provides a **generalizable UART communication layer** for interfacing with external payloads (e.g., cameras, sensors, actuators). It handles low-level UART operations, buffer management, and provides a clean interface for payload-specific protocol handlers.

This guide explains how to design your own payload handler component that works with `PayloadCom` to communicate with your custom payload hardware.

---

## Architecture

```
┌─────────────────┐         ┌──────────────┐         ┌──────────────────┐
│   Your Handler  │◄───────►│  PayloadCom  │◄───────►│  UART Driver     │
│  (e.g., Camera  │         │  (Generic)    │         │  (Hardware)      │
│    Handler)     │         │              │         │                  │
└─────────────────┘         └──────────────┘         └──────────────────┘
       ▲                            ▲
       │                            │
       └────────────────────────────┘
         Data Flow (Buffers)
```

### Component Responsibilities

**PayloadCom (Generic Layer):**
- Receives raw UART data and forwards to handler
- Forwards handler commands to UART
- Manages buffer lifecycle (allocation/deallocation)
- Provides ACK sending capability
- Logs UART activity

**Your Handler (Payload-Specific):**
- Parses payload-specific protocol
- Processes commands/responses
- Handles file operations (if needed)
- Implements payload-specific logic
- Sends ACKs through PayloadCom when needed

---

## PayloadCom Interface

### Input Ports (Your Handler → PayloadCom)

#### `commandOut: Drv.ByteStreamData`
Send commands to your payload through UART.

```cpp
// Example: Send "snap\n" command
const char* cmd = "snap\n";
Fw::Buffer cmdBuffer(
    reinterpret_cast<U8*>(const_cast<char*>(cmd)), 
    strlen(cmd)
);
this->commandOut_out(0, cmdBuffer, Drv::ByteStreamStatus::OP_OK);
```

**Important:** 
- Commands should include newline (`\n`) if your payload expects it
- Buffer is automatically managed by PayloadCom
- Status should be `OP_OK` for normal commands

### Output Ports (PayloadCom → Your Handler)

#### `dataIn: Drv.ByteStreamData` (sync input port)
Receives data from payload via UART.

```cpp
void YourHandler::dataIn_handler(
    FwIndexType portNum,
    Fw::Buffer& buffer,
    const Drv::ByteStreamStatus& status
) {
    // Check status first
    if (status != Drv::ByteStreamStatus::OP_OK) {
        // Handle error - PayloadCom will return buffer
        return;
    }
    
    // Process data
    const U8* data = buffer.getData();
    U32 size = buffer.getSize();
    
    // ... your protocol processing ...
    
    // CRITICAL: Do NOT return buffer here!
    // PayloadCom owns the buffer and will return it automatically
}
```

**Critical Buffer Management Rules:**
1. **DO NOT** call `bufferReturn_out()` in your handler
2. **DO NOT** keep references to buffer data after handler returns
3. **DO** copy data you need to keep into your own buffers
4. PayloadCom handles all buffer deallocation

---

## Designing Your Payload Handler

### Step 1: Define Your Protocol

Before writing code, clearly define your payload's communication protocol. Consider:

**Command Format:**
- Text commands? (e.g., `"snap\n"`, `"test\n"`)
- Binary commands? (e.g., `[0x01, 0x02, 0x03]`)
- Command length? Fixed or variable?

**Response Format:**
- Text responses? (e.g., `"OK\n"`)
- Binary responses? (e.g., `[status_byte, data...]`)
- Protocol headers? (e.g., `<IMG_START>...`)

**Data Transfer:**
- Streaming data? (e.g., images, sensor readings)
- Chunked transfer? (with ACKs between chunks)
- Size information? (in header or separate message)

**Example Protocol (Camera):**
```
Commands:
  "snap\n"     → Capture and send image
  "test\n"     → Test command, responds "OK\n"

Image Transfer Protocol:
  <IMG_START><SIZE>[4-byte LE uint32]</SIZE>[image data chunks]<IMG_END>
  
ACK Protocol:
  Payload sends: <MOISES>\n
  Handler sends: <MOISES>\n (after header, after each chunk, after footer)
```

### Step 2: Create Handler Component

#### FPP Definition (`YourHandler.fpp`)

```fpp
module Components {
    @ Your payload handler description
    passive component YourHandler {
        
        # Commands
        sync command YOUR_COMMAND()
        sync command SEND_COMMAND(cmd: string)
        
        # Events
        event CommandSuccess(cmd: string) severity activity high format "Command {} sent"
        event DataReceived(size: U32, path: string) severity activity high format "Received {} bytes"
        event ProtocolError(msg: string) severity warning high format "Protocol error: {}"
        
        # Ports
        @ Send commands to PayloadCom
        output port commandOut: Drv.ByteStreamData
        
        @ Receive data from PayloadCom
        sync input port dataIn: Drv.ByteStreamData
        
        # Standard AC ports...
        time get port timeCaller
        command reg port cmdRegOut
        command recv port cmdIn
        command resp port cmdResponseOut
        text event port logTextOut
        event port logOut
        telemetry port tlmOut
        param get port prmGetOut
        param set port prmSetOut
    }
}
```

#### Header File (`YourHandler.hpp`)

```cpp
#include "YourHandlerComponentAc.hpp"
#include "Os/File.hpp"  // If handling files

namespace Components {
    class YourHandler : public YourHandlerComponentBase {
    public:
        YourHandler(const char* const compName);
        ~YourHandler();
        
    private:
        // Handler for data from PayloadCom
        void dataIn_handler(
            FwIndexType portNum,
            Fw::Buffer& buffer,
            const Drv::ByteStreamStatus& status
        ) override;
        
        // Command handlers
        void YOUR_COMMAND_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
        void SEND_COMMAND_cmdHandler(
            FwOpcodeType opCode,
            U32 cmdSeq,
            const Fw::CmdStringArg& cmd
        ) override;
        
        // Protocol processing helpers
        void processProtocolBuffer();
        void sendAck();
        bool parseHeader(const U8* data, U32 size);
        
        // State variables
        bool m_receiving = false;
        U32 m_bytesReceived = 0;
        U32 m_expectedSize = 0;
        U8 m_protocolBuffer[128];
        U32 m_protocolBufferSize = 0;
        
        // File handling (if needed)
        Os::File m_file;
        std::string m_currentFilename;
        bool m_fileOpen = false;
    };
}
```

### Step 3: Implement Protocol Processing

#### Basic Data Handler Pattern

```cpp
void YourHandler::dataIn_handler(
    FwIndexType portNum,
    Fw::Buffer& buffer,
    const Drv::ByteStreamStatus& status
) {
    // 1. Check status
    if (status != Drv::ByteStreamStatus::OP_OK) {
        // Handle error state
        if (m_receiving && m_fileOpen) {
            handleTransferError();
        }
        return;  // PayloadCom handles buffer return
    }
    
    // 2. Validate buffer
    if (!buffer.isValid()) {
        return;
    }
    
    // 3. Get data (don't store pointer - copy what you need)
    const U8* data = buffer.getData();
    U32 dataSize = static_cast<U32>(buffer.getSize());
    
    // 4. Process based on state
    if (m_receiving && m_fileOpen) {
        // Currently receiving data stream
        handleDataChunk(data, dataSize);
    } else {
        // Looking for protocol headers/commands
        accumulateProtocolData(data, dataSize);
        processProtocolBuffer();
    }
    
    // 5. DO NOT return buffer - PayloadCom owns it!
}
```

#### Protocol Buffer Management

For protocols with headers (like image transfer), use a small buffer to accumulate header data:

```cpp
// In header
static constexpr U32 PROTOCOL_BUFFER_SIZE = 128;
U8 m_protocolBuffer[PROTOCOL_BUFFER_SIZE];
U32 m_protocolBufferSize = 0;

// Accumulate data until you have enough for header
bool accumulateProtocolData(const U8* data, U32 size) {
    if (m_protocolBufferSize + size > PROTOCOL_BUFFER_SIZE) {
        return false;  // Overflow
    }
    memcpy(&m_protocolBuffer[m_protocolBufferSize], data, size);
    m_protocolBufferSize += size;
    return true;
}

// Process buffer to find headers
void processProtocolBuffer() {
    // Search for protocol markers (e.g., "<IMG_START>")
    // Parse header when complete
    // Transition to receiving state when header valid
}
```

#### Streaming Data Handling

For streaming protocols (like image transfer):

```cpp
void handleDataChunk(const U8* data, U32 size) {
    // Write directly to file (or process)
    U32 remaining = m_expectedSize - m_bytesReceived;
    U32 toWrite = (size < remaining) ? size : remaining;
    
    if (writeChunkToFile(data, toWrite)) {
        m_bytesReceived += toWrite;
        
        // Send ACK after chunk (if protocol requires)
        sendAck();
        
        // Check if complete
        if (m_bytesReceived >= m_expectedSize) {
            finalizeTransfer();
        }
    } else {
        handleTransferError();
    }
}
```

### Step 4: Implement ACK Handling

ACKs are sent through PayloadCom's `commandOut` port:

```cpp
void YourHandler::sendAck() {
    const char* ackMsg = "<MOISES>\n";  // Your ACK format
    Fw::Buffer ackBuffer(
        reinterpret_cast<U8*>(const_cast<char*>(ackMsg)), 
        strlen(ackMsg)
    );
    // Send through commandOut - PayloadCom forwards to UART
    this->commandOut_out(0, ackBuffer, Drv::ByteStreamStatus::OP_OK);
}
```

**ACK Timing:**
- After receiving header (if protocol requires)
- After each data chunk (for chunked transfers)
- After receiving footer/end marker
- After command responses (if needed)

### Step 5: Command Sending

Send commands to payload through PayloadCom:

```cpp
void YourHandler::SEND_COMMAND_cmdHandler(
    FwOpcodeType opCode,
    U32 cmdSeq,
    const Fw::CmdStringArg& cmd
) {
    // Append newline if payload expects it
    Fw::CmdStringArg tempCmd = cmd;
    tempCmd += "\n";
    
    Fw::Buffer commandBuffer(
        reinterpret_cast<U8*>(const_cast<char*>(tempCmd.toChar())), 
        tempCmd.length()
    );
    
    // Send to PayloadCom (forwards to UART)
    this->commandOut_out(0, commandBuffer, Drv::ByteStreamStatus::OP_OK);
    
    this->log_ACTIVITY_HI_CommandSuccess(Fw::LogStringArg(cmd));
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}
```

---

## Protocol Design Patterns

### Pattern 1: Simple Command/Response

**Payload:** Sends text commands, expects text responses

```python
# Payload code
uart.write("command\n")
response = uart.readline()  # "OK\n"
```

**Handler:**
- Accumulate data until newline
- Parse text command
- Send response if needed
- No streaming, no ACKs required

### Pattern 2: Chunked Transfer with ACK

**Payload:** Sends data in chunks, waits for ACK between chunks

```python
# Payload code
uart.write(header)
ack = uart.readline()  # Wait for ACK
uart.write(chunk1)
ack = uart.readline()  # Wait for ACK
uart.write(chunk2)
# ...
```

**Handler:**
- Parse header to get size
- Send ACK after header
- Receive chunks, write to file
- Send ACK after each chunk
- Send final ACK after footer

### Pattern 3: Binary Protocol

**Payload:** Sends binary commands/responses

```python
# Payload code
uart.write(bytes([0x01, 0x02, 0x03]))  # Binary command
response = uart.read(4)  # Binary response
```

**Handler:**
- Parse fixed-size headers
- Handle binary data
- May need endianness conversion
- Use `struct.pack`/`struct.unpack` equivalents

---

## File Handling (If Needed)

### Opening Files

```cpp
char filename[64];
snprintf(filename, sizeof(filename), "/data_%03d.bin", m_fileCount++);
m_currentFilename = filename;

Os::File::Status status = m_file.open(
    m_currentFilename.c_str(),
    Os::File::OPEN_WRITE
);

if (status != Os::File::OP_OK) {
    // Handle error
    return;
}
m_fileOpen = true;
```

### Writing Chunks

```cpp
bool writeChunkToFile(const U8* data, U32 size) {
    if (!m_fileOpen || size == 0) {
        return false;
    }
    
    U32 totalWritten = 0;
    const U8* ptr = data;
    
    while (totalWritten < size) {
        FwSizeType toWrite = static_cast<FwSizeType>(size - totalWritten);
        Os::File::Status status = m_file.write(
            ptr,
            toWrite,
            Os::File::WaitType::WAIT
        );
        
        if (status != Os::File::OP_OK) {
            return false;
        }
        
        totalWritten += static_cast<U32>(toWrite);
        ptr += toWrite;
    }
    
    return true;
}
```

### Closing Files

```cpp
void finalizeTransfer() {
    if (m_fileOpen) {
        m_file.close();
        m_fileOpen = false;
    }
    
    // Log success
    Fw::LogStringArg pathArg(m_currentFilename.c_str());
    this->log_ACTIVITY_HI_DataReceived(m_bytesReceived, pathArg);
    
    // Reset state
    m_receiving = false;
    m_bytesReceived = 0;
    m_expectedSize = 0;
}
```

---

## Integration with Topology

### 1. Add Handler to Topology

In `instances.fpp`:
```fpp
instance yourHandler: Components.YourHandler base id 0x3000 \
    queue size 10 \
    stack size 16384 \
    priority 140;
```

### 2. Connect Ports

In `topology.fpp`:
```fpp
# Connect PayloadCom to YourHandler
connections PayloadCom {
    uartDataOut -> yourHandler.dataIn
}

connections YourHandler {
    commandOut -> payloadCom.commandIn
}
```

### 3. Register Commands

Commands are automatically registered through F Prime's autocoder.

---

## Best Practices

### 1. Buffer Management
- ✅ Copy data you need to keep
- ✅ Use small protocol buffers for headers
- ✅ Don't store pointers to buffer data
- ❌ Don't return buffers (PayloadCom handles it)
- ❌ Don't keep references after handler returns

### 2. Error Handling
- Always check `ByteStreamStatus` before processing
- Handle file errors gracefully
- Reset state on errors
- Log errors for debugging

### 3. Protocol Design
- Use clear markers (e.g., `<IMG_START>`, `<IMG_END>`)
- Include size information in headers
- Design for chunked transfer (don't assume all data arrives at once)
- Use ACKs for reliability if needed

### 4. Performance
- Write data directly to file during streaming (don't buffer entire file)
- Send ACKs promptly (payload may be waiting)
- Log sparingly during high-speed transfers
- Use efficient parsing (avoid string operations for binary data)

### 5. Testing
- Test with payload simulator first
- Test with partial data (headers split across buffers)
- Test error cases (file write failures, buffer overflows)
- Test ACK timing (payload may timeout)

---

## Example: Camera Handler Reference

See `CameraHandler` component for a complete implementation example:

- **Protocol:** `<IMG_START><SIZE>[uint32]</SIZE>[data]<IMG_END>`
- **ACK Pattern:** After header, after each chunk, after footer
- **File Handling:** Streaming write to filesystem
- **Command Format:** Text commands with newline (`"snap\n"`)

Key implementation details:
- Protocol buffer for header accumulation
- State machine (idle → receiving → complete)
- Chunked file writing
- ACK sending through PayloadCom
- Error recovery and state reset

---

## Payload Code Example (Python/OpenMV)

Your payload code should:

1. **Wait for commands:**
```python
while True:
    msg = uart.readline()
    if msg:
        command = msg.decode("utf-8").strip()
        if command == "snap":
            capture_and_send_image()
```

2. **Send protocol headers:**
```python
uart.write(b"<IMG_START><SIZE>")
uart.write(struct.pack("<I", file_size))  # 4-byte LE uint32
uart.write(b"</SIZE>")
```

3. **Wait for ACKs:**
```python
def wait_for_ack():
    ack = uart.readline()  # Read until newline
    return ack is not None
```

4. **Send data in chunks:**
```python
chunk_size = 64
with open(filename, "rb") as f:
    while True:
        chunk = f.read(chunk_size)
        if not chunk:
            break
        uart.write(chunk)
        if not wait_for_ack():
            return False  # Transfer failed
```

5. **Send footer:**
```python
uart.write(b"<IMG_END>")
wait_for_ack()  # Final ACK
```

---

## Troubleshooting

### Issue: Buffer Management Errors
**Symptom:** Crashes, memory leaks  
**Solution:** Ensure you never return buffers from handler, don't keep buffer pointers

### Issue: Protocol Not Parsing
**Symptom:** Headers not detected  
**Solution:** Check protocol buffer size, handle split headers across multiple buffers

### Issue: ACK Timing Issues
**Symptom:** Payload times out  
**Solution:** Send ACKs immediately after receiving data, check UART timeout settings

### Issue: File Write Failures
**Symptom:** Files incomplete or missing  
**Solution:** Check filesystem space, handle partial writes, verify file close

### Issue: Commands Not Working
**Symptom:** Payload doesn't respond  
**Solution:** Verify newline included, check UART baud rate, verify command format

---

## Summary

1. **PayloadCom** = Generic UART layer (don't modify)
2. **Your Handler** = Payload-specific protocol processing
3. **Protocol** = Define clearly before implementation
4. **Buffers** = PayloadCom owns them, copy what you need
5. **ACKs** = Send through `commandOut` port when protocol requires
6. **Files** = Stream write during transfer, don't buffer entire file
7. **State** = Track receiving state, reset on errors

Follow the `CameraHandler` example as a reference implementation, and adapt the patterns to your specific payload protocol.

