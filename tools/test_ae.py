"""Test whether the AE (Antenna Electronics) setting is what starves the antenna.

SYSCONFIG reports `AE : 100M` - a 100 MHz clock fed to the antenna electronics.
The Beitian BT-800D is a plain active antenna with a DC-biased LNA and no such
clock, so if the board drives AE instead of bias, the LNA never powers up and
the receiver sees zero satellites.

The test steps AE through OFF / 25M / back to 100M and counts satellites in
GPGSV at each step. Nothing is saved: SAVECONFIG is never sent, so a power cycle
restores the previous state.

Usage: python tools/test_ae.py COM13
"""
import re
import sys
import time

import serial

BAUD = 115200
SETTLE_S = 20  # give the receiver time to re-acquire after a change


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
    reply = "silent" if not out.strip() else out.strip().splitlines()[0]
    if "Invalid" in out or "Error!" in out:
        reply = f"REJECTED: {reply}"
    return reply


def count_sats(text):
    """Highest 'satellites in view' seen in a GPGSV burst."""
    best = 0
    for line in text.splitlines():
        if "GPGSV" not in line:
            continue
        f = line.split("*")[0].split(",")
        if len(f) > 3:
            try:
                best = max(best, int(f[3]))
            except ValueError:
                pass
    return best


def best_snr(text):
    """Best C/N0 across all GPGSV sentences."""
    best = 0
    for line in text.splitlines():
        if "GPGSV" not in line:
            continue
        f = line.split("*")[0].split(",")
        # after the 4-field header, groups of 4: prn, elev, azim, snr
        for i in range(4, len(f) - 3, 4):
            try:
                best = max(best, int(f[i + 3]))
            except (ValueError, IndexError):
                pass
    return best


def sample(ser, label, seconds=SETTLE_S):
    text = read_for(ser, seconds)
    sats = count_sats(text)
    snr = best_snr(text)
    print(f"  {label:28s} sats in view: {sats:2d}   best C/N0: {snr:2d} dB-Hz")
    return sats, snr


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    port = sys.argv[1]

    ser = serial.Serial(port, BAUD, timeout=0.05)
    print(f"{port} @{BAUD} 8N1\n")

    print(f"port: {send(ser, 'INTERFACEMODE COMPASS COMPASS ON', 1.0)}")
    send(ser, "UNLOGALL COM1", 1.2)
    print(f"GPGSV log: {send(ser, 'LOG COM1 GPGSV ONTIME 1', 1.2)}")
    print()

    print(f"baseline, AE as configured (100M), {SETTLE_S}s:")
    base = sample(ser, "AE 100M (current)")

    print(f"\nSET AE OFF -> {send(ser, 'SET AE OFF', 1.5)}")
    off = sample(ser, "AE OFF")

    print(f"\nSET AE 25M -> {send(ser, 'SET AE 25M', 1.5)}")
    m25 = sample(ser, "AE 25M")

    print(f"\nrestore: SET AE 100M -> {send(ser, 'SET AE 100M', 1.5)}")
    sample(ser, "AE 100M (restored)")

    print()
    print("=" * 62)
    if max(off[0], m25[0]) > base[0] or max(off[1], m25[1]) > base[1]:
        print("AE IS THE CULPRIT - satellites appeared with a different setting.")
    else:
        print("AE makes no difference. The antenna is not the AE clock type,")
        print("or the LNA has no power at all - measure the connector next.")
    print("Nothing was saved; a power cycle restores the previous state.")

    ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
