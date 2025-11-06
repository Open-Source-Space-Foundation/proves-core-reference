# PayloadHandler Buffer Strategy

## Overview

The PayloadHandler uses a **two-buffer strategy** to efficiently handle both protocol/header data and large image data:

1. **Small static buffer** (2 KB) for protocol headers/commands
2. **Large dynamic buffer** (256 KB) from BufferManager for image data

## Why This Approach?

### Problem with Static Buffers for Images
- **Memory waste**: 100s of KB allocated permanently even when not receiving
- **Stack overflow**: Embedded systems have limited stack/static memory
- **Component bloat**: Every component instance would have huge buffer

### Solution: BufferManager
- Allocates memory **only when needed**
- Returns memory when done
- Memory pooling for efficiency
- Multiple components can share memory pool

## Architecture

```
UART Driver (64 byte chunks)
         ↓
   PayloadHandler
         ↓
   ┌─────┴─────┐
   ↓           ↓
Protocol     Image
Buffer       Buffer
(2 KB)       (256 KB)
static       dynamic
```

## Buffer Details

### Protocol Buffer
- **Size**: 2048 bytes (static allocation)
- **Purpose**: Parse commands, headers, metadata
- **Lifecycle**: Always present
- **Location**: `m_protocolBuffer[PROTOCOL_BUFFER_SIZE]`

### Image Buffer
- **Size**: 256 KB (dynamic allocation)
- **Purpose**: Accumulate complete image data
- **Lifecycle**: Allocated when receiving, deallocated when done
- **Location**: `m_imageBuffer` (Fw::Buffer from BufferManager)

## Data Flow

### 1. Idle State (No Image)
```
Incoming data → Protocol Buffer → Parse for commands
```

### 2. Image Header Detected
```
Parse "<IMG_START>\n" → allocateImageBuffer()
                      → m_receiving = true
                      → Start accumulating
```

### 3. Receiving Image
```
Incoming data → Image Buffer (accumulate) → Check each chunk for "<IMG_END>"
```

### 4. Image Complete
```
Detect "<IMG_END>\n" → processCompleteImage() → Write to file
                     → deallocateImageBuffer()
                     → m_receiving = false
```

## Camera Protocol

The Nicla Vision camera sends images using this protocol:

```
<IMG_START>\n
[raw JPEG data in 512-byte chunks]
\n<IMG_END>\n
```

Example:
```
<IMG_START>
ÿØÿà...JFIF...image data...ÿÙ
<IMG_END>
```

## Key Functions

### Buffer Allocation
```cpp
bool allocateImageBuffer()
```
- Requests 256 KB buffer from BufferManager
- Returns true on success, false on failure
- Logs event if allocation fails

### Buffer Deallocation
```cpp
void deallocateImageBuffer()
```
- Returns buffer to BufferManager
- Called automatically when image complete or on error

### Data Accumulation
```cpp
bool accumulateProtocolData(const U8* data, U32 size)  // Small chunks
bool accumulateImageData(const U8* data, U32 size)     // Large chunks
```

## Configuration

### BufferManager Setup (instances.fpp)
```fpp
instance payloadBufferManager: Svc.BufferManager {
  // 256 KB buffers, 2 buffers = 512 KB total
  bins[0].bufferSize = 256 * 1024
  bins[0].numBuffers = 2
}
```

### Topology Connections (topology.fpp)
```fpp
payload.allocate -> payloadBufferManager.bufferGetCallee
payload.deallocate -> payloadBufferManager.bufferSendIn
```

## Memory Usage

### Static (Always Allocated)
- Protocol buffer: 2 KB
- Component overhead: ~1 KB
- **Total: ~3 KB**

### Dynamic (Only When Receiving)
- Image buffer: 256 KB (when allocated)
- **Total: 0-256 KB**

### Pool Configuration
- 2 buffers of 256 KB = **512 KB total pool**
- Allows receiving 2 images simultaneously or buffering one while processing another

## Customization

### To Change Buffer Sizes

1. **Protocol buffer** (PayloadHandler.hpp):
```cpp
static constexpr U32 PROTOCOL_BUFFER_SIZE = 2048;  // Change this
```

2. **Image buffer** (PayloadHandler.hpp):
```cpp
static constexpr U32 IMAGE_BUFFER_SIZE = 256 * 1024;  // Change this
```

3. **BufferManager pool** (instances.fpp):
```cpp
bins[0].bufferSize = 256 * 1024;  // Must match IMAGE_BUFFER_SIZE
bins[0].numBuffers = 2;           // Number of buffers in pool
```

### To Add Different Buffer Sizes

You can configure multiple buffer bins in the BufferManager:
```cpp
bins[0].bufferSize = 256 * 1024;  // 256 KB for large images
bins[0].numBuffers = 2;
bins[1].bufferSize = 64 * 1024;   // 64 KB for small images
bins[1].numBuffers = 4;
```

BufferManager will allocate the smallest buffer that fits the request.

## Implementation Details

### Protocol Parsing (Implemented)

The `processProtocolBuffer()` function:
1. Scans for newline-terminated lines
2. Checks if line matches `<IMG_START>`
3. If match, calls `allocateImageBuffer()` and sets `m_receiving = true`
4. Generates filename: `/mnt/data/img_NNN.jpg`

### Image Reception (Implemented)

The `in_port_handler()` function:
- When `m_receiving == true`, accumulates data into image buffer
- Each chunk is checked for `<IMG_END>` marker using `findImageEndMarker()`
- When marker found, extracts image data (before marker) and calls `processCompleteImage()`

### End Marker Detection

The camera sends:
```
[image data]\n<IMG_END>\n
```

The handler searches each incoming chunk for `<IMG_END>` and stops accumulation when found. Image data before the marker is written to file.

## Error Handling

### Allocation Failure
- Logs `BufferAllocationFailed` event
- Continues processing protocol buffer
- Can retry later if needed

### Buffer Overflow
- Image buffer overflow: Logs `ImageDataOverflow` event, deallocates buffer
- Protocol buffer overflow: Clears buffer and restarts

### Component Destruction
- Destructor automatically deallocates any active image buffer
- Prevents memory leaks

## Performance Notes

- **Allocation overhead**: ~microseconds (one-time per image)
- **Copy overhead**: memcpy for each 64-byte UART chunk (~100 cycles)
- **Memory efficiency**: Only allocates when receiving, 256x better than static
- **Latency**: No impact on UART receive rate (64 bytes @ 10Hz = 640 bytes/sec)

## Testing Checklist

- [ ] Receive small image (< 256 KB)
- [ ] Receive large image (> 256 KB) - should fail gracefully
- [ ] Receive multiple images sequentially
- [ ] Handle allocation failure (simulate by allocating 2 buffers)
- [ ] Handle protocol buffer overflow
- [ ] Handle image buffer overflow
- [ ] Component cleanup (destructor)

