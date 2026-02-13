# PayloadCom Usage Guide

## Overview

The PayloadCom receives images from the Nicla Vision camera over UART and saves them to the filesystem.

## Camera Commands

### Take a Snapshot

Send this command to the camera over UART:
```
snap
```

The camera will:
1. Take a photo
2. Save it locally as `images/img_NNNN.jpg`
3. Send it back over UART with the protocol:
   ```
   <IMG_START><SIZE>[4-byte size]</SIZE>[JPEG data]<IMG_END>
   ```

## F' Commands

### Send Command to Camera

Use the F' command interface to send commands to the camera:

```
SEND_COMMAND("snap")
```

This forwards the command over UART to the camera.

## Expected Behavior

### Successful Image Reception

1. **Command Sent**
   - Event: `CommandSuccess` - "Command snap sent successfully"

2. **Image Start**
   - PayloadCom receives `<IMG_START><SIZE>...bytes...</SIZE>`
   - Parses image size from 4-byte little-endian value
   - Allocates 256 KB buffer from BufferManager
   - Event: `ImageHeaderReceived` - "Received image header"

3. **Image Data**
   - Receives 512-byte chunks from camera
   - Accumulates in image buffer
   - Event: `UartReceived` - "Received UART data" (multiple times)

4. **Image Complete**
   - Receives all expected bytes (size from header)
   - Writes image to `/mnt/data/img_NNN.jpg`
   - Event: `DataReceived` - "Stored NNNNN bytes of payload data to /mnt/data/img_NNN.jpg"
   - Returns buffer to BufferManager
   - Note: Camera also sends `<IMG_END>` but it's trimmed off

### Image Filenames

Images are saved with sequential filenames:
- `/mnt/data/img_000.jpg`
- `/mnt/data/img_001.jpg`
- `/mnt/data/img_002.jpg`
- etc.

Counter increments each time a valid `<IMG_START><SIZE>...` header is received.

## Error Cases

### Buffer Allocation Failed

**Event:** `BufferAllocationFailed` - "Failed to allocate buffer of size 262144"

**Cause:**
- BufferManager pool exhausted (both 256 KB buffers in use)
- Previous image reception didn't complete/cleanup

**Recovery:**
- Wait for current image to complete
- Retry command

### Image Data Overflow

**Event:** `ImageDataOverflow` - "Image data overflow - buffer full"

**Cause:**
- Image larger than 256 KB
- Camera sent more data than buffer can hold

**Recovery:**
- Increase `IMAGE_BUFFER_SIZE` in PayloadCom.hpp
- Update BufferManager configuration in instances.fpp

### Command Error

**Event:** `CommandError` - "Failed to send snap command over UART"

**Cause:**
- UART driver not ready
- UART send failed

**Recovery:**
- Check UART connection
- Retry command

## Monitoring

### Key Telemetry (from BufferManager)

Check these telemetry points to monitor buffer usage:

- `payloadBufferManager.CurrBuffs` - Currently allocated buffers (0-2)
- `payloadBufferManager.TotalBuffs` - Total buffers available (2)
- `payloadBufferManager.HiBuffs` - High water mark
- `payloadBufferManager.NoBuffs` - Count of allocation failures
- `payloadBufferManager.EmptyBuffs` - Count of empty buffer returns

### Expected Values During Operation

**Idle:**
```
CurrBuffs = 0
TotalBuffs = 2
```

**Receiving Image:**
```
CurrBuffs = 1
TotalBuffs = 2
```

**If NoBuffs > 0:**
- Indicates allocation failures occurred
- Check if images are completing properly
- May need to increase buffer count

## Protocol Details

### UART Settings

- Baud rate: 115200
- Data bits: 8
- Stop bits: 1
- Parity: None

### Camera → Flight Software Protocol

```
Command from FS:  "snap\n"
Camera response:  "<IMG_START><SIZE>[4-byte little-endian uint32]</SIZE>"
                  [JPEG file bytes - exact size from header]
                  "<IMG_END>"
```

**Header format:**
- `<IMG_START>` - 11 bytes ASCII
- `<SIZE>` - 6 bytes ASCII
- Size value - 4 bytes binary (little-endian uint32)
- `</SIZE>` - 7 bytes ASCII
- **Total:** 28 bytes

**Example header for 51200 byte image:**
```
<IMG_START><SIZE>\x00\xC8\x00\x00</SIZE>
```

### Timing

- Command → Image start: ~2-3 seconds (camera capture time)
- Image transmission: depends on size
  - At 115200 baud ≈ 11.5 KB/s
  - 50 KB image ≈ 4-5 seconds
  - 200 KB image ≈ 17-18 seconds

## Testing

### Basic Test

1. Ensure camera is powered and UART connected
2. Send command: `SEND_COMMAND("snap")`
3. Monitor events for successful completion
4. Check file exists: `/mnt/data/img_000.jpg`
5. Verify file size is reasonable (typically 30-100 KB for VGA JPEG)

### Stress Test

Send multiple consecutive `snap` commands:
```
SEND_COMMAND("snap")
# Wait for DataReceived event
SEND_COMMAND("snap")
# Wait for DataReceived event
SEND_COMMAND("snap")
# etc.
```

Should handle at least 10+ images sequentially without errors.

### Buffer Exhaustion Test

Send 3 `snap` commands rapidly (don't wait):
```
SEND_COMMAND("snap")
SEND_COMMAND("snap")
SEND_COMMAND("snap")  # This should fail with BufferAllocationFailed
```

First two should succeed, third should fail gracefully.

## Filesystem Requirements

### Mount Point

The handler expects `/mnt/data/` to be a writable filesystem.

If using a different mount point, update in `processProtocolBuffer()`:
```cpp
snprintf(filename, sizeof(filename), "/your/path/img_%03d.jpg", m_data_file_count++);
```

### Space Requirements

Each VGA JPEG is typically 30-100 KB.

With 2 MB filesystem, you can store ~20-60 images.

Monitor filesystem usage with the `fsSpace` component.

## Troubleshooting

### No Images Received

1. **Check UART connection**
   - Verify peripheralUartDriver is configured
   - Check baud rate matches camera (115200)

2. **Check camera is responding**
   - Send any command and check for `UartReceived` events
   - Verify camera power

3. **Check events**
   - Should see `CommandSuccess` when command sent
   - Should see `ImageHeaderReceived` when camera responds

### Images Corrupted

1. **Check for buffer overflow**
   - Look for `ImageDataOverflow` events
   - Increase buffer size if needed

2. **Check UART errors**
   - Verify no data corruption on UART
   - Check for timing issues

3. **Verify complete reception**
   - Image size in `DataReceived` event should match expected
   - Compare with camera's saved file size

### Images Not Saving

1. **Check filesystem mounted**
   - Verify `/mnt/data/` exists and is writable

2. **Check disk space**
   - Monitor `fsSpace` telemetry

3. **Check file operations**
   - Add error events for file open/write failures
   - Check file permissions

## Advanced Configuration

### Changing Buffer Pool Size

To support larger images or more simultaneous captures, edit `instances.fpp`:

```fpp
// Support 4 images of 512 KB each = 2 MB pool
bins[0].bufferSize = 512 * 1024;
bins[0].numBuffers = 4;
```

Must also update `PayloadCom.hpp`:
```cpp
static constexpr U32 IMAGE_BUFFER_SIZE = 512 * 1024;
```

### Verifying Image Integrity

Since the protocol now includes size, you can verify completeness:

```cpp
// In processCompleteImage()
if (m_imageBufferUsed != m_expected_size) {
    this->log_WARNING_HI_ImageSizeMismatch(m_expected_size, m_imageBufferUsed);
}
```

This catches truncation or corruption during transmission.

### Multiple Image Formats

To support different image sizes/formats, configure multiple buffer bins:

```fpp
// Small images (thumbnail)
bins[0].bufferSize = 32 * 1024;
bins[0].numBuffers = 4;

// Large images (full resolution)
bins[1].bufferSize = 256 * 1024;
bins[1].numBuffers = 2;
```

BufferManager will automatically select the appropriate bin.
