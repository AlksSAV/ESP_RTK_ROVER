"""Probe whether SAVECONFIG is reachable after INTERFACEMODE ... AUTO AUTO ON.

The question the docs leave open: the shipped boot sequence sends SAVECONFIG
after the port has switched to RTCM auto-detect. If the port no longer parses
ASCII at that point, SAVECONFIG is dead code and has to move earlier.

Nothing is persisted here - SAVECONFIG is deliberately NOT trusted to have
landed. If it did land, power-cycling the receiver restores the saved state
anyway, and the test leaves the port in COMPASSNAV.

Usage: python tools/test_saveconfig.py COM13
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
    return buf


def send(ser, cmd, wait=1.5):
    drain(ser)
    ser.write(cmd.encode("ascii") + b"\r\n")
    ser.flush()
    out = read_for(ser, wait)
    print(f"[>] {cmd}")
    if out:
        print(out.decode("ascii", "replace").rstrip())
    else:
        print("[<] (silence)")
    print()
    return out


def verdict(label, out, expect_reply):
    got = bool(out.strip())
    ok = got == expect_reply
    print(f"    {'PASS' if ok else 'FAIL'}: {label} - "
          f"{'answered' if got else 'silent'}, expected "
          f"{'a reply' if expect_reply else 'silence'}")
    return ok


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    port = sys.argv[1]

    ser = serial.Serial(port, BAUD, timeout=0.05)
    print(f"opened {port} @{BAUD} 8N1\n")
    results = []

    print("=== 1. baseline: port must parse ASCII ===")
    results.append(verdict("LOG VERSION before the switch",
                           send(ser, "LOG VERSION"), True))

    print("=== 2. switch COM1 to differential auto-detect ===")
    send(ser, "INTERFACEMODE COM1 AUTO AUTO ON", 2.0)

    print("=== 3. does the port still parse ASCII? ===")
    results.append(verdict("LOG VERSION after AUTO AUTO ON",
                           send(ser, "LOG VERSION"), False))

    print("=== 4. the actual question: is SAVECONFIG heard? ===")
    results.append(verdict("SAVECONFIG after AUTO AUTO ON",
                           send(ser, "SAVECONFIG", 2.0), False))

    print("=== 5. escape hatch back to COMPASSNAV ===")
    send(ser, "INTERFACEMODE COMPASS COMPASS ON", 2.0)

    print("=== 6. recovery check ===")
    results.append(verdict("LOG VERSION after the escape hatch",
                           send(ser, "LOG VERSION"), True))

    ser.close()

    print("=" * 60)
    passed = sum(results)
    print(f"{passed}/{len(results)} checks passed")
    if not results[0] or not results[-1]:
        print("WARNING: the port did not come back. Power-cycle the receiver.")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
