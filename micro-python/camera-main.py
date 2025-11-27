# OpenMV Nicla Vision - UART Image Transfer
# Compatible with PayloadHandler component
# Protocol: <IMG_START><SIZE>[4-byte uint32]</SIZE>[JPEG data]<IMG_END>
#
# Notes:
# - This version is defensive about UART writes/reads and checks ACK contents.
# - It attempts to be robust to boot timing and power-related issues.

import sensor, time, uos
from pyb import UART, LED
import struct

# --- UART Setup ---
# Use a per-character timeout (ms) when reading; we'll also use a total-timeout mechanism.
uart = UART("LP1", 115200, timeout=1000)  # 1s inter-char timeout (OpenMV UART)

# --- LEDs ---
red = LED(1)    # error indicator
green = LED(2)  # activity (command received)
blue = LED(3)   # idle heartbeat

# --- STATE MACHINE ---
STATE_IDLE = 0
STATE_CAPTURE = 1
STATE_SEND = 2
STATE_ERROR = 3

# Set global state
state = STATE_IDLE

# --- Camera Setup ---
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
# Keep existing framesize (your original used sensor.HD)
sensor.set_framesize(sensor.HD)
sensor.skip_frames(time=2000)  # allow auto-exposure/whitebalance to settle

# --- Image Folder Setup ---
IMG_FOLDER = "images"
MAX_IMAGES = 10
image_counter = 0
try:
    uos.mkdir(IMG_FOLDER)
except OSError:
    # probably already exists
    pass


# --- Utility functions ---

def write_all(uart_obj, data, write_timeout_ms=2000):
    """
    Ensure all bytes in `data` are written to UART.
    Returns True on success, False on timeout/partial write.
    """
    total = len(data)
    off = 0
    start = time.ticks_ms()
    while off < total:
        written = uart_obj.write(data[off:])  # may return number of bytes written or None
        if written is None:
            written = 0
        off += written
        # If nothing written, check timeout
        if written == 0 and time.ticks_diff(time.ticks_ms(), start) > write_timeout_ms:
            return False
    return True

def next_filename():
    """Generate next sequential filename safely."""
    try:
        files = uos.listdir(IMG_FOLDER)
    except OSError:
        # If folder unexpectedly missing, recreate and start at 1
        try:
            uos.mkdir(IMG_FOLDER)
        except Exception:
            pass
        files = []

    global image_counter
    image_counter = (image_counter % MAX_IMAGES) + 1
    return "{}/img_{:04d}.jpg".format(IMG_FOLDER, image_counter)

def is_ack(ack_bytes):
    """
    Decide whether the received bytes qualify as an ACK.
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

def wait_for_ack(total_timeout_ms=3000):
    """
    Wait for an ACK line from UART up to total_timeout_ms.
    Returns the received bytes (non-empty) on success, or None on timeout.
    """
    deadline = time.ticks_add(time.ticks_ms(), total_timeout_ms)
    # We will use uart.readline() which returns bytes up to newline (or None on timeout)
    while time.ticks_diff(deadline, time.ticks_ms()) > 0:
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
        time.sleep_ms(10)
    # timed out
    return None

def send_image_protocol(filename):
    """
    Send image using protocol with ACK handshake:
    <IMG_START><SIZE>[4-byte LE uint32]</SIZE>[data chunks with ACK]<IMG_END>
    """
    # Get file size
    try:
        stat = uos.stat(filename)
        file_size = stat[6]  # st_size in uos.stat tuple
    except Exception as e:
        print("ERROR: cannot stat {}: {}".format(filename, e))
        return False

    print("=== Starting image transfer ===")
    print("File: {}, size: {} bytes".format(filename, file_size))

    # Build header in one buffer
    try:
        header = b"<IMG_START><SIZE>" + struct.pack("<I", file_size) + b"</SIZE>"
    except Exception as e:
        print("ERROR: building header:", e)
        return False

    # Send header
    if not write_all(uart, header):
        print("ERROR: header write failed/timeout")
        red.on(); time.sleep_ms(200); red.off()
        return False

    # Wait for ACK after header
    ack = wait_for_ack(total_timeout_ms=3000)
    if not ack:
        print("ERROR: No ACK after header")
        red.on(); time.sleep_ms(300); red.off()
        return False

    # Send image data in chunks with ACK handshake
    chunk_size = 64  # relatively small - adjust if necessary
    bytes_sent = 0

    try:
        with open(filename, "rb") as f:
            while True:
                chunk = f.read(chunk_size)
                if not chunk:
                    break
                # write and ensure all bytes go out
                if not write_all(uart, chunk):
                    print("ERROR: chunk write timeout")
                    red.on(); time.sleep_ms(300); red.off()
                    return False
                bytes_sent += len(chunk)
                # Wait for ACK after each chunk (or at least make sure receiver is ready)
                ack = wait_for_ack(total_timeout_ms=2000)
                if not ack:
                    print("ERROR: No ACK after chunk (bytes_sent={})".format(bytes_sent))
                    red.on(); time.sleep_ms(300); red.off()
                    return False
    except Exception as e:
        print("ERROR: reading/sending file:", e)
        red.on(); time.sleep_ms(300); red.off()
        return False

    # Send footer
    if not write_all(uart, b"<IMG_END>"):
        print("ERROR: footer write failed")
        red.on(); time.sleep_ms(300); red.off()
        return False

    # Final ACK
    if wait_for_ack(total_timeout_ms=3000):
        # flash green LED to show success
        for _ in range(3):
            green.on(); time.sleep_ms(100); green.off(); time.sleep_ms(100)
        return True
    else:
        print("ERROR: No final ACK")
        red.on(); time.sleep_ms(300); red.off()
        return False

def snap_handler():
    """Capture image, save to storage, and send over UART"""
    global state
    if state != STATE_IDLE:
        print("WARNING: snap command received while not idle. Ignoring.")
        return
    
    state = STATE_CAPTURE
    
    try:
        # small pause to ensure camera ready
        time.sleep_ms(50)
        img = sensor.snapshot()

        filename = next_filename()
        # save returns None on success; wrap in try
        img.save(filename)
        print("Saved:", filename)

        state = STATE_SEND
        success = send_image_protocol(filename)

        # Log result
        if success:
            try:
                sz = uos.stat(filename)[6]
            except Exception:
                sz = "?"
            print("SUCCESS: {} sent ({} bytes)".format(filename, sz))
            state = STATE_IDLE
        else:
            print("FAILED: {} - transfer failed".format(filename))
            state = STATE_ERROR

    except Exception as e:
        print("ERROR in snap():", e)
        red.on(); time.sleep_ms(200); red.off()
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

time.sleep_ms(100)
print("Ready for commands...")

heartbeat_ms = 1000
last_hb = time.ticks_ms()

COMMANDS = {
    "snap" : snap_handler
    # TODO: Add more commands?
}

while True:
    # Read line (non-blocking-ish due to UART timeout param)
    if state == STATE_IDLE:
        try:
            msg = uart.readline()
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
                green.on(); time.sleep_ms(80); green.off()
                print("Command received: '{}'".format(text))

                # Commands
                cmd = text.lower()
                handler = COMMANDS.get(cmd)
                if handler == None:
                    # Unknown commands
                    pass
                else:
                    handler()

    elif state == STATE_ERROR:
        # TODO: How should we handle errors?
        print("Recovering from error...")
        time.sleep_ms(500)
        state = STATE_IDLE

    # Idle heartbeat
    if time.ticks_diff(time.ticks_ms(), last_hb) >= heartbeat_ms:
        try:
            blue.toggle()
        except Exception:
            # some platforms may not have toggle; emulate
            blue.on(); time.sleep_ms(20); blue.off()
        last_hb = time.ticks_ms()

    # yield tiny amount of CPU
    time.sleep_ms(10)

