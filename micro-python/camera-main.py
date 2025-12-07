"""OpenMV Nicla Vision - UART Image Transfer.

Compatible with PayloadHandler component
Protocol: <IMG_START><SIZE>[4-byte uint32]</SIZE>[JPEG data]<IMG_END>

Notes:
- This version is defensive about UART writes/reads and checks ACK contents.
- It attempts to be robust to boot timing and power-related issues.

NOTE: If code crashes, remove optional type hints.
"""

import struct
import time

import sensor
from pyb import LED, UART


# --- Utility functions ---
def blink_led(led: LED, duration_ms: int = 100) -> None:
    """Blink an LED once.

    Args:
        led: LED object to blink
        duration_ms: How long to keep LED on (in milliseconds)
    """
    led.on()
    time.sleep_ms(duration_ms)  # type: ignore[attr-defined]
    led.off()


def write_all(uart_obj: UART, data: bytes, write_timeout_ms: int = 2000) -> bool:
    """Ensure all bytes in `data` are written to UART.

    Returns True on success, False on timeout/partial write.
    """
    total = len(data)
    off = 0
    start = time.ticks_ms()  # type: ignore[attr-defined]
    while off < total:
        written = uart_obj.write(data[off:])  # may return number of bytes written or None
        if written is None:
            written = 0
        off += written
        # If nothing written, check timeout
        if written == 0 and time.ticks_diff(time.ticks_ms(), start) > write_timeout_ms:  # type: ignore[attr-defined]
            return False
    return True


# --- UART Setup ---
# Use a per-character timeout (ms) when reading; we'll also use a total-timeout mechanism.
uart = UART("LP1", 115200, timeout=1000)  # 1s inter-char timeout (OpenMV UART)

# --- LEDs ---
red = LED(1)  # error indicator
green = LED(2)  # activity (command received)
blue = LED(3)  # idle heartbeat

# --- STATE MACHINE ---
STATE_IDLE = 0
STATE_CAPTURE = 1
STATE_SEND = 2
STATE_ERROR = 3

# Set global state
state = STATE_IDLE

# Track current framesize
current_framesize = sensor.QVGA  # Default to QVGA
QUALITY = 90

# --- Camera Setup ---
# Add delay for hardware to stabilize when running standalone
time.sleep_ms(500)  # type: ignore[attr-defined]
image_count = 0

# Initialize camera with error handling for standalone operation
try:
    sensor.reset()
    # Brief pause after reset
    time.sleep_ms(100)  # type: ignore[attr-defined]
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)

    try:
        sensor.skip_frames(n=30)  # Skip 30 frames to let auto-exposure settle
    except RuntimeError as e:
        # If skip_frames times out (common when running standalone),
        # just continue - the camera will initialize on first actual snapshot()
        print("WARNING: skip_frames timed out (normal for standalone), camera will init on first capture: {}".format(e))
        time.sleep_ms(200)  # type: ignore[attr-defined]
except Exception as e:
    print("ERROR: Camera initialization failed:", e)
    # Flash red LED to indicate camera init failure
    for _ in range(5):
        blink_led(red, 100)
        time.sleep_ms(100)  # type: ignore[attr-defined]
    raise  # Re-raise to stop execution if camera can't initialize


# --- Communication functions ---


def is_ack(ack_bytes: bytes) -> bool:
    r"""Decide whether the received bytes qualify as an ACK.

    Accepts common variants like:
      b"<MOISES>\n", b"<MOISES>\r\n", b"MOISES\n", b"ACK\n"
    """
    if not ack_bytes:
        return False
    # work with uppercase, strip whitespace
    try:
        s = ack_bytes.strip().upper()
    except Exception:
        try:
            s = bytes(ack_bytes).strip().upper()
        except Exception:
            return False

    # common acceptable ACK tokens
    if b"<MOISES>" in s or s == b"<MOISES>" or s.startswith(b"<MOISES>"):
        return True
    if s == b"MOISES" or s.startswith(b"MOISES"):
        return True
    if s == b"ACK" or s.startswith(b"ACK"):
        return True
    return False


def wait_for_ack(total_timeout_ms: int = 3000) -> bytes | None:
    """Wait for an ACK line from UART up to total_timeout_ms.

    Returns the received bytes (non-empty) on success, or None on timeout.
    """
    deadline = time.ticks_add(time.ticks_ms(), total_timeout_ms)  # type: ignore[attr-defined]
    # We will use uart.readline() which returns bytes up to newline (or None on timeout)
    while time.ticks_diff(deadline, time.ticks_ms()) > 0:  # type: ignore[attr-defined]
        try:
            line = uart.readline()
        except Exception:
            line = None

        if line:
            # debug print (to USB console if connected)
            try:
                print("RX:", repr(line))
            except Exception:
                pass

            if is_ack(line):
                return line
            # not a recognized ack -> continue reading until timeout (ignore other chatter)
        # small sleep to yield CPU
        time.sleep_ms(10)  # type: ignore[attr-defined]
    # timed out
    return None


def send_image_protocol(jpeg_bytes: bytes) -> bool:
    """Send JPEG image bytes using protocol with ACK handshake.

    Protocol: <IMG_START><SIZE>[4-byte LE uint32]</SIZE>[data chunks with ACK]<IMG_END>
    """
    # Get image size
    file_size = len(jpeg_bytes)

    if file_size == 0:
        print("ERROR: empty JPEG data")
        return False

    print("=== Starting image transfer ===")
    print("JPEG size: {} bytes".format(file_size))

    # Build header in one buffer
    try:
        header = b"<IMG_START><SIZE>" + struct.pack("<I", file_size) + b"</SIZE>"
    except Exception as e:
        print("ERROR: building header:", e)
        return False

    # Send header
    if not write_all(uart, header):
        print("ERROR: header write failed/timeout")
        blink_led(red, 200)
        return False

    # Wait for ACK after header
    ack = wait_for_ack(total_timeout_ms=3000)
    if not ack:
        print("ERROR: No ACK after header")
        blink_led(red, 300)
        return False

    # Send image data in chunks with ACK handshake
    chunk_size = 64  # relatively small - adjust if necessary
    bytes_sent = 0
    offset = 0

    try:
        while offset < file_size:
            # Python slicing handles case where end_index exceeds the list length. It just returns the remaining items.
            chunk = jpeg_bytes[offset : offset + chunk_size]
            if not chunk:
                break
            # write and ensure all bytes go out
            if not write_all(uart, chunk):
                print("ERROR: chunk write timeout")
                blink_led(red, 300)
                return False
            bytes_sent += len(chunk)
            offset += len(chunk)
            # Wait for ACK after each chunk (or at least make sure receiver is ready)
            ack = wait_for_ack(total_timeout_ms=2000)
            if not ack:
                print("ERROR: No ACK after chunk (bytes_sent={})".format(bytes_sent))
                blink_led(red, 300)
                return False
    except Exception as e:
        print("ERROR: sending JPEG data:", e)
        blink_led(red, 300)
        return False

    # Send footer
    if not write_all(uart, b"<IMG_END>"):
        print("ERROR: footer write failed")
        blink_led(red, 300)
        return False

    # Final ACK
    if wait_for_ack(total_timeout_ms=3000):
        # flash green LED to show success
        for _ in range(3):
            blink_led(green, 100)
            time.sleep_ms(100)  # type: ignore[attr-defined]
        return True
    else:
        print("ERROR: No final ACK")
        blink_led(red, 300)
        return False


def ping_handler() -> bool:
    """Respond to ping command with pong message."""
    try:
        # Send pong response over UART
        response = b"PONG\n"
        if write_all(uart, response):
            print("Ping received, sent PONG")
            # Quick green LED flash to indicate ping response
            blink_led(green, duration_ms=50)
            return True
        else:
            print("ERROR: Failed to send PONG response")
            return False
    except Exception as e:
        print("ERROR in ping_handler():", e)
        return False


def snap_handler() -> None:
    """Capture JPEG image in memory and send over UART."""
    blue.off()
    global state
    if state != STATE_IDLE:
        print("WARNING: snap command received while not idle. Ignoring.")
        return

    state = STATE_CAPTURE

    try:
        # small pause to ensure camera ready
        time.sleep_ms(50)  # type: ignore[attr-defined]
        img = sensor.snapshot()

        jpeg_params = {"quality": QUALITY, "encode_for_ide": False}

        print("Snapping with quality: {}".format(QUALITY))

        # Try to convert to JPEG, with fallback to lower quality if needed
        jpeg_bytes = None
        try:
            jpeg_bytes = img.to_jpeg(**jpeg_params).bytearray()
        except Exception as e:
            error_msg = str(e)
            # If frame buffer error and we're using high quality, try lower quality
            if "frame buffer" in error_msg.lower() and QUALITY > 50:
                print("WARNING: High quality failed, trying lower quality (50)...")
                try:
                    jpeg_params["quality"] = 50
                    jpeg_bytes = img.to_jpeg(**jpeg_params).bytearray()
                except Exception as e2:
                    print("ERROR: Failed to get JPEG data even at lower quality:", e2)
                    blink_led(red, 200)
                    state = STATE_ERROR
                    return
            else:
                print("ERROR: Failed to get JPEG data from image:", e)
                blink_led(red, 200)
                state = STATE_ERROR
                return

        if not jpeg_bytes or len(jpeg_bytes) == 0:
            print("ERROR: Failed to get JPEG data from image")
            blink_led(red, 200)
            state = STATE_ERROR
            return

        print("Captured JPEG: {} bytes".format(len(jpeg_bytes)))

        state = STATE_SEND
        success = send_image_protocol(jpeg_bytes)

        # Log result
        if success:
            print("SUCCESS: JPEG sent ({} bytes)".format(len(jpeg_bytes)))
            state = STATE_IDLE
        else:
            print("FAILED: JPEG transfer failed ({} bytes)".format(len(jpeg_bytes)))
            state = STATE_ERROR

    except Exception as e:
        print("ERROR in snap():", e)
        blink_led(red, 200)
        state = STATE_ERROR


# --- Main Loop ---

# Brief startup logging (no noisy startup protocol messages)
print("\n=== Camera Started ===")

# Flush any leftover UART data from previous runs (defensive)
try:
    while uart.any():
        uart.read()
except Exception:
    pass

time.sleep_ms(100)  # type: ignore[attr-defined]
print("Ready for commands...")

heartbeat_ms = 1000
last_hb = time.ticks_ms()  # type: ignore[attr-defined]

COMMANDS = {
    "snap": snap_handler,
    "ping": ping_handler,
}

DEBUG = False

if DEBUG:
    from pyb import USB_VCP

    try:
        usb = USB_VCP()
        usb.setinterrupt(-1)
    except Exception as e:
        print(f"Error using usb: {e}")


while True:
    # Read line (non-blocking-ish due to UART timeout param)
    if state == STATE_IDLE:
        try:
            msg = uart.readline()
            if DEBUG:
                msg = usb.readline()
        except Exception:
            msg = None

        if msg:
            # try to decode, but allow bytes processing if decode fails
            try:
                text = msg.decode("utf-8", "ignore").strip()
                text = text.replace("\x00", "").strip()
            except Exception:
                text = ""

            if text:
                # blink green LED for received command
                blink_led(green, 80)
                print("Command received: '{}'".format(text))

                # Commands
                cmd = text.lower()
                handler = COMMANDS.get(cmd)
                if handler is None:
                    print("Unknown command: '{}'".format(text))
                    # Unknown commands
                    pass
                else:
                    try:
                        handler()
                    except Exception as e:
                        print("ERROR in handler:", e)
                        blink_led(red, 200)
                        state = STATE_ERROR
                        pass

    elif state == STATE_ERROR:
        # TODO: How should we handle errors?
        print("Recovering from error...")
        time.sleep_ms(500)  # type: ignore[attr-defined]
        state = STATE_IDLE

    # Idle heartbeat
    if time.ticks_diff(time.ticks_ms(), last_hb) >= heartbeat_ms:  # type: ignore[attr-defined]
        try:
            blue.toggle()
        except Exception:
            # some platforms may not have toggle; emulate
            blink_led(blue, 20)
        last_hb = time.ticks_ms()  # type: ignore[attr-defined]

    # yield tiny amount of CPU
    time.sleep_ms(10)  # type: ignore[attr-defined]
