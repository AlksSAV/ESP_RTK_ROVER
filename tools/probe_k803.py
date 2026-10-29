"""Minimal ComNav K803 ASCII console over a serial port.

Usage:
    python tools/probe_k803.py COM17                  # listen, then LOG VERSION
    python tools/probe_k803.py COM17 "LOG VERSION"    # send specific commands

Requires pyserial. The receiver's UART is 3.3 V, 115200 8N1, CR+LF.
"""
import sys
import time

import serial


def read_for(ser, seconds):
    deadline = time.time() + seconds
    buf = b""
    while time.time() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf += chunk
        else:
            time.sleep(0.005)
    return buf


def send(ser, cmd, wait=1.0):
    while ser.in_waiting:
        ser.read(ser.in_waiting)
    ser.write(cmd.encode("ascii") + b"\r\n")
    ser.flush()
    return read_for(ser, wait)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    port = sys.argv[1]
    cmds = sys.argv[2:]

    ser = serial.Serial(port, 115200, timeout=0.05)
    print(f"opened {port} @115200 8N1")

    if not cmds:
        print("--- passive listen, 3 s ---")
        data = read_for(ser, 3.0)
        print(f"{len(data)} bytes")
        if data:
            print(data.decode("ascii", "replace"))

        print("--- INTERFACEMODE COMPASS COMPASS ON ---")
        out = send(ser, "INTERFACEMODE COMPASS COMPASS ON", 1.0)
        print(out.decode("ascii", "replace") if out else "(no response)")

        print("--- LOG VERSION ---")
        out = send(ser, "LOG VERSION", 2.0)
        print(out.decode("ascii", "replace") if out else "(no response)")
    else:
        send(ser, "INTERFACEMODE COMPASS COMPASS ON", 0.8)
        for c in cmds:
            print(f"--- {c} ---")
            out = send(ser, c, 2.0)
            print(out.decode("ascii", "replace") if out else "(no response)")

    ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
