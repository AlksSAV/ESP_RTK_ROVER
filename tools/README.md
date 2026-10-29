# tools

Scripts for talking to the K803 directly from a PC, without the ESP32 in the way.

A receiver fault and a firmware fault look identical from the phone — no NMEA, no fix. A
**3.3 V USB-TTL adapter** removes the firmware from the picture: the receiver speaks plain
ASCII over UART, so a PC can configure it, interrogate it and measure it. Wiring is in
[HARDWARE.md → Testing the receiver without the ESP32](../HARDWARE.md#testing-the-receiver-without-the-esp32).

## Requirements

```
pip install pyserial
```

`embed_html.py` additionally needs the PlatformIO environment the rest of the project uses.

## Receiver scripts

All take the Windows port as the first argument and talk 115200 8N1 with CR+LF terminators.
Each one leaves the receiver in `COMPASSNAV` mode, and none of them calls `SAVECONFIG` — a
power cycle always restores the previous state.

| Script | What it does |
|---|---|
| `probe_k803.py` | Listens passively, then sends `LOG VERSION`. Extra arguments are sent as commands |
| `replay_boot.py` | Sends exactly the firmware's `kCommands[]`, in order, and prints a per-command verdict |
| `test_saveconfig.py` | Checks whether `SAVECONFIG` is heard after `INTERFACEMODE … AUTO AUTO ON` |
| `test_autoparse.py` | Separates "the port stopped parsing ASCII" from "this command is unsupported" |
| `test_ae.py` | Steps `SET AE` through `100M` / `OFF` / `25M` and counts satellites at each |
| `test_ecutoff.py` | Drops the elevation mask to 0° and samples `GPGSV` for several minutes |
| `scan_spectrum.py` | Sweeps L1 and L5 through `SCANSPECTRUM` + the binary `SPECTRUMB` log |
| `ntrip_to_receiver.py` | Streams RTCM3 from an NTRIP caster into the receiver and watches `GGA` |
| `check_links.py` | Verifies relative links and anchors across the Markdown files |

### Feeding corrections straight into the receiver

The one test that exercises the whole correction path without the ESP32 or a phone:

```powershell
python tools/ntrip_to_receiver.py COM17 2307
```

It reads `host` / `port` / `login` / `pass` from **`.ntrip`** in the project root (one
`key value` pair per line, git-ignored, never printed), switches COM1 to `AUTO`, and pushes
caster bytes into the UART. Then it reports the `GGA` fix quality it observed.

Expected progression on a good sky: `1` (SINGLE) → `5` (RTK FLOAT) → `4` (RTK FIXED). Reaching
`5` proves the corrections are arriving; only a clean environment gets you `4`.

Pick the mount point nearest the rover — a long baseline costs accuracy and time to fix. Base
line and the RTCM message types each mount point serves are visible in the caster's own
statistics page.

### Typical sequence

Start here on any new board:

```powershell
python tools/probe_k803.py COM13
```

If `LOG VERSION` answers, the link works. Then check that the firmware's own configuration
sequence actually lands:

```powershell
python tools/replay_boot.py COM13
```

`VERDICT: OK` means every command was accepted. `WARN` means some command came back
`Error! Invalid key word!` — the link is fine, that command is not supported by this build.
`NO REPLY` means the receiver stopped answering partway through and everything after that
point is unconfigured.

### When there are no satellites

Start outdoors. `$GPGSV,1,1,00` with zero satellites in view is most often a missing sky, not a
missing antenna — glass (especially low-E coated), a balcony slab and a wall can hide everything.

```powershell
python tools/probe_k803.py COM13 "LOG COM1 GPGSV ONTIME 1"   # outside, antenna flat and up
```

If satellites still do not appear, work through the checklist in
[HARDWARE.md → When no satellites appear at all](../HARDWARE.md#when-no-satellites-appear-at-all):
the elevation mask, the DC bias **under load**, the connector, then a substitute antenna.

```powershell
python tools/test_ecutoff.py COM13 300    # a 10 degree mask plus a wall hides the sky
python tools/test_ae.py COM13             # antenna-electronics clock: no effect on a BT-800D
```

### What the spectrum sweep can and cannot show

```powershell
python tools/scan_spectrum.py COM13
```

`SCANSPECTRUM` sweeps the receiver's front end. It is useful for **interference** — a narrow peak
above a flat floor is a real emitter. It is **not** a satellite detector: GNSS signals sit about
20 dB below the noise floor in the swept band, so a working antenna and a dead one both look
flat. Do not conclude "no signal" from a flat profile.

Two traps: levels are in arbitrary units (no dBm calibration in V3.1), and a sweep can return
fewer points than requested — check the point count before comparing two profiles.

The decisive antenna test is a multimeter, not a script: measure DC on the centre pin with the
antenna **disconnected** and then **connected**. A sag under load means the LNA is drawing
current; an unchanged reading means it is not.

## Notes

- **Port name versus port number.** The scripts take a Windows port (`COM13`).
  `GNSS_CMD_PORT` in `src/config.h` is the receiver's own label (`COM1`) and has nothing to do
  with it.
- **`OK!` is not proof.** `LOG TRACKSTATA` answers `OK! Command accepted!` and emits nothing.
  These scripts report what actually came back, not just the acknowledgement.
- **Binary logs need the `B` suffix.** `LOG SPECTRUM` is accepted and silent; `SPECTRUMB`
  produces data. `scan_spectrum.py` handles the ComNav binary header
  (`AA 44 12`, length, message ID, body length, 32-bit CRC).
- **A repeating `CBOOT` banner is a cable fault.** If `probe_k803.py` prints
  `CBOOT … / Loading...` over and over, the receiver is rebooting, not ignoring you. See
  [COMMANDS.md → Recovery](../COMMANDS.md#recovery-when-the-port-stops-answering).

## Firmware helper

| Script | What it does |
|---|---|
| `embed_html.py` | Inlines `web/rtk-monitor.html` into `src/web_dashboard.h` as a `PROGMEM` string for the Wi-Fi AP build. Runs automatically as a PlatformIO extra script before every build — no need to invoke it by hand |
