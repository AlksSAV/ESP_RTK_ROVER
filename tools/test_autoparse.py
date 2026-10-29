"""Disambiguate the SAVECONFIG result.

test_saveconfig.py showed SAVECONFIG silent after INTERFACEMODE ... AUTO AUTO ON,
but silence is only meaningful if the command answers in COMPASSNAV mode. This
probes three things:

  1. Does SAVECONFIG answer at all, in COMPASSNAV mode?
  2. Does the port parse ASCII in AUTO mode? A garbage keyword is the test: a
     live ASCII parser answers "Error! Invalid key word!", a dead one is silent.
  3. Is INTERFACEMODE itself still recognised in AUTO mode? The escape hatch is
     documented to work, which would mean AUTO does not kill all ASCII parsing -
     only the rest of the command set.

Usage: python tools/test_autoparse.py COM13
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


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    port = sys.argv[1]

    ser = serial.Serial(port, BAUD, timeout=0.05)
    print(f"opened {port} @{BAUD} 8N1\n")

    print("=== A. COMPASSNAV: does SAVECONFIG answer at all? ===")
    send(ser, "INTERFACEMODE COMPASS COMPASS ON", 1.0)
    send(ser, "SAVECONFIG", 2.0)

    print("=== B. COMPASSNAV: does a garbage keyword get rejected? ===")
    send(ser, "FOOBARBAZ", 1.5)

    print("=== C. switch to AUTO and repeat the garbage keyword ===")
    send(ser, "INTERFACEMODE COM1 AUTO AUTO ON", 2.0)
    send(ser, "FOOBARBAZ", 1.5)

    print("=== D. in AUTO: is a normal command still parsed? ===")
    send(ser, "LOG VERSION", 2.0)

    print("=== E. back to COMPASSNAV ===")
    send(ser, "INTERFACEMODE COMPASS COMPASS ON", 2.0)
    send(ser, "LOG VERSION", 1.5)

    ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
