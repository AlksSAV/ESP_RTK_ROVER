# ESP_RTK_ROVER

**A DIY centimetre-accurate GNSS rover.**

<p align="center">
  <b><a href="install/">⚡ Flash in your browser</a></b> ·
  <b><a href="web/rtk-monitor.html">🛰️ Live Dashboard</a></b> ·
  <a href="HARDWARE.md">🔧 Hardware</a> ·
  <a href="COMMANDS.md">📡 K803 Commands</a> ·
  <a href="FLASHING.md">📖 Flashing</a>
</p>

---

## What it is

A **ComNav SinoGNSS K803** OEM board: a multi-frequency GNSS receiver with a real RTK engine,
the size of a stamp, sold as a component for survey equipment.

This project wires it to a **Seeed Studio XIAO ESP32C6** and an external
**Beitian BT-800D** antenna, and turns the combination into a BLE RTK rover that any mobile GIS
app can talk to.

```
  phone  (NTRIP client + GIS app)
    │   RTCM3 down  ·  NMEA up
    │   Bluetooth LE
  XIAO ESP32C6
    │   UART 115200
  ComNav K803  ──  external antenna on the pole
```

The firmware:

- **puts the K803 into a known rover configuration on every boot** — no laptop before a survey
- **passes bytes both ways, transparently** — NMEA up, RTCM3 corrections down
- **speaks BLE** (Nordic UART Service, works on iOS and Android)
- **can also broadcast a Wi-Fi AP** with a WebSocket that serves the same dashboard in any
  browser (Safari on iPhone included)
- **ships a browser dashboard** for live diagnostics

---

## ⚠️ This is a DIY device, not a survey instrument

Read this before using it for anything that matters.

- **No calibration of your own.** The BT-800D has published phase-centre figures, but the
  receiver does not apply them: heights carry a constant offset (the antenna's own phase
  centre plus whatever is between the mount and the ground). It is the same offset on every
  point, so relative work is fine and absolute height is not verified.
- **No certification.** No type approval, no conformity assessment, no traceability to a
  national standard.
- **No integrity monitoring.** Commercial receivers add interference detection, multipath
  mitigation and integrity checking. This has none. **A false fix looks exactly like a good
  one.**
- **No support.** If it gives you a wrong answer, there is nobody to call. That, more than the
  silicon, is what the expensive units are really selling.

**Do not use it for:** cadastral or land-registry work, legal boundaries, construction
setting-out, aviation, marine navigation, or anything where a wrong position hurts someone.

Use at your own risk.

---

## Hardware

| Item |
|---|
| ComNav SinoGNSS **K803** OEM board |
| [Seeed Studio XIAO ESP32C6](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32C6-p-5884.html) |
| **Beitian BT-800D** antenna, on the pole above the electronics |
| USB power bank, wire, heat-shrink |

Four wires between the receiver and the ESP32:

| K803 board | XIAO ESP32C6 | Note |
|---|---|---|
| **GNSS TX** | `D7` / GPIO17 | ESP32 RX ← receiver TX |
| **GNSS RX** | `D6` / GPIO16 | ESP32 TX → receiver RX |
| **GND** | `GND` | Mandatory common ground |
| **5 V** | `5V` | VBUS passthrough from USB-C |

TX and RX cross over. If you get no data, swap those two first.

> ⚠️ **Power the receiver properly.** It pulls 150–300 mA, plus the antenna's LNA. Feeding it
> from a 3.3 V regulator causes boot loops that look like corrupted firmware.
> See [HARDWARE.md → Power](HARDWARE.md#power).

> ⚠️ **You own the antenna now.** With an OEM board and a separate antenna there is no
> designer hiding your mistakes: check the antenna's frequency coverage, whether it is active
> and what voltage its LNA wants, and give it a real ground plane.
> See [HARDWARE.md → Antenna](HARDWARE.md#antenna).

Pinout, power, antenna and housing: **[HARDWARE.md](HARDWARE.md)**

---

## Flash it

**XIAO ESP32C6 — one click, no toolchain:**
**[alkssav.github.io/ESP_RTK_ROVER/install](https://alkssav.github.io/ESP_RTK_ROVER/install/)**
— Chrome or Edge on desktop.

```powershell
pio run -t upload    # XIAO ESP32C6, BLE (default env)
pio device monitor   # console @ 115200
```

Details: **[FLASHING.md](FLASHING.md)**

---

## Connect your phone

<p align="center">
  <img src="media/swmaps-connect.jpeg" alt="SW Maps GNSS Connection dialog" width="265">
  &nbsp;&nbsp;
  <img src="media/swmaps-rtk-fix.jpeg" alt="SW Maps showing RTK FIX" width="265">
</p>

In **SW Maps** (Android / iOS):

1. **☰ → Bluetooth GNSS**
2. **Connection Mode**: `Bluetooth LE`
3. Pick `ESP32-RTK-XXXX`
4. **Instrument Model**: `Generic NMEA` — set **Instrument Height** to your pole length
5. **Connect**
6. **☰ → NTRIP Client** → your caster details → **Connect**
7. Wait for `SINGLE` → `FLOAT` → **`RTK FIX`**

Also works with **QField**, **Mergin Maps**, **Mapit GIS**.

Corrections come from your phone, not the ESP32 — the phone already has the internet connection.
Any NTRIP caster serving RTCM 3.x works.

> Note: SW Maps **subtracts** the instrument height from the elevation it stores. Enter your pole
> length and it records the ground point; leave it empty and it records the antenna.

---

## Live dashboard

**[Open the dashboard →](web/rtk-monitor.html)**

Two ways to get data:

1. **BLE (Web Bluetooth).** Chrome or Edge on desktop or Android. Connect to `ESP32-RTK-XXXX`
   and the dashboard auto-pairs via GATT.
2. **Wi-Fi AP.** The rover creates a hotspot `ESP32-RTK` (password `12345678`). Connect to it,
   open `http://192.168.4.1/` — the rover serves the dashboard itself. Works in **any browser**,
   including Safari on iPhone and Chrome on Android.

> **iOS Safari limitation:** Web Bluetooth is not implemented in Safari (or any iOS browser,
> because Apple requires all browsers to use WebKit). The Wi-Fi AP method works on iPhone.

One self-contained HTML file. Decodes the stream live: fix quality, position, σ values, DOPs,
correction age, and a raw NMEA log with checksum errors highlighted. If the receiver is asked for
`LOG COM1 BESTPOSA ONTIME 1`, the dashboard also shows the engine's own verdict — Pos type
(`SINGLE` / `PSRDIFF` / `NARROW_FLOAT` / `NARROW_INT`) and σ straight from the RTK engine. Nothing
is uploaded anywhere — your position stays in your browser.

---

## Documentation

| | |
|---|---|
| **[HARDWARE.md](HARDWARE.md)** | The board, pinout, wiring, power, external antenna, housing |
| **[COMMANDS.md](COMMANDS.md)** | The ComNav K803 command set — base → rover, NMEA rates, recovery |
| **[FLASHING.md](FLASHING.md)** | Web flashing, PlatformIO, esptool, troubleshooting |

Firmware layout: `src/main.cpp` moves bytes, `src/gnss_init.cpp` configures the K803,
`src/link_ble.cpp` implements the BLE transport.

---

## Field test results

| | |
|---|---|
| **Open sky (60 s)** | 17 sats, C/N0 median 33 dB-Hz, max 38 dB-Hz, HDOP 0.8, quality 1 |
| **RTCM path (150 s, 15.4 km)** | 136 epochs SINGLE → 16 epochs RTK FLOAT, GGA age 1.0 s |
| **Caster** | `pidt.net` mount 2307, MSM5 GPS/GLONASS/Galileo/BeiDou |
| **Antenna** | BT-800D without ground plane — C/N0 33–38 vs typical 45–52 |

The whole chain is verified: `INTERFACEMODE AUTO` → `0xD3` auto-detect → CRC-24Q → RTK engine
→ FLOAT. The limiting factor is the antenna's C/N0 without a ground plane, not the firmware.

See **[TODO.md](TODO.md) §11–12** for the full bench logs, the `BESTPOSA` bench tool, and the
open-sky measurement that proved the antenna was the only missing piece.

---

## License

[MIT](LICENSE) — do what you like, credit appreciated, no warranty.