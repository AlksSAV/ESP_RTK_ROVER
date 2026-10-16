# COMMANDS.md — ComNav K803 Command Reference

Command reference for the **SinoGNSS K803** RTK receiver, covering the boot sequence
used by this firmware, the full set of ASCII commands from OEM Board Reference Manual
**V3.1**, and live-verified commands from **FW 7.4.6** (2025-12-30).

- [How the receiver talks](#how-the-receiver-talks)
- [The point of no return](#the-point-of-no-return)
- [The shipped boot sequence](#the-shipped-boot-sequence)
- [Command reference](#command-reference)
  - [Unlogging](#unlogging)
  - [Port configuration](#port-configuration)
  - [Positioning mode](#positioning-mode)
  - [NMEA output](#nmea-output)
  - [Diagnostics](#diagnostics)
  - [Interface mode and differential input](#interface-mode-and-differential-input)
  - [Persistence and reset](#persistence-and-reset)
  - [IMU / Dead-reckoning (full K803 only)](#imu--dead-reckoning-full-k803-only)
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
| ✅ | **Emitted by the shipped boot sequence.** The mark says nothing about whether the receiver accepted it — watch for `REJECTED` in the boot log. |
| 🔷 | From the ComNav OEM Board Reference Manual V3.1 — not sent by this firmware, confirm on your unit before relying on it. |
| 🟡 | Verified live on FW 7.4.6 (2025-12-30) on a separate test rig (comnav.md). That unit was configured as a **base**, not a rover. |

> ⚠️ **Nothing here has been verified on this fork's own board.** The boot sequence is
> inherited from upstream, where it was confirmed on a single K803 **Lite** taken out of an
> ECOVACS base station. The full K803 in this project has not been talked to yet, and its
> IMU commands (`INSCONTROL`, `SET SMOOTHEDDR`) have never been exercised at all. Treat every
> ✅ as "will be sent", not as "works".

Command availability varies with firmware build and **authorization**. A command your unit
does not support is rejected, not silently ignored — check the reply.

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

  > ⚠️ In FW 7.4.6, the reply format differs from older docs: accepted commands return
  > `OK! Command accepted! Port: COMn.` rather than `$command,response: OK*<crc>`.
  > Rejected commands return `Error! Invalid key word!`. The firmware checks for
  > `Invalid` in the reply string.

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

Run by `gnssInit()` on every boot, before Bluetooth starts. Roughly **13 seconds**:
2 × 1000 ms preamble + 18 × `GNSS_CMD_WAIT_MS` (600 ms).

Running it every boot is deliberate — the receiver is left in a known state no matter what
the previous session, a firmware experiment, or the mower did to it.

### Preamble

| # | Command | Why | |
|---|---|---|---|
| 1 | `INTERFACEMODE COMPASS COMPASS ON` | Escape hatch. A previous run left COM1 in RTCM auto-detect, where ASCII is ignored; this pulls it back to the ComNav ASCII/binary protocol. On a cold receiver it is a harmless no-op. **Must be first.** | ✅ |
| 2 | `LOG VERSION` | Proves the link works and records model, firmware build and authorization in the boot log. | ✅ |

### Main sequence

| # | Command | Why | |
|---|---|---|---|
| 3 | `UNLOGALL COM1` | Stop everything the factory base-station config was streaming on COM1. | ✅ |
| 4 | `UNLOGALL COM2` | Same for COM2 — the port that fed the QD-02 data module we dropped. | ✅ |
| 5 | `UNLOGALL COM3` | Same for COM3. | ✅ |
| 6 | `FIX NONE` | **Critical.** Clears the stored base-station coordinate. A base is `FIX POSITION`-ed to a surveyed point and will not compute a rover solution while that is set. | ✅ |
| 7 | `SET APPSCENE Survey` | Select **rover survey** application scenario. Replaces `RTKDYNAMICS FOOT` (not in V3.1). See V3.1 §2.2.24, Table 2-5. | ✅ |
| 8 | `UNLOCKOUTALL` | Re-enable every satellite/constellation the base config may have locked out. | ✅ |
| 9 | `INSCONTROL ENABLE` | Enable INS navigation — only on full K803 with IMU. Silently rejected on K803 Lite (not fatal). | ✅ |
| 10 | `SET SMOOTHEDDR ON` | Enable dead-reckoning smoothing — only on full K803 with IMU. Silently rejected on K803 Lite (not fatal). See V3.1 §4.5.4. | ✅ |
| 11 | `LOG COM1 GPGGA ONTIME 1` | Position, fix quality, sats, HDOP, altitude, **correction age**, base ID. The single most important sentence — NTRIP clients also echo it back to the caster. | ✅ |
| 12 | `LOG COM1 GPGST ONTIME 1` | Position-error statistics: σ latitude / σ longitude / σ altitude. This is where "6 mm" comes from. | ✅ |
| 13 | `LOG COM1 GPGSA ONTIME 1` | Active satellites, PDOP / HDOP / VDOP. | ✅ |
| 14 | `LOG COM1 GPGSV ONTIME 1` | Satellites in view with SNR. Useful for siting; the bulkiest sentence by far. | ✅ |
| 15 | `LOG COM1 GPRMC ONTIME 1` | Recommended minimum: position, speed, course, date. Some apps refuse to start without it. | ✅ |
| 16 | `LOG COM1 GPVTG ONTIME 1` | Course and speed over ground. | ✅ |
| 17 | `LOG COM1 GPZDA ONTIME 1` | UTC date and time. Lets an app set the device clock from GNSS. | ✅ |
| 18 | `LOG COMCONFIG` | Dump the port configuration to the boot log — **while the port still answers**. See V3.1 §3.2.2.1. | ✅ |
| 19 | `INTERFACEMODE COM1 AUTO AUTO ON` | Enable differential auto-detect. RTCM3 from the phone now reaches the RTK engine. [Point of no return](#the-point-of-no-return). | ✅ |
| 20 | `SAVECONFIG` | Persist configuration to non-volatile memory. See V3.1 §2.2.22. **Sent after the port stops parsing ASCII — see the note below.** | ✅ |

> ⚠️ **`SAVECONFIG` is in the wrong place and is effectively dead code.** It is emitted
> *after* `INTERFACEMODE … AUTO AUTO ON`, at which point the port interprets incoming bytes
> as RTCM and no longer reads ASCII, so the receiver never sees it as a command. It is
> harmless (nothing rejects it, nothing acts on it), and it costs nothing, because the ESP32
> replays the entire sequence on every boot and never relies on stored state.
>
> If you want the configuration to survive without the ESP32, move `SAVECONFIG` to just
> **before** the `INTERFACEMODE` line and check the reply in the boot log. Until that is
> done and verified on a real unit, treat the receiver's non-volatile config as untouched.

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
    └───────────  port: COM1~COM4        57600, 115200, 230400, 460800, 921600
```

| Command | Effect | |
|---|---|---|
| `COM COM1 115200` | Set COM1 baud rate. V3.1 §2.2.1 defines only `<port> <baudrate>` — parity, stop bits and handshaking are not settable. | 🔷 |
| `LOG COMCONFIG` | Report the current port settings. See V3.1 §3.2.2.1, Table 3-4 entry #10. | ✅ |

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
| `FIX POSITION <lat> <lon> <h>` | Turn the unit **back into a base** at a known coordinate (lat/lon in degrees, h in m MSL). See comnav.md K2. | 🔷 |
| `LOCKOUT <prn>` | Exclude a specific satellite by PRN number. See V3.1 §2.2.12. | 🔷 |

> ⚠️ `SET APPSCENE` replaces `RTKDYNAMICS FOOT` and `RTKOBSMODE 0` from older docs.
> Both `RTKDYNAMICS` and `RTKOBSMODE` are **not present** in V3.1 — they were Lite-only
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

Reports the full system configuration in one dump: PVT/PPP/RTK frequency, cut angles,
lockout status, RTK base mode, app scene, interface mode per port, PPS control, datum, etc.
Useful for diff-checking after write commands. See comnav.md.

#### Other useful logs

| Command | Purpose | |
|---|---|---|
| `LOG BESTPOSA ONTIME 1` | ComNav ASCII position with solution status and stdevs — richer than GGA. See V3.1 §3.2.5.1. | 🔷 |
| `LOG BESTVELA ONTIME 1` | Best velocity (ID 99). See V3.1 §3.2.5.2. | 🔷 |
| `LOG PSRPOSA ONTIME 1` | Pseudorange position (ID 47). See V3.1 §3.2.5.5. | 🔷 |
| `LOG SATVISA ONTIME 10` | Satellite visibility (ID 48). See V3.1 §3.2. | 🔷 |
| `LOG LOGLIST` | List all available log types (ID 5). See V3.1 §3.2.2.2. | 🔷 |

These are ASCII logs, not NMEA. Handy on the bench; leave them off in the field, since GIS
apps will not parse them and they eat bandwidth.

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

> ⚠️ **Unverified on FW 7.4.6.** A separate test rig (comnav.md, K1) reports that a bare
> `INTERFACEMODE` — and `LOG INTERFACEMODE` / `LOG COM1 INTERFACEMODE` — returns
> `Error! Invalid key word!` on FW 7.4.6, and that `LOG SYSCONFIG` shows all five ports
> already at `RxMode: COMPASSNAV, TxMode: COMPASSNAV`. That test only probed the **bare**
> keyword and the query forms; the full `INTERFACEMODE <port> <in> <out> <switch>` form used
> here was **not** exercised. Note the asymmetry: existence of a command on ComNav is *not*
> reliably probed by sending it bare (`FIX POSITION` bare also answers `OK!`), so
> `Invalid key word` on the bare form does not prove the full form is absent.
>
> Practical consequence: if `INTERFACEMODE … AUTO AUTO ON` turns out to be unsupported on your
> firmware, RTCM3 from the phone will not reach the RTK engine and you will never get
> `GGA` quality `4` — the boot log will show `REJECTED` on that line. Verify it on your own
> unit with the boot log before trusting rover mode.

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

### IMU / Dead-reckoning (full K803 only)

The full K803 (not Lite) includes an IMU for inertial navigation and dead-reckoning.
These commands are from V3.1 Chapter 4.5.

| Command | Effect | |
|---|---|---|
| `INSCONTROL ENABLE` | Enable INS navigation. See V3.1 §2.2.23. | ✅ |
| `INSCONTROL DISABLE` | Disable INS navigation. | 🔷 |
| `INSCONTROL ENABLE 0/1/2/3` | Set INS mode. See V3.1 §4.5.4. | 🔷 |
| `SET SMOOTHEDDR ON` | Enable dead-reckoning smoothing. See V3.1 §4.5.4. | ✅ |
| `SET SMOOTHEDDR OFF` | Disable dead-reckoning smoothing. | 🔷 |
| `SET IMUAXESTYPE 1/2/3/4/5/6/7/8` | Set IMU axis orientation. See V3.1 §4.5.4. | 🔷 |
| `SET DRLEVERARM X Y Z` | Set lever arm vector from antenna phase center to IMU. Range: ±20 m. See V3.1 §4.5.2. | 🔷 |
| `SET DRCONFIG 1.0` | Set DR configuration parameter. See V3.1 §4.5.4. | 🔷 |
| `INSCONTROL CLEAR` | Clear INS state. See V3.1 §4.5.4. | 🔷 |
| `SET DRTIMEOUT 0-3600` | Set DR timeout in seconds. See V3.1 §4.5.4. | 🔷 |
| `TST DR RAW ON` | Enable raw DR debug output. See V3.1 §4.5.4. | 🔷 |

> These commands are silently rejected if the unit does not support IMU (K803 Lite).
> The firmware does not treat rejections as fatal.

#### INS initialization

After enabling INS, the receiver must be initialized:

1. Enable INS: `INSCONTROL ENABLE`
2. Enter RTK fixed solution mode using differential data
3. Drive/move the receiver in an open environment to reach initialization speed
4. Drive approximately 10 meters to complete initialization
5. Save configuration: `SAVECONFIG`

---

### RTCM relay

| Command | Effect | |
|---|---|---|
| `SET RELAYRTCMV3 ON COM2` | Relay RTCM3 from COM1 to COM2. See comnav.md K10. | 🟡 |
| `SET RELAYRTCMV3 OFF COM2` | Disable RTCM3 relay. | 🟡 |

> State is visible in `LOG SYSCONFIG` as `RelayRtcmv3: ON, COM2` or `OFF, COM1`.

---

### System lockout

| Command | Effect | |
|---|---|---|
| `LOCKOUTSYSTEM GPS` | Lock out all satellites in a system (GPS, GLONASS, GALILEO, BD2, BD3). See comnav.md K5. | 🟡 |
| `UNLOCKOUTSYSTEM GPS` | Unlock all satellites in a system. | 🟡 |
| `UNLOCKOUTALL` | Re-enable all previously locked-out satellites (V3.1 §2.2.28). | ✅ |

> State is visible in `LOG SYSCONFIG` under `Lockouted SVs:`. Default from factory:
> all systems unlocked (empty lists).

---

### PPS output

| Command | Effect | |
|---|---|---|
| `PPSCONTROL ENABLE <polarity> <period_s> <pulse_width_us>` | Configure PPS output. See comnav.md K4. | 🟡 |
| `PPSCONTROL DISABLE` | Disable PPS output. | 🟡 |

- `polarity`: `POSITIVE` (triggered on falling edge) or `NEGATIVE` (triggered on rising edge)
- `period_s`: 0.05–20.0 s (0.05, 0.1, 0.2, 0.25, 0.5, 1.0, 2.0, 3.0, …)
- `pulse_width_us`: any positive value ≤ half the period, in **microseconds**

> Default from factory: `ENABLE POSITIVE 1.00s 1000us Model:0`. State visible in
> `LOG SYSCONFIG` as `PPS Control: Switch: ENABLE; Polarity: POSITIVE; Period: 1.00s; Width: 1000us; Model:0`.

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

---

## Changing the sequence in firmware

All of it lives in one array in [`src/gnss_init.cpp`](src/gnss_init.cpp):

```c
static const char *const kCommands[] = {
    "UNLOGALL " GNSS_CMD_PORT,
    ...
    "LOG " GNSS_CMD_PORT " GPGGA ONTIME 1",
    ...
    "INTERFACEMODE " GNSS_CMD_PORT " AUTO AUTO ON",   // LAST
    "SAVECONFIG",
};
```

Rules:

1. `INTERFACEMODE … AUTO AUTO ON` is the **last command the receiver can still hear.**
   Anything after it is shouted into a port that is no longer listening — which is exactly
   where the shipped `SAVECONFIG` currently sits (see [the note above](#the-shipped-boot-sequence)).
2. Add diagnostics before `LOG COMCONFIG`, not after.
3. `GNSS_CMD_PORT` is a macro — use the concatenation form so one define changes every line.
4. Watch the boot log for `REJECTED` after any change.
5. Set `GNSS_INIT_K803 0` in `src/config.h` to skip the whole thing — for a receiver you
   configured by hand, or a non-ComNav one (u-blox, Septentrio). The bridge still works; it
   just stops touching the receiver's configuration.

Timing: the sequence costs `2 s + (n × GNSS_CMD_WAIT_MS)`. Lowering `GNSS_CMD_WAIT_MS` speeds
up boot at the cost of possibly reading a reply too early — 600 ms is a comfortable margin,
300 ms usually works.

---

## Configuring from a PC

Useful for exploring the command set before committing anything to firmware.

1. Wire a **3.3 V** USB-TTL adapter to the harness: adapter RX ← receiver TX, adapter TX →
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
| Wrong baud | Try 9600, 19200, 38400, 57600, 115200, 230400 |
| TX/RX swapped | Swap the pair. This is the most common wiring error |
| No common ground | Meter it. UART without a shared reference does not work |
| No power | Confirm 5 V at the harness |
| Line ending | Must be CR+LF |
| Receiver genuinely lost | `FRESET`, then reconfigure. Expect a slow first cold start |

The ESP32 boot log tells you which of these it is: `(no response)` on **every** line means
wiring, power or baud. Correct replies followed by `REJECTED` on specific commands means the
link is fine and only those commands are unsupported.

---

## Gotchas

- **Order is load-bearing.** `INTERFACEMODE … AUTO AUTO ON` last, always.
- **`FIX NONE` is not optional.** A leftover base coordinate silently prevents a rover
  solution, and the failure looks like "RTK never fixes" rather than like a configuration
  error.
- **Base-station leftovers.** The unit came configured to *transmit* corrections. Unlog
  COM2/COM3 as well — we dropped the QD-02 data link, and there is no reason to keep feeding
  a port nothing is listening on.
- **Reply parsing.** The firmware only looks for `Invalid` in the reply. A command can be
  accepted syntactically and still be a no-op if the feature is not authorized — check
  `LOG VERSION`.
- **Don't save a baud change** unless you also change `GNSS_BAUD` and reflash.
- **Talker IDs vary.** `$GPGGA` vs `$GNGGA` is normal, and neither breaks anything.
- **Board revisions vary.** The boot sequence was confirmed by upstream on one K803 **Lite**
  from one ECOVACS base station; it has not been re-confirmed on the full K803. If your unit
  behaves differently, please open an issue with `LOG VERSION` output and the boot log.
- **`SET APPSCENE` replaces `RTKDYNAMICS` and `RTKOBSMODE`.** Both are absent from V3.1
  and not supported by FW 7.4.6. Sending them will return `Error! Invalid key word!`.
- **IMU commands are optional.** `INSCONTROL ENABLE` and `SET SMOOTHEDDR ON` are silently
  rejected on K803 Lite. This is expected and not an error.
- **`SYSCONFIG` is read-only via `LOG`.** There is no `SET SYSCONFIG` command — the
  `SYSCONFIG` keyword in comnav.md was an error. Use individual commands (e.g. `PPSCONTROL`,
  `SET RELAYRTCMV3`) and verify via `LOG SYSCONFIG`.

---

Next: **[FLASHING.md](FLASHING.md)** — getting the firmware onto the board.