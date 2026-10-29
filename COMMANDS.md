# COMMANDS.md — ComNav K803 Command Reference

Command reference for the **SinoGNSS K803** RTK receiver: the boot sequence used by this
firmware, plus the ASCII command set from OEM Board Reference Manual **V3.1**.

- [How the receiver talks](#how-the-receiver-talks)
- [The point of no return](#the-point-of-no-return)
- [The shipped boot sequence](#the-shipped-boot-sequence)
- [Command reference](#command-reference)
  - [Unlogging](#unlogging)
  - [Port configuration](#port-configuration)
  - [Positioning mode](#positioning-mode)
  - [NMEA output](#nmea-output)
  - [Diagnostics](#diagnostics)
  - [Antenna electronics (`SET AE`)](#antenna-electronics-set-ae)
  - [Spectrum scan](#spectrum-scan)
  - [Interface mode and differential input](#interface-mode-and-differential-input)
  - [Persistence and reset](#persistence-and-reset)
  - [IMU / Dead-reckoning (not used)](#imu--dead-reckoning-not-used)
  - [RTCM relay](#rtcm-relay)
  - [System lockout](#system-lockout)
  - [PPS output](#pps-output)
- [Output rate recipes](#output-rate-recipes)
- [RTCM input](#rtcm-input)
- [Changing the sequence in firmware](#changing-the-sequence-in-firmware)
- [Configuring from a PC](#configuring-from-a-pc)
- [Recovery: when the port stops answering](#recovery-when-the-port-stops-answering)
- [Gotchas](#gotchas)

---

## Status of the information here

| Mark | Meaning |
|---|---|
| ✅ | **Emitted by the shipped boot sequence.** Confirmed accepted by the receiver on this project's board (FW 7.4.6). |
| 🔷 | Documented in the ComNav OEM Board Reference Manual V3.1, but not sent by this firmware. |
| ⚠️ | **Not found in V3.1.** Reported to work on some firmware builds; confirm on your unit before relying on it. |

> **Verified on hardware.** The boot sequence and the commands marked ✅ below were replayed
> against this project's K803 (model `S32352K803`, FW **7.4.6**, build 2025/Dec/30) over
> COM13 at 115200 8N1. All 17 commands were accepted — `rejected=0, silent=0` — and the
> resulting configuration survived a cold power cycle. See [TODO.md](TODO.md) item 11 for the
> full bench log, including the antenna fault that was found along the way.

Command availability still varies with firmware build and **authorization**. A command your
unit does not support is rejected, not silently ignored — check the reply.

> ⚠️ **One command group in V3.1 does not apply to this receiver.** The `insrtk` tilt
> measurement group (§4.6) and the tilt logs of §3.4.8 (`INSRTKCOMPENSATE`, ID 2366) are
> documented for a different board variant. On the K803 they are rejected, and no amount of
> authorization will make them answer. Everything else in this file applies.

> ⚠️ **`OK! Command accepted!` does not mean the log exists.** `LOG TRACKSTATA` answers
> exactly that and then emits nothing, because the keyword parses but the log is not
> implemented in 7.4.6. Trust the output, not the acknowledgement.

---

## How the receiver talks

- **Transport:** COM1, `115200 8N1`, 3.3 V LVTTL.
- **Syntax:** abbreviated ASCII, one command per line, case-insensitive in practice.
- **Terminator:** `\r\n` (CRLF). The firmware appends it; a terminal set to LF-only will hang
  with no reply.
- **No prefix:** commands are sent as plain ASCII without `$` or `*CRC` suffix.
- **Replies** depend on firmware build. Both of these are normal:

  ```
  OK! Command accepted! Port: COM1.
  ```

- **Rejections** contain `Invalid`:

  ```
  Error! Invalid key word!
  ```

  > ⚠️ Two reply formats are in the wild. Older documentation shows
  > `$command,response: OK*<crc>`; newer firmware answers
  > `OK! Command accepted! Port: COMn.` and rejects with `Error! Invalid key word!`.
  > Which one you get depends on your build — read the boot log once and you will know.
  > The firmware keys on `Invalid` and `Error!`, so it copes with both.

- **Logs** are asynchronous. Replies arrive interleaved with whatever the receiver is already
  streaming, so never stop reading at the first newline. The firmware collects for a whole
  settle window (`GNSS_CMD_WAIT_MS`, default 600 ms) instead — see
  [`src/gnss_init.cpp`](src/gnss_init.cpp).

The firmware logs the whole exchange to the USB console:

```
[>] LOG COM1 GPGGA ONTIME 1
[<] OK! Command accepted! Port: COM1.
```

and flags failures:

```
    ^^ REJECTED: SET APPSCENE Survey
```

> `GNSS_CMD_PORT` in `src/config.h` is **the receiver's own port name** (`COM1`), i.e. the
> physical port your ESP32 is wired to. It has nothing to do with a Windows COM port number.

---

## The point of no return

```
INTERFACEMODE COM1 AUTO AUTO ON
```

This puts COM1 into **differential auto-detect**: the port starts interpreting incoming bytes
as RTCM corrections rather than as ASCII commands. That is exactly what an RTK rover needs —
and it means **the port stops accepting commands**.

Consequences:

1. Every `LOG` and every setting must be in place **before** this line. Ordering in the boot
   sequence is not cosmetic.
2. To get the command console back, you must first send the escape hatch — see
   [Recovery](#recovery-when-the-port-stops-answering).

---

## The shipped boot sequence

Run by `gnssInit()` on every boot, before Bluetooth starts. Roughly **12 seconds**:
2 × 1000 ms preamble + 16 × `GNSS_CMD_WAIT_MS` (600 ms).

Running it every boot is deliberate — the receiver is left in a known state no matter what the
previous session or a firmware experiment did to it.

### Preamble

| # | Command | Why | |
|---|---|---|---|
| 1 | `INTERFACEMODE COMPASS COMPASS ON` | Escape hatch. A previous run left COM1 in RTCM auto-detect, where ASCII is ignored; this pulls it back to the ComNav ASCII/binary protocol. On a cold receiver it is a harmless no-op. **Must be first.** | ✅ |
| 2 | `LOG VERSION` | Proves the link works and records model, firmware build and authorization in the boot log. **Gate for the rest of the sequence:** no reply here means the UART is dead and the remaining 16 commands cannot land either, so `gnssInit()` aborts instead of spending ten seconds on timeouts. | ✅ |

### Main sequence

| # | Command | Why | |
|---|---|---|---|
| 3 | `UNLOGALL COM1` | Stop everything the factory base-station config was streaming on COM1. | ✅ |
| 4 | `UNLOGALL COM2` | Same for COM2 — a spare port on a bare OEM board, but a saved base config may still be streaming there. | ✅ |
| 5 | `UNLOGALL COM3` | Same for COM3. | ✅ |
| 6 | `FIX NONE` | **Critical.** Clears the stored base-station coordinate. A base is `FIX POSITION`-ed to a surveyed point and will not compute a rover solution while that is set. | ✅ |
| 7 | `SET APPSCENE Survey` | Select **rover survey** application scenario. Replaces `RTKDYNAMICS FOOT` (not in V3.1). See V3.1 §2.2.24, Table 2-5. | ✅ |
| 8 | `UNLOCKOUTALL` | Re-enable every satellite/constellation the base config may have locked out. | ✅ |
| 9 | `LOG COM1 GPGGA ONTIME 1` | Position, fix quality, sats, HDOP, altitude, **correction age**, base ID. The single most important sentence — NTRIP clients also echo it back to the caster. | ✅ |
| 10 | `LOG COM1 GPGST ONTIME 1` | Position-error statistics: σ latitude / σ longitude / σ altitude. This is where "6 mm" comes from. | ✅ |
| 11 | `LOG COM1 GPGSA ONTIME 1` | Active satellites, PDOP / HDOP / VDOP. | ✅ |
| 12 | `LOG COM1 GPGSV ONTIME 1` | Satellites in view with SNR. Useful for siting; the bulkiest sentence by far. | ✅ |
| 13 | `LOG COM1 GPRMC ONTIME 1` | Recommended minimum: position, speed, course, date. Some apps refuse to start without it. | ✅ |
| 14 | `LOG COM1 GPVTG ONTIME 1` | Course and speed over ground. | ✅ |
| 15 | `LOG COM1 GPZDA ONTIME 1` | UTC date and time. Lets an app set the device clock from GNSS. | ✅ |
| 16 | `LOG COMCONFIG` | Dump the port configuration to the boot log — **while the port still answers**. See V3.1 §3.2.2.1. | ✅ |
| 17 | `SAVECONFIG` | Persist configuration to non-volatile memory. See V3.1 §2.2.22. **Must come before `INTERFACEMODE`** — see the note below. | ✅ |
| 18 | `INTERFACEMODE COM1 AUTO AUTO ON` | Enable differential auto-detect. RTCM3 from the phone now reaches the RTK engine. [Point of no return](#the-point-of-no-return). | ✅ |

> **INS / dead-reckoning is deliberately absent.** An earlier revision of this firmware sent
> `INSCONTROL ENABLE` and `SET SMOOTHEDDR ON`. They are gone: the K803 has an IMU but no tilt
> compensation, so DR adds no accuracy to a stop-and-go pole rover, and once INS engages the
> receiver reports GGA quality `6` (GNSS/INS integrated) instead of an honest `4` (RTK FIXED) —
> which a GIS app accepts as valid. See
> [IMU / Dead-reckoning](#imu--dead-reckoning-not-used) for the commands if you want them.

> **`SAVECONFIG` used to sit after `INTERFACEMODE` and was dead code — now fixed.** Measured on
> FW 7.4.6: once the port is in `AUTO`, it parses **no** ASCII at all. Even a garbage keyword
> draws silence instead of `Error! Invalid key word!`, so nothing sent after `INTERFACEMODE` is
> ever heard. `INTERFACEMODE` itself is the single exception — that is what makes the escape
> hatch work.
>
> `SAVECONFIG` now comes **before** the switch, with `expectReply = true` so the boot log proves
> it landed. It answers in ~400 ms (flash write), the slowest reply in the sequence and still
> comfortably inside `GNSS_CMD_WAIT_MS = 600`. Verified on hardware: after a cold power cycle
> the receiver starts streaming NMEA on its own and `LOG LOGLIST` still lists all seven logs.

After the sequence the firmware drains the echo tail so the first bytes a phone receives are
live NMEA, not a command transcript.

> `LOG COM1 …` targets are written with the literal port name from `GNSS_CMD_PORT`. If your
> ESP32 is wired to COM2, change that one define and the whole sequence follows.

---

## Command reference

### Unlogging

| Command | Effect | |
|---|---|---|
| `UNLOGALL COM1` | Stop all logs on COM1 | ✅ |
| `UNLOGALL` | Stop all logs on **all** ports | 🔷 |
| `UNLOG COM1 GPGSV` | Stop one specific log | 🔷 |

Always unlog before configuring. A factory base station streams RTCM and proprietary
messages continuously; leaving them on wastes bandwidth and confuses NMEA parsers.

### Port configuration

```
COM COM1 115200
    │    └──────  baud: 1200, 4800, 9600, 19200, 38400,
    └───────────  port: COM1~COM5        57600, 115200, 230400, 460800, 921600
```

| Command | Effect | |
|---|---|---|
| `COM COM1 115200` | Set COM1 baud rate. V3.1 §2.2.1 defines only `<port> <baudrate>` — parity, stop bits and handshaking are not settable. | 🔷 |
| `LOG COMCONFIG` | Report the current port settings. See V3.1 §3.2.2.1, Table 3-4 entry #10. | ✅ |

> **Five ports in the log, three on the board.** `LOG COMCONFIG` on FW 7.4.6 lists
> COM1–COM5, all at 115200 8N1. Only COM1–COM3 are physically broken out on this carrier
> board; COM4/COM5 are internal and have no connector. Do not go looking for pads that are
> not there.

> ⚠️ **Changing the baud rate will lock you out.** The receiver switches the moment the
> command is acknowledged; your terminal (or the ESP32) is still on the old rate. If you
> change it *and* save it, you must also change `GNSS_BAUD` in `src/config.h` and reflash,
> or the boot sequence will fail on every subsequent power-up. 115200 is fast enough for
> everything documented here — there is no reason to change it.

### Positioning mode

| Command | Effect | |
|---|---|---|
| `FIX NONE` | Clear any fixed/base coordinate — required for rover operation | ✅ |
| `UNLOCKOUTALL` | Re-enable all satellites previously locked out | ✅ |
| `SET APPSCENE Survey` | Select application scenario: Survey, Robot, Car, Uav, Air, Space. See V3.1 §2.2.24, Table 2-5. | ✅ |
| `FIX POSITION <lat> <lon> <h>` | Turn the unit **back into a base** at a known coordinate (lat/lon in degrees, h in m MSL). See V3.1 §2.2.6. | 🔷 |
| `LOCKOUT <prn>` | Exclude a specific satellite by PRN number. See V3.1 §2.2.12. | 🔷 |

> ⚠️ `SET APPSCENE` replaces `RTKDYNAMICS FOOT` and `RTKOBSMODE 0` from older docs.
> Both `RTKDYNAMICS` and `RTKOBSMODE` are **not present** in V3.1 — they are older constructs.
> or older firmware constructs.

**`SET APPSCENE` modes:**

| Mode | Use |
|---|---|
| `Survey` | Walking survey / pole rover — **the firmware default** |
| `Robot` | Mobile robot |
| `Car` | Vehicle-mounted |
| `Uav` | Drone |
| `Air` | Aircraft |
| `Space` | Spacecraft |

Change it in `kCommands[]` if you are mounting the rover on a vehicle or a drone.

### NMEA output

```
LOG <port> <sentence> ONTIME <interval>
```

`<interval>` is in **seconds**: `1` = 1 Hz, `0.2` = 5 Hz, `0.1` = 10 Hz.
See V3.1 §2.2.14 for ONTIME trigger format.

| Sentence | Contents | Consumed by |
|---|---|---|
| `GPGGA` | Time, position, **fix quality**, sats used, HDOP, MSL altitude, geoid separation, **correction age**, base station ID | Everything. Also uploaded to VRS/MAC casters |
| `GPGST` | σ latitude, σ longitude, σ altitude — the precision estimate | Dashboard "Quality", SW Maps ΔH/ΔV |
| `GPGSA` | Active satellites, PDOP / HDOP / VDOP | Dashboard DOPs |
| `GPGSV` | Satellites in view + SNR per satellite | Sky plots, siting |
| `GPRMC` | Position, speed, course, date | Apps that require RMC |
| `GPVTG` | Course and speed over ground | Motion display |
| `GPZDA` | UTC date and time | Clock sync |

**GGA fix-quality field (field 6)** — the number your whole setup is judged by:

| Value | Meaning | Typical accuracy |
|---|---|---|
| `0` | No fix | — |
| `1` | Single / autonomous | 1.5–3 m |
| `2` | DGPS / SBAS | 0.5–2 m |
| `4` | **RTK FIXED** | **~10 mm** ← the goal |
| `5` | RTK FLOAT | 0.2–1 m |
| `6` | GNSS/INS integrated | IMU-assisted |

The dashboard and SW Maps both decode this. `4` is what you are aiming for.

> **Talker IDs:** ComNav emits `GP…` for these logs, but multi-constellation firmware may
> emit `GN…` (e.g. `$GNGGA`). Both the dashboard and every serious GIS app match on the last
> three characters, so either is fine. Some receivers accept `GNGGA` as a log name too; if
> `LOG COM1 GNGGA ONTIME 1` is rejected, use the `GP` form — the output talker ID is decided
> by the receiver, not by the log name.

### Diagnostics

#### `LOG VERSION`

```
LOG VERSION
```

Returns model, firmware build, serial number and enabled authorizations. Run this first on a
new unit — it tells you which constellations, frequencies and output rates you actually own.
Abbreviated ASCII format: `<VERSION COM1 0 60.0 FINESTEERING 2437 389513.150 00000000 0000 1114`
followed by `GPSCARD` line with model, SN, board, FW version, build date.

#### `LOG COMCONFIG`

Reports current port settings. The boot sequence runs it **before** `INTERFACEMODE … AUTO
AUTO ON`, because afterwards the port will not answer.

#### `LOG SYSCONFIG`

Reports the current system configuration in one dump: PVT/PPP/RTK frequency, cut angles,
lockout status, RTK base mode, app scene, interface mode per port, PPS control, datum, etc.
Useful for diff-checking after write commands. **Not documented in V3.1**, but confirmed
working on FW 7.4.6 — it answers with the full block. | ✅

Worth reading once on a new unit. What it reports on a factory board:

```
PVT Frequency: 5 Hz          RTK Base Mode: FIXED-BASE      App Scene: SURVEY. 0
RtkSnrThresh : 28            RTK Time Out : 200.0 s         ReceiverRole: Single
PvtObs       : G(1) C(6) R(9) E(13)      SBAS: Disabled     PPP: OFF
AE           : 100M          PPS Control: ENABLE POSITIVE 1.00s 1000us Model 0
```

`RTK Base Mode: FIXED-BASE` is a stored setting, not an active state — `ReceiverRole: Single`
and an empty `LOGLIST` are what tell you the unit is not running as a base.

`AE : 100M` is the antenna-electronics clock. See [Antenna electronics](#antenna-electronics-set-ae).

#### `LOG LOGLIST`

Lists the logs currently configured. On a fresh receiver it answers `OK!` and prints **nothing**
— that is a correct empty list, not a broken command. After the boot sequence it shows all
seven NMEA logs:

```
COM1,GPGGA,ABBASCII,ONTIME,1.000,0.000,
COM1,GPGST,ABBASCII,ONTIME,1.000,0.000,
...
```

Useful as the cheapest proof that `SAVECONFIG` worked: power-cycle the receiver and check that
the list is still populated without the ESP32 touching it.

#### Other useful logs

| Command | Purpose | |
|---|---|---|
| `LOG BESTPOSA ONTIME 1` | ComNav ASCII position with solution status and stdevs — richer than GGA. See V3.1 §3.2.5.1. | 🔷 |
| `LOG BESTVELA ONTIME 1` | Best velocity (ID 99). See V3.1 §3.2.5.2. | 🔷 |
| `LOG PSRPOSA ONTIME 1` | Pseudorange position (ID 47). See V3.1 §3.2.5.5. | 🔷 |
| `LOG SATXYZA ONTIME 5` | Satellite positions in ECEF plus clock/iono/tropo corrections. See V3.1 §3.2.7.5. | 🔷 |
| `LOG SYSRTS ONTIME 1` | Real-time board status: CPU/interrupt load, temperature, antenna status. See below. | 🔷 |
| `LOG SPECTRUMB ONTIME 1` | RF spectrum sweep. **Binary only** — `LOG SPECTRUM` is accepted and stays silent. See [Spectrum scan](#spectrum-scan). | 🔷 |

These are ASCII logs, not NMEA. Handy on the bench; leave them off in the field, since GIS
apps will not parse them and they eat bandwidth.

#### `LOG SYSRTS` — board status

```
$SYSRTS,400.00,000622.00,29,1,0,AT,0,,,0,0,,,,,,,,,M,SURVEY,0,,,0,0,2200,,,0,,,,*42
                     │ │ │ │  │ └ ANT1 SNR
                     │ │ │ │  └─── ANT1 qual
                     │ │ │ └────── ANT1 status
                     │ │ └──────── CPU temperature
                     │ └────────── CPU utilization %
                     └──────────── interrupt utilization %
```

Per V3.1 §3.4.5, `ANT1 status` is `0 = No Load`, `1 = Normal`, `2 = Short circuit`,
`3 = Crosstalk`, and `ANT1 qual` is a 0–100 score (≥ 85 in open sky).

> ⚠️ **On this board the antenna fields are empty.** FW 7.4.6 reports `ANT1 SNR` as `0` and
> leaves `qual` and `status` blank, so `SYSRTS` **cannot** be used to diagnose the antenna
> here. Everything else in the log (load, temperature, solution status) is populated and
> useful. Do not read the blank fields as "no antenna" — they simply are not filled in.

#### `LOG TRACKSTATA`

Not usable. It answers `OK! Command accepted!` and then emits nothing — the keyword parses but
the log is not implemented in 7.4.6. There is no per-channel C/N0 view on this build; use
`GPGSV` for signal strength instead.

### Antenna electronics (`SET AE`)

```
SET AE OFF | 25M | 100M
```

V3.1 §2.2.24: sets the working state of the **AE (Antenna Electronics)** at power-on — `OFF`,
`25M` (25 MHz clock AE) or `100M` (100 MHz clock AE, **default**). This is a clocked antenna
interface, not the plain DC bias an ordinary active antenna uses.

> **It does nothing for a Beitian BT-800D.** Measured on this project's board: stepping
> `SET AE` through `100M` → `OFF` → `25M` and waiting 20 s at each step left the satellite
> count at exactly zero. The BT-800D has a DC-biased LNA and no clocked electronics, so the
> setting is irrelevant to it. If you are chasing a dead antenna, look elsewhere —
> [HARDWARE.md → Antenna](HARDWARE.md#antenna) has the diagnostic sequence.

### Spectrum scan

```
SCANSPECTRUM <center-freq> <scan-range> <scan-times>
LOG COM1 SPECTRUMB ONTIME 1
```

V3.1 §2.2.26: sweeps the RF front end. `center-freq` is in **kHz** (1165000–1613000),
`scan-range` in kHz (0–11500), `scan-times` is the number of points (3–200). The receiver
acknowledges with `ScanBegin:<f>KHz <r>KHz <n>`.

`LOG SPECTRUM` (ID 2260) is the result — and it is **binary only**. Requesting `SPECTRUM`
without the `B` suffix is accepted and then emits nothing, which is easy to mistake for a
missing feature.

Body layout, from V3.1 §3.4.4:

| Offset | Field | Type |
|---|---|---|
| H+0 | Center frequency, kHz | int32 |
| H+4 | Scan range, kHz | int32 |
| H+8 | Number of points | int32 |
| H+12 | Start frequency, kHz | int32 |
| H+16 | Step, kHz (negative — the sweep runs downwards) | int32 |
| H+20 | Level, one `uint16` per point | uint16 × N |

Binary logs use the ComNav header: sync `AA 44 12`, header length at byte 3 (28 in current
firmware), message ID at bytes 4–5, body length at bytes 8–9.

> **How to read it — and what it cannot tell you.** GNSS signals sit **20 dB below the noise
> floor** in the band a sweep covers: L1 C/A spreads ~23 dBW of transmitted power over 2 MHz, so
> at the antenna it is around −130 dBm while thermal noise in that bandwidth is about −111 dBm.
> **A spectrum sweep therefore never shows satellites.** It shows the noise floor and anything
> narrowband sitting above it. Do not conclude "no signal" from a flat profile — a perfectly
> working antenna looks flat too.
>
> What the sweep is genuinely good for:
>
> - **Interference.** A narrow peak spanning a few steps is a real emitter — a bad power supply,
>   a video transmitter, an LED driver, a phone charger. Subtracting it is worth more than any
>   amount of antenna fiddling.
> - **Whether the front end sees anything at all.** A level pinned at the bottom of the range, or
>   identical with the antenna shorted and open, points at a dead front end.
> - **Relative comparison.** Same `SCANSPECTRUM` arguments, antenna connected versus
>   disconnected: an active antenna with a live LNA raises the floor by roughly its gain minus the
>   receiver's own noise figure.
>
> Two traps measured on this board. The reported level is in **arbitrary receiver units** — there
> is no dBm calibration in V3.1, so absolute numbers mean nothing and only comparisons do. And a
> sweep can come back **short**: one run reported 56 points where 100 were requested. Always check
> that the returned point count matches the requested one before comparing two profiles.
>
> `tools/scan_spectrum.py` automates both passes. For antenna health, the DC bias measurement
> under load is a far better instrument than the sweep — see
> [HARDWARE.md → Antenna](HARDWARE.md#antenna).

### Interface mode and differential input

```
INTERFACEMODE <port> <input-mode> <output-mode> <switch>
```

| Command | Effect | |
|---|---|---|
| `INTERFACEMODE COM1 AUTO AUTO ON` | Auto-detect on both directions — RTCM3 corrections in, logs out. **Rover mode.** | ✅ |
| `INTERFACEMODE COMPASS COMPASS ON` | Back to the ComNav protocol so ASCII commands are parsed again. **The escape hatch.** | ✅ |
| `INTERFACEMODE COM1 RTCMV3 RTCMV3 ON` | Explicit: RTCM3 in and out. Use if `AUTO` misbehaves on your build | 🔷 |

With `AUTO` on the receive side, the RTCM3 stream your phone forwards is detected and fed to
the RTK engine with no further configuration. That is the entire correction path.

> V3.1 §2.2.10: `input-mode` can be `NONE`, `RTCMV3`, `AUTO`, `COMPASS`. `output-mode`
> remains in universal mode regardless of this command.

> ⚠️ **Probe it on your own unit before trusting rover mode.** On some firmware builds a bare
> `INTERFACEMODE`, and `LOG INTERFACEMODE` / `LOG COM1 INTERFACEMODE`, answer
> `Error! Invalid key word!` even though the full `INTERFACEMODE <port> <in> <out> <switch>`
> form used here works fine. Command existence on ComNav is *not* reliably probed by sending
> the keyword bare (`FIX POSITION` bare also answers `OK!`), so `Invalid key word` on the bare
> form proves nothing.
>
> Practical consequence: if `INTERFACEMODE … AUTO AUTO ON` turns out to be unsupported on your
> firmware, RTCM3 from the phone will not reach the RTK engine and you will never get
> `GGA` quality `4` — the boot log will show `REJECTED` on that line. Read the boot log once
> before trusting rover mode.

> **What `AUTO` actually does to the port, measured on 7.4.6.** After
> `INTERFACEMODE COM1 AUTO AUTO ON` the port parses **no** ASCII: a valid command and a garbage
> keyword both draw silence, while in `COMPASSNAV` the same garbage answers
> `Error! Invalid key word!`. The single exception is `INTERFACEMODE COMPASS COMPASS ON`, which
> is still honoured — that is the escape hatch, and it is why recovery works at all.
>
> Two consequences: everything you want the receiver to keep must be sent **before** this line,
> and `SAVECONFIG` in particular belongs above it (see [the boot sequence](#the-shipped-boot-sequence)).

### Persistence and reset

| Command | Effect | |
|---|---|---|
| `SAVECONFIG` | Write the current configuration to non-volatile memory. See V3.1 §2.2.22. | ✅ |
| `RESET` | Warm reset; configuration retained (reverts to last SAVECONFIG or factory default). See V3.1 §2.2.17. | 🔷 |
| `FRESET` | **Factory reset.** Wipes configuration, saved almanac and ephemeris. See V3.1 §2.2.7. | 🔷 ⚠️ |

> `FRESET` also clears the almanac, so the next cold start is slow (minutes, not seconds).
> Prefer `UNLOGALL` + explicit configuration. Keep `FRESET` for a receiver that is genuinely
> stuck.

---

### IMU / Dead-reckoning (not used)

The K803 carries an IMU, and V3.1 §4.5 documents a dead-reckoning command group for it.
**This firmware sends none of them** — see the note under
[the shipped boot sequence](#the-shipped-boot-sequence). They are listed here so you know what
exists before you go looking for it.

| Command | Effect | |
|---|---|---|
| `INSCONTROL ENABLE` | Enable INS navigation. See V3.1 §2.2.23. | 🔷 |
| `INSCONTROL DISABLE` | Disable INS navigation. | 🔷 |
| `INSCONTROL ENABLE 0/1/2/3` | Set INS mode. See V3.1 §4.5.4. | 🔷 |
| `SET SMOOTHEDDR ON` | Enable dead-reckoning smoothing. See V3.1 §4.5.4. | 🔷 |
| `SET SMOOTHEDDR OFF` | Disable dead-reckoning smoothing. | 🔷 |
| `SET IMUAXESTYPE 1/2/3/4/5/6/7/8` | Set IMU axis orientation. See V3.1 §4.5.4. | 🔷 |
| `SET DRLEVERARM X Y Z` | Set lever arm vector from antenna phase center to IMU, in the IMU frame (X forward, Y right, Z down). Range: ±20 m. See V3.1 §4.5.2. | 🔷 |
| `SET DRCONFIG 1.0` | Set DR configuration parameter. See V3.1 §4.5.4. | 🔷 |
| `INSCONTROL CLEAR` | Clear INS state. See V3.1 §4.5.4. | 🔷 |
| `SET DRTIMEOUT 0-3600` | Set DR timeout in seconds. See V3.1 §4.5.4. | 🔷 |
| `TST DR RAW ON` | Enable raw DR debug output. See V3.1 §4.5.4. | 🔷 |
| `LOG IMUDATAB` | Raw gyro + accelerometer, ID 2264, 100 Hz (`ontime 0.01`). See V3.1 §3.4.9. | 🔷 |

#### Why not, on a pole rover

DR needs a **lever arm** (`SET DRLEVERARM x y z`, millimetres from the antenna phase centre to
the navigation centre) and a **vehicle axis convention** (`SET IMUAXESTYPE`) before it produces
anything meaningful — it is designed for a moving platform that has to keep reporting position
through a tunnel or under tree cover. A rover that stops to take a point already has a clean
sky. Worse, once INS engages the receiver starts reporting GGA quality `6` (GNSS/INS
integrated) instead of `4` (RTK FIXED), and a GIS app has no way to tell that the position is
now dead-reckoned rather than measured.

If you want it anyway, the manual's §4.5.3 initialization is: enable INS, get RTK FIXED from
differential data, then move the receiver ~10 m in open sky to converge, and `SAVECONFIG`.

#### Tilt compensation is a different, unavailable thing

Do not confuse DR with **tilt measurement** — the "tilt the pole over and still get the right
point" feature. That is V3.1 §4.6 (`insrtk reset`, `insrtk rodlength`, `insrtk leverarmgnss`,
`insrtk leverarmarp`, `insrtk installangle`, `insrtk fastinit`, `insrtk init 1`) plus the logs
of §3.4.8 (`INSRTKCOMPENSATE`, ID 2366).

V3.1 documents both as being for a different board variant, not for the K803. They will be
rejected, and no authorization adds them.

Practical substitute: keep the pole vertical and enter its length as **Instrument Height** in
the GIS app. See [HARDWARE.md → What the K803 does not do](HARDWARE.md#what-the-k803-does-not-do).

---

### RTCM relay

| Command | Effect | |
|---|---|---|
| `SET RELAYRTCMV3 ON COM2` | Relay RTCM3 from COM1 to COM2. See V3.1 §2.2 (`SET RELAYRTCMV3`). | 🔷 |
| `SET RELAYRTCMV3 OFF COM2` | Disable RTCM3 relay. | 🔷 |

> Whether the relay is on is visible in `LOG SYSCONFIG` (`RelayRtcmv3 : OFF, COM1` on a
> factory board). Otherwise just try it: corrections either arrive on COM2 or they do not.

---

### System lockout

| Command | Effect | |
|---|---|---|
| `LOCKOUTSYSTEM GPS` | Lock out all satellites in a system (GPS, GLONASS, GALILEO, BD2, BD3). See V3.1 §2.2.13. | 🔷 |
| `UNLOCKOUTSYSTEM GPS` | Unlock all satellites in a system. See V3.1 §2.2.29. | 🔷 |
| `UNLOCKOUTALL` | Re-enable all previously locked-out satellites. See V3.1 §2.2.28. | ✅ |

Factory default: nothing locked out. `LOG SYSCONFIG` lists locked-out satellites where
supported; otherwise check SNR in `GPGSV` — a locked-out constellation simply disappears.

---

### PPS output

| Command | Effect | |
|---|---|---|
| `PPSCONTROL ENABLE <polarity> <period> <pulse-width> [model]` | Configure PPS output. See V3.1 §2.2.15. | 🔷 |
| `PPSCONTROL DISABLE` | Disable PPS output. | 🔷 |

- `polarity`: `NEGATIVE` — triggered on the rising edge; `POSITIVE` — on the falling edge
- `period`: 0.05, 0.1, 0.2, 0.25, 0.5, 1.0, 2.0, 3.0 … 20.0 s
- `pulse-width`: any positive value ≤ half the period, in **milliseconds**
- `model`: `0` — no PPS after a satellite-signal interruption; `1` — force PPS through an
  interruption (accuracy decays until signal returns); `-1` — never output PPS

Example from V3.1: `PPSCONTROL ENABLE POSITIVE 1 1000 1`.

> The firmware sends `PPSCONTROL ENABLE POSITIVE 1 1000 1` during init and counts the pulse on
> GPIO2 — see [HARDWARE.md → PPS](HARDWARE.md#pps-optional).

---

## Output rate recipes

Edit `kCommands[]` in [`src/gnss_init.cpp`](src/gnss_init.cpp) and reflash.

### 1 Hz — the default

```
LOG COM1 GPGGA ONTIME 1
LOG COM1 GPGST ONTIME 1
LOG COM1 GPGSA ONTIME 1
LOG COM1 GPGSV ONTIME 1
LOG COM1 GPRMC ONTIME 1
LOG COM1 GPVTG ONTIME 1
LOG COM1 GPZDA ONTIME 1
```

Right for point survey and mapping. ~1 KB/s.

### 5 Hz — moving rover

```
LOG COM1 GPGGA ONTIME 0.2
LOG COM1 GPGST ONTIME 0.2
LOG COM1 GPRMC ONTIME 0.2
LOG COM1 GPVTG ONTIME 0.2
LOG COM1 GPGSA ONTIME 1
LOG COM1 GPGSV ONTIME 5
LOG COM1 GPZDA ONTIME 1
```

Note what changed: the **position** sentences go to 5 Hz, while `GSV` — which is several
sentences per epoch across four constellations — drops to once every 5 s. Nothing on a map
needs a satellite list at 5 Hz.

### 10 Hz — machine guidance, vehicle tracking

```
LOG COM1 GPGGA ONTIME 0.1
LOG COM1 GPRMC ONTIME 0.1
LOG COM1 GPVTG ONTIME 0.1
LOG COM1 GPGST ONTIME 1
LOG COM1 GPGSA ONTIME 1
LOG COM1 GPZDA ONTIME 1
```

Drop `GSV` entirely and keep only what actually needs the rate.

### Bandwidth reality check

At 115200 baud you have about **11.5 KB/s**. A GGA is ~80 bytes; GSV across four
constellations can be **500+ bytes per epoch on its own**.

| Configuration | Approx. load | Verdict |
|---|---|---|
| Full set @ 1 Hz | ~1 KB/s | ✅ Comfortable |
| Position set @ 5 Hz + GSV @ 0.2 Hz | ~2 KB/s | ✅ Fine |
| Position set @ 10 Hz, no GSV | ~3 KB/s | ✅ Fine |
| **Full set including GSV @ 10 Hz** | **~10 KB/s** | ❌ Saturates the UART and the BLE link; sentences get dropped or truncated |

Symptoms of over-subscription: rising bad-checksum counts in the dashboard footer, truncated
sentences, an app that stutters. If you see that, cut `GSV` first.

> High output rates may require an authorization your unit does not have. If
> `ONTIME 0.1` is rejected, check `LOG VERSION`.

---

## RTCM input

Nothing to configure — `INTERFACEMODE COM1 AUTO AUTO ON` auto-detects the stream. What the
receiver expects from a caster:

| Message | Contents |
|---|---|
| 1005 / 1006 | Base station ARP coordinates |
| 1074 / 1075 / 1077 | GPS MSM observations |
| 1084 / 1085 / 1087 | GLONASS MSM |
| 1094 / 1095 / 1097 | Galileo MSM |
| 1124 / 1125 / 1127 | BeiDou MSM |
| 1230 | GLONASS code-phase biases |
| 1004 / 1012 | Legacy GPS / GLONASS observables |

Any standard NTRIP mount point serving RTCM 3.x works. **BeiDou (1124-series) matters** on a
ComNav receiver — the engine is strong on BDS, and a mount point without it leaves
performance on the table.

**Correction age**, in GGA field 13, is the health metric to watch:

| Age | Meaning |
|---|---|
| < 2 s | Healthy |
| 2–5 s | Acceptable |
| 5–10 s | Degrading — the dashboard turns this amber |
| > 10 s | Fix will drop to FLOAT — dashboard turns it red |

Growing age with a connected NTRIP client means the phone's cellular link is stalling, not
the receiver.

### Verified on hardware

The correction path was tested end to end with **no ESP32 and no phone**: RTCM3 from a real
NTRIP caster was written straight into the receiver's UART, exactly as the bridge would, with
COM1 in `AUTO`. See [TODO.md](TODO.md) item 12 for the full log.

```
caster stream:  603 B/s, 88 frames in 20 s, 0 CRC failures
  1075/1085/1095/1125  MSM5 @ 1 Hz   (code + full carrier phase)
  1006 + 1008 + 1033   station and antenna metadata
result: 136 epochs SINGLE -> 16 epochs RTK FLOAT, GGA age 1.0 s, ref station 0004
```

Two conclusions worth keeping:

- **The bridge's job is proven.** `INTERFACEMODE … AUTO AUTO ON`, `0xD3` auto-detect, the
  length field and the CRC-24Q check all work on real corrections — the receiver advances to
  FLOAT, so the corrections are reaching the RTK engine.
- **MSM5 is enough.** Full carrier phase, not just code. A mount point serving MSM4 or higher
  is usable; MSM1–MSM3 (code only, or coarse phase) will never give a fixed solution.

`FIXED` was not reached in that run. The antenna was on a bench without a ground plane, C/N0
sat at 29–36 dB-Hz against `RtkSnrThresh 28`, and the sky was overcast. That is an
observation-environment result, not a code fault — retest under open sky before chasing
anything in the firmware.

To run the same test yourself: `python tools/ntrip_to_receiver.py COM17 <mountpoint>`, with
caster credentials in `.ntrip` (git-ignored).

---

## Changing the sequence in firmware

All of it lives in one array in [`src/gnss_init.cpp`](src/gnss_init.cpp):

```c
struct Cmd {
  const char *text;
  bool expectReply;
  uint8_t maxRetries;
};

static const Cmd kCommands[] = {
    {"UNLOGALL " GNSS_CMD_PORT, true, 2},
    ...
    {"LOG " GNSS_CMD_PORT " GPGGA ONTIME 1", true, 1},
    ...
    {"SAVECONFIG", true, 1},                                      // before the switch
    {"INTERFACEMODE " GNSS_CMD_PORT " AUTO AUTO ON", false, 1},   // LAST
};
```

Each entry carries three fields:

| Field | Meaning |
|---|---|
| `text` | The ASCII command string (concatenated with `GNSS_CMD_PORT` at compile time) |
| `expectReply` | `true` — silence counts as a failure; `false` — silence is normal |
| `maxRetries` | **Total attempts.** `1` = send once. `2` = resend once if the first attempt stayed silent — used on `UNLOGALL COM1` and `SET APPSCENE Survey`, where the receiver can be mid-log and miss a command. Never raise it above `255` and never set `0`: an entry with `0` would be skipped and silently counted as a no-reply |

Rules:

1. `INTERFACEMODE … AUTO AUTO ON` is the **last command the receiver can still hear.**
   Anything after it is shouted into a port that is no longer listening. `SAVECONFIG` sits
   directly above it for exactly this reason.
2. Add diagnostics before `LOG COMCONFIG`, not after.
3. `GNSS_CMD_PORT` is a macro — use the concatenation form so one define changes every line.
4. Watch the boot log for `REJECTED` after any change.
5. Set `GNSS_INIT_K803 0` in `src/config.h` to skip the whole thing — for a receiver you
   configured by hand, or a non-ComNav one (u-blox, Septentrio). The bridge still works; it
   just stops touching the receiver's configuration.

Timing: the sequence costs `2 s + (n × GNSS_CMD_WAIT_MS)`. Lowering `GNSS_CMD_WAIT_MS` speeds
up boot at the cost of possibly reading a reply too early — 600 ms is a comfortable margin,
300 ms usually works, but `SAVECONFIG` alone takes ~400 ms to answer (flash write), so do not
go below that. Commands that go silent are retried, which adds a few seconds per silenced
command. Three silenced commands is treated as a dead link (`NO REPLY`), not a soft warning.

Before flashing, the whole table can be replayed against a receiver on the bench:
`python tools/replay_boot.py COM13` sends the same commands in the same order and prints a
per-command verdict.

---

## Configuring from a PC

Useful for exploring the command set before committing anything to firmware — and the only way
to diagnose a receiver that will not talk to the ESP32.

1. Wire a **3.3 V** USB-TTL adapter to the receiver's UART pins: adapter RX ← receiver TX, adapter TX →
   receiver RX, GND ↔ GND. Power the receiver from its own supply.
2. Open a terminal at **115200 8N1** — PuTTY, `pio device monitor`, CoolTerm, RealTerm.
3. Set line ending to **CR+LF**. This is the most common reason "nothing responds".
4. Turn on local echo if you want to see what you type.
5. Start with:

   ```
   INTERFACEMODE COMPASS COMPASS ON
   LOG VERSION
   UNLOGALL COM1
   ```

   If `LOG VERSION` answers, you have a working console.

6. Explore. Then put whatever you settled on into `kCommands[]` and let the ESP32 do it
   every boot.

> With `INTERFACEMODE COM1 AUTO AUTO ON` already active from a previous session, the first
> command will appear to do nothing. Send the `COMPASS` line first — it is exactly why the
> firmware sends it too.

---

## Recovery: when the port stops answering

**Symptom:** every command returns `(no response)`, or the receiver streams NMEA and ignores
you completely.

**Cause, nine times out of ten:** the port is in RTCM differential auto-detect from a
previous session.

**Fix:**

```
INTERFACEMODE COMPASS COMPASS ON
```

Send it, wait a second, then try `LOG VERSION`. This is the first thing `gnssInit()` sends
on every boot, which is why the firmware recovers by itself from a state that would leave a
manual user stuck.

If it still does not answer, work through:

| Check | How |
|---|---|
| **Boot loop, not silence** | If you see `CBOOT … / Register Code is OK. / Loading...` repeating, the receiver is resetting, not ignoring you. This is a **cable or power** fault, not a protocol one — see below |
| Wrong baud | Try 9600, 19200, 38400, 57600, 115200, 230400 |
| TX/RX swapped | Swap the pair. This is the most common wiring error |
| No common ground | Meter it. UART without a shared reference does not work |
| No power | Confirm 5 V at the board |
| Line ending | Must be CR+LF |
| Receiver genuinely lost | `FRESET`, then reconfigure. Expect a slow first cold start |

> **A repeating `CBOOT` banner is a wiring fault.** Seen on this project with a bad USB cable:
> the receiver never finishes booting, so every command is sent into a unit that is rebooting
> roughly every 1.3 s and never parses anything.
>
> ```
> CBOOT 7.4.6 BUILD (2025-Feb-13 13:48:00)
> SN: 01A0 09478926 2024/01/17 A80 746
> Register Code is OK.
> Loading...                    ← and the banner starts again
> ```
>
> `Register Code is OK` proves the flash and the authorization are fine, so this is not
> corrupted firmware. Replacing the cable fixed it immediately. If you see this pattern, swap
> the USB cable and check that the supply can actually source 150–300 mA before touching
> anything else — see [HARDWARE.md → Power](HARDWARE.md#power).

The ESP32 boot log tells you which of these it is. Since the sequence stops at the first
silent `LOG VERSION`, a dead link shows up as two lines and `init NO REPLY` rather than
sixteen timeouts. Correct replies followed by `REJECTED` on specific commands means the
link is fine and only those commands are unsupported.

---

## Gotchas

- **Order is load-bearing.** `INTERFACEMODE … AUTO AUTO ON` last, always.
- **`FIX NONE` is not optional.** A leftover base coordinate silently prevents a rover
  solution, and the failure looks like "RTK never fixes" rather than like a configuration
  error.
- **Base-station leftovers.** A board that shipped as a base is configured to *transmit*
  corrections, and it may have been doing so on more than one port. Unlog COM2/COM3 as well —
  they are harmless no-ops on a bare OEM board, and save you a mystery stream.
- **Reply parsing.** The firmware checks every reply for `Invalid` / `Error!`. Rejected commands
  are counted and surfaced in the boot log (`init WARN`) and in every `[STAT]` line
  (`GNSS WARN rejected=N`). A refused `FIX NONE` leaves the unit in base mode and RTK
  never fixes — visible as a persistent warning, not as missing data.
- **No reply.** If `LOG VERSION` gets no answer, the entire sequence aborts after ~2 s
  (instead of wasting ~12 s on dead commands). The boot log says `init NO REPLY` and
  `[STAT]` shows `GNSS NO REPLY` until the receiver is wired correctly.
- **Silence is retried, rejection is not.** A command that answers nothing is re-sent (up to
  `maxRetries` times) — a receiver that was mid-log can miss one. A command answered
  `Invalid` is never retried: the reply proves the UART is alive and the command is genuinely
  unsupported. Once three commands stay silent, the init reports `NO REPLY` rather than `WARN`
  — everything after the point where the link went quiet was never configured, so the unit
  would stream NMEA and never produce an RTK fix.
- **Don't save a baud change** unless you also change `GNSS_BAUD` and reflash.
- **Talker IDs vary.** `$GPGGA` vs `$GNGGA` is normal, and neither breaks anything.
- **Board revisions vary.** The boot sequence was replayed against this project's board
  (`S32352K803`, FW 7.4.6) and all 17 commands were accepted. A different revision may answer
  differently — on first boot read the log; if your unit behaves differently, open an issue
  with `LOG VERSION` output and the boot log.
- **`SET APPSCENE` replaces `RTKDYNAMICS` and `RTKOBSMODE`.** Both are absent from V3.1.
  Sending them returns `Error! Invalid key word!`.
- **Tilt compensation is not on this board.** The whole `insrtk` group and the §3.4.8 tilt logs
  are documented for a different variant. They are rejected — that is a hardware difference,
  not a missing authorization, and it is not fixable from software.
- **`SYSCONFIG` is read-only via `LOG`.** There is no `SET SYSCONFIG` command. Use individual
  commands (e.g. `PPSCONTROL`, `SET RELAYRTCMV3`) and verify with `LOG SYSCONFIG` — it does
  answer on 7.4.6, despite not being in V3.1.
- **`OK!` is not proof.** `LOG TRACKSTATA` answers `OK! Command accepted!` and emits nothing.
  Check that output actually appears before building anything on a log.
- **Binary logs need the `B` suffix.** `LOG SPECTRUM` is accepted and silent;
  `LOG SPECTRUMB` produces the data. The same pattern applies to other binary-only logs.
- **An empty `LOGLIST` is a correct answer.** On a factory receiver nothing is configured yet.
- **Antenna faults do not show up in `SYSRTS` on this board.** `ANT1 status` and `ANT1 qual`
  are left blank even with the antenna disconnected. Diagnose the antenna with
  `LOG COM1 GPGSV ONTIME 1` and, if that is empty, with a
  [spectrum scan](#spectrum-scan) against a disconnected-antenna baseline.
- **`RTKSOURCE` can silently discard every correction.** V3.1 §2.2.20 filters incoming RTCM by
  base-station ID. With a specific ID set, corrections from any other station are dropped
  without a word — NMEA keeps streaming, a point keeps plotting, and RTK never fixes. The
  documented `RTKSOURCE RTCMV3 ANY` is accepted on 7.4.6; send it once and confirm. Note that
  `LOG RTKSOURCE` is **not** a valid read command (`Error! Invalid key word!`) — the value only
  appears inside `LOG SYSCONFIG`.
- **SBAS is turned off with `SET SIGNAL SBAS OFF`.** Neither `SBASSYS DISABLE` nor
  `SET SBAS OFF` is accepted on 7.4.6, even though `SBASSYS` is listed in the V3.1 contents as
  §2.2.32. `SET SIGNAL SBAS OFF` answers `OK` and `LOG SYSCONFIG` then shows
  `SBAS Control : Disabled`.

---

Next: **[FLASHING.md](FLASHING.md)** — getting the firmware onto the board.