"""Lower the elevation mask to 0 and watch GPGSV for a long window.

A receiver that has been off for months has a stale almanac, which slows the
first fix but does NOT hide satellites: acquisition searches PRNs in parallel,
independent of orbital data. So zero satellites in GPGSV is an RF problem, not a
cold-start problem.

This removes the other benign explanation - a 10 degree elevation mask combined
with a balcony that blocks the sky - and then samples for several minutes.
ECUTOFF is not saved, so a power cycle restores it.

Usage: python tools/test_ecutoff.py COM13 [seconds]
"""
import sys
import time

import serial

BAUD = 115200


def drain(ser):
    while ser.in_waiting:
        ser.read(ser.in_waiting)


def read_for(ser, seconds):
    deadline = time.time() + seconds
    buf = b""
    while time.time() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf += chunk
        else:
            time.sleep(0.005)
    return buf.decode("ascii", "replace")


def send(ser, cmd, wait=1.2):
    drain(ser)
    ser.write(cmd.encode("ascii") + b"\r\n")
    ser.flush()
    out = read_for(ser, wait)
    if not out.strip():
        return "silent"
    return out.strip().splitlines()[0]


def parse(text):
    """(max sats in view, best C/N0, last GGA fix quality, last GGA sats used)."""
    max_sats = 0
    best_snr = 0
    quality = None
    used = None
    for line in text.splitlines():
        if "GPGSV" in line:
            f = line.split("*")[0].split(",")
            if len(f) > 3:
                try:
                    max_sats = max(max_sats, int(f[3]))
                except ValueError:
                    pass
            for i in range(4, len(f) - 3, 4):
                try:
                    best_snr = max(best_snr, int(f[i + 3]))
                except (ValueError, IndexError):
                    pass
        elif "GPGGA" in line:
            f = line.split("*")[0].split(",")
            if len(f) > 7:
                try:
                    quality = int(f[6])
                    used = int(f[7])
                except ValueError:
                    pass
    return max_sats, best_snr, quality, used


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    port = sys.argv[1]
    seconds = int(sys.argv[2]) if len(sys.argv) > 2 else 300

    ser = serial.Serial(port, BAUD, timeout=0.05)
    print(f"{port} @{BAUD} 8N1\n")

    print(f"port:    {send(ser, 'INTERFACEMODE COMPASS COMPASS ON', 1.0)}")
    send(ser, "UNLOGALL COM1", 1.2)
    print(f"ECUTOFF: {send(ser, 'ECUTOFF 0', 1.5)}")
    print(f"BD2:     {send(ser, 'BD2ECUTOFF 0', 1.5)}")
    send(ser, "LOG COM1 GPGSV ONTIME 1", 1.2)
    send(ser, "LOG COM1 GPGGA ONTIME 1", 1.2)

    print(f"\nsampling {seconds} s with a 0 degree mask...")
    print(f"{'t':>5}  {'sats in view':>12}  {'best C/N0':>9}  {'GGA qual':>8}  {'used':>4}")
    deadline = time.time() + seconds
    t0 = time.time()
    while time.time() < deadline:
        text = read_for(ser, 30)
        sats, snr, qual, used = parse(text)
        print(f"{int(time.time() - t0):5d}  {sats:12d}  {snr:9d}  "
              f"{str(qual):>8}  {str(used):>4}")

    print()
    print("restoring the 10 degree mask")
    print(f"  ECUTOFF 10    -> {send(ser, 'ECUTOFF 10', 1.2)}")
    print(f"  BD2ECUTOFF 10 -> {send(ser, 'BD2ECUTOFF 10', 1.2)}")
    print("Nothing was saved; a power cycle restores the previous state.")

    ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
