"""Read the receiver's own spectrum scanner over the antenna input.

The last instrument available without a multimeter: SCANSPECTRUM sweeps the RF
front end and SPECTRUM (log ID 2260) reports the level at each step. A live
antenna shows an elevated region where GNSS signals sit; a dead one shows a flat
noise floor.

The log is binary only and must be requested as SPECTRUMB - the plain "SPECTRUM"
name is accepted but emits nothing.

Usage: python tools/scan_spectrum.py COM13 [center_khz] [range_khz] [points]
"""
import struct
import sys
import time

import serial

BAUD = 115200
LOG_ID_SPECTRUM = 2260
SYNC = b"\xaa\x44\x12"


def drain(ser):
    while ser.in_waiting:
        ser.read(ser.in_waiting)


def send(ser, cmd, wait=1.2):
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
    first = text.strip().splitlines()[0] if text.strip() else "(silence)"
    if "Invalid" in text or "Error!" in text:
        first = f"REJECTED: {first}"
    return first


def read_binary(ser, seconds):
    deadline = time.time() + seconds
    buf = bytearray()
    while time.time() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf += chunk
        else:
            time.sleep(0.005)

    logs = []
    i = 0
    while True:
        i = buf.find(SYNC, i)
        if i < 0 or i + 28 > len(buf):
            break
        header_len = buf[i + 3]
        msg_id, = struct.unpack_from("<H", buf, i + 4)
        msg_len, = struct.unpack_from("<H", buf, i + 8)
        body = bytes(buf[i + header_len:i + header_len + msg_len])
        if msg_id == LOG_ID_SPECTRUM and len(body) >= 20:
            logs.append(body)
        i += header_len + msg_len + 4
    return logs


def parse_spectrum(body):
    cent, rng, times = struct.unpack_from("<iii", body, 0)
    start, step = struct.unpack_from("<ii", body, 12)
    n = max(0, min(times, (len(body) - 20) // 2))
    values = struct.unpack_from(f"<{n}H", body, 20) if n else ()
    return cent, rng, times, start, step, values


def run(ser, label, center, rng, points, sweep_s=5.0):
    print(f"\n=== {label} ===")
    print(f"    SCANSPECTRUM {center} {rng} {points} -> "
          f"{send(ser, f'SCANSPECTRUM {center} {rng} {points}', 2.0)}")
    print(f"    LOG COM1 SPECTRUMB ONTIME 1   -> "
          f"{send(ser, 'LOG COM1 SPECTRUMB ONTIME 1', 1.5)}")

    logs = read_binary(ser, sweep_s)
    send(ser, "UNLOG COM1 SPECTRUMB", 1.0)

    if not logs:
        print("    no SPECTRUM log received")
        return None

    cent, rng, times, start, step, values = parse_spectrum(logs[-1])
    print(f"    center {cent} kHz · range {rng} kHz · {len(values)} points "
          f"· start {start} kHz · step {step} kHz")

    lo, hi = min(values), max(values)
    mean = sum(values) / len(values)
    var = sum((v - mean) ** 2 for v in values) / len(values)
    print(f"    min {lo} · max {hi} · mean {mean:.0f} · stddev {var ** 0.5:.1f} "
          f"· dynamic range {hi - lo}")

    n = len(values)
    buckets = min(40, n)
    print("    sweep profile (left = low frequency):")
    for b in range(buckets):
        a = b * n // buckets
        z = max(a + 1, (b + 1) * n // buckets)
        v = sum(values[a:z]) / (z - a)
        bar = "#" * int(round(v / max(1, hi) * 55))
        print(f"      {b:2d}  {v:6.0f}  {bar}")
    return cent, values, lo, hi, mean


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    port = sys.argv[1]
    center = int(sys.argv[2]) if len(sys.argv) > 2 else 1575420
    rng = int(sys.argv[3]) if len(sys.argv) > 3 else 4000
    points = int(sys.argv[4]) if len(sys.argv) > 4 else 100

    ser = serial.Serial(port, BAUD, timeout=0.05)
    print(f"{port} @{BAUD} 8N1")
    print(f"port: {send(ser, 'INTERFACEMODE COMPASS COMPASS ON', 1.0)}")
    send(ser, "UNLOGALL COM1", 1.2)

    l1 = run(ser, "L1 1575.42 MHz, +/- 2 MHz", center, rng, points)
    l5 = run(ser, "L5 1176.45 MHz, +/- 2 MHz", 1176450, rng, points)

    print()
    print("=" * 62)
    if l1 and l5:
        print(f"L1 mean {l1[4]:.0f}, max {l1[3]}")
        print(f"L5 mean {l5[4]:.0f}, max {l5[3]}")
        print()
        print("Interpretation: an active antenna raises the whole floor")
        print("(LNA gain) and shows structure where GNSS signals are. A")
        print("flat profile at the noise minimum means no RF reaches the")
        print("front end - dead LNA, open cable, or a disconnected centre pin.")

    ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
