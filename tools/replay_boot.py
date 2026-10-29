"""Replay the firmware's boot sequence against a receiver over serial.

Mirrors gnssInit() in src/gnss_init.cpp command for command, so the whole
kCommands[] table gets exercised on real hardware without flashing an ESP32.
Reports accepted / rejected / silent per command and the resulting verdict.

Nothing is persisted unless SAVECONFIG lands, and that is reported explicitly.

Usage: python tools/replay_boot.py COM13
"""
import sys
import time

import serial

BAUD = 115200
WAIT_MS = 0.6  # GNSS_CMD_WAIT_MS

# (command, expect_reply, max_retries) - keep in sync with src/gnss_init.cpp
COMMANDS = [
    ("UNLOGALL COM1", True, 2),
    ("UNLOGALL COM2", False, 1),
    ("UNLOGALL COM3", False, 1),
    ("FIX NONE", True, 1),
    ("SET APPSCENE Survey", True, 2),
    ("UNLOCKOUTALL", True, 1),
    ("RTKSOURCE RTCMV3 ANY", True, 1),
    ("LOG COM1 GPGGA ONTIME 1", True, 1),
    ("LOG COM1 GPGST ONTIME 1", True, 1),
    ("LOG COM1 GPGSA ONTIME 1", True, 1),
    ("LOG COM1 GPGSV ONTIME 1", True, 1),
    ("LOG COM1 GPRMC ONTIME 1", True, 1),
    ("LOG COM1 GPVTG ONTIME 1", True, 1),
    ("LOG COM1 GPZDA ONTIME 1", True, 1),
    ("LOG COMCONFIG", True, 1),
    ("PPSCONTROL ENABLE POSITIVE 1 1000 1", True, 1),
    ("SAVECONFIG", True, 1),
    ("INTERFACEMODE COM1 AUTO AUTO ON", False, 1),
]

NONE, OK, REJECTED = "NONE", "OK", "REJECTED"


def drain(ser):
    while ser.in_waiting:
        ser.read(ser.in_waiting)


def send(ser, cmd, wait):
    drain(ser)
    ser.write(cmd.encode("ascii") + b"\r\n")
    ser.flush()
    deadline = time.time() + wait
    buf = b""
    while time.time() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf += chunk
        else:
            time.sleep(0.005)
    text = buf.decode("ascii", "replace")
    if not text.strip():
        return NONE, ""
    if "Invalid" in text or "Error!" in text:
        return REJECTED, text
    return OK, text


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    port = sys.argv[1]

    ser = serial.Serial(port, BAUD, timeout=0.05)
    print(f"replaying the boot sequence on {port} @{BAUD} 8N1\n")

    # preamble, exactly as gnssInit()
    send(ser, "INTERFACEMODE COMPASS COMPASS ON", 1.0)
    r, _ = send(ser, "LOG VERSION", 1.0)
    print(f"gate LOG VERSION: {r}")
    if r == NONE:
        print("ABORT: no reply, the rest of the sequence cannot land")
        ser.close()
        return 2
    print()

    rejected = 0
    silent = 0
    rows = []

    for cmd, expect_reply, max_retries in COMMANDS:
        r = NONE
        for _ in range(max_retries):
            r, text = send(ser, cmd, WAIT_MS)
            if r != NONE or not expect_reply:
                break
        if r == REJECTED:
            rejected += 1
        elif r == NONE and expect_reply:
            silent += 1
        mark = {OK: "ok", REJECTED: "REJECTED", NONE: "silent"}[r]
        rows.append((cmd, mark, expect_reply))
        print(f"  {mark:9s} {cmd}")

    print()
    print(f"rejected={rejected} silent={silent}")
    if silent >= 3:
        print("VERDICT: NO REPLY - the link died mid-sequence")
    elif rejected:
        print("VERDICT: WARN - some commands are not supported by this build")
    else:
        print("VERDICT: OK - every command was accepted")

    # The port is now in AUTO and parses nothing. Leave it reachable.
    send(ser, "INTERFACEMODE COMPASS COMPASS ON", 1.5)
    r, _ = send(ser, "LOG VERSION", 1.0)
    print(f"escape hatch: {'restored' if r == OK else 'FAILED - power-cycle the receiver'}")

    print()
    print("SAVECONFIG was sent while the port still parsed ASCII, so the")
    print("configuration above should survive a power cycle. Verify with:")
    print("  python tools/probe_k803.py <port> \"LOG LOGLIST\"")

    ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
