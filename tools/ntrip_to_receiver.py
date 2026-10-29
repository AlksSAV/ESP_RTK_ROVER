"""Feed live NTRIP RTCM3 into the K803 over UART and watch RTK lock.

This is the end-to-end test for the correction path: the same bytes the phone
sends through the ESP32 bridge, pushed straight into the receiver's COM1 while
the port sits in AUTO auto-detect. If GGA quality goes 1 -> 5 -> 4, the whole
premise of the rover works, with no ESP32 and no phone involved.

Credentials come from a local file (git-ignored) and are never printed.

Usage: python tools/ntrip_to_receiver.py COM17 [mountpoint]
       reads host/port/login/pass from .ntrip in the project root
"""
import base64
import json
import socket
import sys
import threading
import time

import serial

BAUD = 115200
CFG = ".ntrip"


def load_cfg():
    cfg = {}
    for line in open(CFG, encoding="utf-8"):
        parts = line.split(None, 1)
        if len(parts) == 2:
            cfg[parts[0].lower()] = parts[1].strip()
    for key in ("host", "port", "login", "pass"):
        if key not in cfg:
            sys.exit(f"{CFG} is missing the '{key}' line")
    return cfg


class Stop(threading.Event):
    pass


def ntrip_stream(host, port, mount, login, pw, stop, stats):
    """Yield RTCM bytes from the caster; runs in a thread, writes to UART."""
    auth = base64.b64encode(f"{login}:{pw}".encode()).decode()
    req = (
        f"GET /{mount} HTTP/1.1\r\n"
        f"Host: {host}:{port}\r\n"
        "User-Agent: ESP-RTK-ROVER-bench/1.0 (NTRIP caster client)\r\n"
        f"Authorization: Basic {auth}\r\n"
        "Accept: */*\r\n"
        "Ntrip-Version: Ntrip/2.0\r\n"
        "\r\n"
    )
    s = socket.create_connection((host, port), timeout=15)
    s.sendall(req.encode())
    head = b""
    while b"\r\n\r\n" not in head:
        chunk = s.recv(4096)
        if not chunk:
            raise RuntimeError("caster closed the connection during handshake")
        head += chunk
    status = head.split(b"\r\n", 1)[0].decode("latin1")
    print(f"  caster: {status}")
    if "200" not in status:
        s.close()
        raise RuntimeError(f"caster refused mountpoint: {status}")

    s.settimeout(1.0)
    try:
        while not stop.is_set():
            try:
                data = s.recv(4096)
            except socket.timeout:
                continue
            if not data:
                print("  caster closed the stream")
                break
            stats["bytes"] += len(data)
            yield data
    finally:
        s.close()


def serial_read(ser, stop, state):
    """Drain the receiver and keep only the sentences we care about."""
    buf = ""
    while not stop.is_set():
        chunk = ser.read(ser.in_waiting or 1)
        if not chunk:
            time.sleep(0.005)
            continue
        buf += chunk.decode("ascii", "replace")
        while "\n" in buf:
            line, buf = buf.split("\n", 1)
            line = line.strip()
            if not line:
                continue
            state["raw"].append(line)
            if len(state["raw"]) > 4000:
                state["raw"].pop(0)
            if line.startswith("$") and line[3:6] in ("GGA", "GST", "GSA"):
                if line[3:6] == "GGA":
                    state["last"] = line
                    state["gga"].append(line)
                    if len(state["gga"]) > 600:
                        state["gga"].pop(0)
                else:
                    state["last_other"] = line


def parse_gga(line):
    f = line.split("*")[0].split(",")
    if len(f) < 15:
        return None
    return {
        "utc": f[1],
        "lat": f[2],
        "lon": f[4],
        "qual": f[6],
        "sats": f[7],
        "hdop": f[8],
        "alt": f[9],
        "age": f[13],
        "refid": f[14],
    }


QUAL = {"0": "NO FIX", "1": "SINGLE", "2": "DGPS/SBAS", "4": "RTK FIXED",
        "5": "RTK FLOAT", "6": "GNSS/INS"}


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    port = sys.argv[1]
    mount = sys.argv[2] if len(sys.argv) > 2 else "2307"

    cfg = load_cfg()
    print(f"caster {cfg['host']}:{cfg['port']}  mount {mount}  "
          f"login {cfg['login'][:2]}***")

    ser = serial.Serial(port, BAUD, timeout=0.05)

    def send(cmd, wait=1.2):
        while ser.in_waiting:
            ser.read(ser.in_waiting)
        ser.write(cmd.encode("ascii") + b"\r\n")
        ser.flush()
        time.sleep(wait)

    print("\n== 1. put COM1 in differential auto-detect ==")
    send("INTERFACEMODE COMPASS COMPASS ON", 1.0)
    send("UNLOGALL COM1", 1.2)
    for s in ("GPGGA", "GPGST", "GPGSA", "GPGSV"):
        send(f"LOG COM1 {s} ONTIME 1", 0.6)
    print("  NMEA logs re-enabled, switching port to AUTO...")
    send("INTERFACEMODE COM1 AUTO AUTO ON", 1.5)

    state = {"raw": [], "gga": [], "last": None}
    stats = {"bytes": 0}
    stop = Stop()

    reader = threading.Thread(target=serial_read, args=(ser, stop, state), daemon=True)
    reader.start()

    print("\n== 2. stream RTCM from the caster into the UART ==")
    ntrip = ntrip_stream(cfg["host"], int(cfg["port"]), mount,
                         cfg["login"], cfg["pass"], stop, stats)

    t0 = time.time()
    last_report = 0
    qualities = {}
    try:
        for data in ntrip:
            ser.write(data)
            now = time.time()
            if now - last_report >= 5.0:
                last_report = now
                g = parse_gga(state["last"]) if state["last"] else None
                rate = stats["bytes"] / max(1e-9, now - t0)
                if g:
                    qualities[g["qual"]] = qualities.get(g["qual"], 0) + 1
                    print(f"  t+{now-t0:4.0f}s  {rate:7.0f} B/s in   "
                          f"qual {g['qual']} ({QUAL.get(g['qual'], '?')})  "
                          f"sats {g['sats']:>2s}  hdop {g['hdop']:>4s}  "
                          f"age {g['age']!r}  ref {g['refid']!r}")
                else:
                    print(f"  t+{now-t0:4.0f}s  {rate:7.0f} B/s in   (no GGA yet)")
            if now - t0 > 180:
                break
    except KeyboardInterrupt:
        print("\n  interrupted")
    except Exception as e:
        print(f"\n  ERROR: {type(e).__name__}: {e}")

    stop.set()
    time.sleep(0.3)

    print("\n== 3. result ==")
    print(f"  RTCM pushed: {stats['bytes']} bytes "
          f"({stats['bytes']/max(1e-9, time.time()-t0):.0f} B/s)")
    print(f"  fix qualities observed: "
          f"{ {QUAL.get(k, k): v for k, v in sorted(qualities.items())} }")
    if state["gga"]:
        print("  last GGA:", state["gga"][-1])
    gst = [l for l in state["raw"] if l[3:6] == "GST"]
    if gst:
        print("  last GST:", gst[-1])

    print("\n== 4. back to the command console ==")
    send("INTERFACEMODE COMPASS COMPASS ON", 1.0)
    print("  ", parse_gga(state["gga"][-1])["qual"] if state["gga"] else "?",
          "- port restored to COMPASSNAV")
    ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
