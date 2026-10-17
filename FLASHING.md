# FLASHING.md — Getting the Firmware onto the XIAO ESP32C6

Three routes, easiest first:

| Route | Needs | For |
|---|---|---|
| **[Web flashing](#1-web-flashing-1-click)** | Chrome or Edge, a USB cable | **XIAO ESP32C6 only.** No toolchain, no IDE, no drivers |
| **[PlatformIO](#2-platformio-from-source)** | VS Code + PlatformIO | Anyone changing `config.h` or the K803 commands |
| **[esptool](#3-esptool-manual)** | Python + esptool | CI, scripted installs, awkward setups |

- [Web flashing (1-click)](#1-web-flashing-1-click)
- [PlatformIO (from source)](#2-platformio-from-source)
- [esptool (manual)](#3-esptool-manual)
- [First boot — what a good log looks like](#first-boot--what-a-good-log-looks-like)
- [Publishing your own installer page (maintainers)](#publishing-your-own-installer-page-maintainers)
- [Troubleshooting](#troubleshooting)

---

## 1. Web flashing (1-click)

### ▶ [https://alkssav.github.io/ESP_RTK_ROVER/install/](https://alkssav.github.io/ESP_RTK_ROVER/install/)

[ESP Web Tools](https://esphome.github.io/esp-web-tools/) drives the ESP32 ROM bootloader
straight from the browser over Web Serial. No Arduino IDE, no PlatformIO, and the XIAO's
native USB needs no driver at all.

### Requirements

| | |
|---|---|
| Browser | **Chrome or Edge, desktop, version 89+**. Any Chromium-based browser with Web Serial |
| Not supported | Safari, Firefox (no Web Serial) |
| Page | Must be served over **HTTPS** (or `localhost`) |
| Cable | A **data** USB cable. Charge-only cables enumerate nothing and are a classic time sink |

### Steps

1. Open **[the installer](https://alkssav.github.io/ESP_RTK_ROVER/install/)**.
2. Plug the XIAO into USB-C. **Disconnect the receiver wiring first** — flash it standalone.
3. Click **Connect**.
4. Choose the serial port in the browser's dialog — `USB JTAG/serial debug unit` or similar.
5. Click **Install**, confirm the erase prompt, and wait ~1–2 minutes.
6. When it finishes, click **Logs & Console** to watch the first boot at 115200.

### If the board is not detected

Put it into download mode by hand:

| Board | How |
|---|---|
| XIAO ESP32-C6 | Hold **BOOT (B)**, tap **RESET (R)**, release **BOOT** |

Then click **Connect** again. Some clones need this every time; that is normal for boards
without the auto-reset transistor pair.

---

## 2. PlatformIO (from source)

The route to take the moment you want to change a pin, the device name, the NMEA rate, or the
K803 command sequence.

### Setup

1. Install [VS Code](https://code.visualstudio.com/), then the **PlatformIO IDE** extension.
2. Clone and open the project:

   ```powershell
   git clone https://github.com/AlksSAV/ESP_RTK_ROVER.git
   cd ESP_RTK_ROVER
   code .
   ```

First build downloads the toolchain, the [pioarduino](https://github.com/pioarduino/platform-espressif32)
ESP32 platform (Arduino core 3.x, required for the C6) and NimBLE. Expect a few minutes once.

### Build and upload

```powershell
pio run -t upload    # XIAO ESP32C6, BLE (default env)
pio run              # build only
pio run -t clean     # clean
pio run -t erase     # full chip erase
pio device list      # find the port
pio device monitor   # console @ 115200
pio device monitor -p COM7   # ...on a specific port
```

The monitor is configured with `esp32_exception_decoder` and timestamps, so a crash gives you
a decoded backtrace rather than raw addresses.

### Things worth changing in `src/config.h`

| Setting | Why you would touch it |
|---|---|
| `GNSS_BAUD` | Your receiver's COM1 is not at 115200 |
| `LINK_DEVICE_NAME` | You run more than one rover |
| `GNSS_INIT_K803` | Set `0` for an already-configured or non-ComNav receiver |
| `BLE_TX_POWER_DBM` | More BLE range, at the cost of current |
| `DEBUG_USB` | Set `0` to silence the console |

Pins come from `platformio.ini` and override the `config.h` defaults — change them there, not
in the header.

---

## 3. esptool (manual)

```powershell
pip install esptool
```

### Flash offsets

| Chip | Bootloader | Partitions | boot_app0 | Application |
|---|---|---|---|---|
| ESP32-C6 (XIAO) | `0x0` | `0x8000` | `0xe000` | `0x10000` |

The bootloader offset differs by chip generation. Getting it wrong gives you a board that
never leaves the ROM loader.

### Flash the artifacts a PlatformIO build produced

```powershell
esptool --chip esp32c6 --port COM7 --baud 921600 write_flash -z `
  0x0     .pio/build/xiao_esp32c6_ble/bootloader.bin `
  0x8000  .pio/build/xiao_esp32c6_ble/partitions.bin `
  0x10000 .pio/build/xiao_esp32c6_ble/firmware.bin
```

`boot_app0.bin` ships with the Arduino framework package
(`~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin`) and is
only needed for OTA-capable layouts.

Erase everything:

```powershell
esptool --chip esp32c6 --port COM7 erase_flash
```

> esptool v5 renamed some subcommands to hyphenated forms (`write-flash`, `erase-flash`,
> `merge-bin`). If a command is rejected, try the other spelling.

---

## First boot — what a good log looks like

Open the console at **115200**. On a XIAO you may need to reconnect after flashing — native
USB re-enumerates.

```
[GNSS] K803 rover init on COM1
[>] INTERFACEMODE COMPASS COMPASS ON
[<] $command,INTERFACEMODE COMPASS COMPASS ON,response: OK*…
[>] LOG VERSION
[<] #VERSIONA,COM1,0,…;…,"K803",…
[>] UNLOGALL COM1
[<] $command,UNLOGALL COM1,response: OK*…
…
[>] INTERFACEMODE COM1 AUTO AUTO ON
[<] $command,INTERFACEMODE COM1 AUTO AUTO ON,response: OK*…
[GNSS] init done - COM1 is in RTCM auto-detect mode
[BLE] advertising as "ESP32-RTK-4AAE" (xx:xx:xx:xx:4a:ae)
[SYS] ESP32-RTK-4AAE via BLE - UART1 @ 115200 (RX=GPIO17, TX=GPIO16)
[STAT] BLE waiting  up 0 B/s (0)  down 0 B/s (0)
```

**Reading it:**

| Line | Means |
|---|---|
| `[>]` / `[<]` | Command sent / receiver's reply |
| `[<] (no response)` on **every** line | Wiring, power or baud — the receiver is not talking. [HARDWARE.md](HARDWARE.md#bench-test-before-you-go-out) |
| `^^ REJECTED: …` | Link is fine, that specific command is unsupported. [COMMANDS.md](COMMANDS.md) |
| `[SYS] … via BLE` | Confirms which build is actually running |
| `[STAT] … up 512 B/s` | Receiver is streaming **and** a phone is linked |
| `[STAT] … down 240 B/s` | RTCM corrections are reaching the receiver |

The whole init takes about **12 seconds** before Bluetooth comes up. That is expected — 16
commands at 600 ms each, plus a 2 s preamble.

> The init sequence runs before the link starts on purpose: the receiver echoes its replies,
> and a phone that connected early would otherwise be fed the entire command transcript as if
> it were NMEA.

---

## Publishing your own installer page (maintainers)

ESP Web Tools wants one **merged** binary plus a small JSON manifest, served over HTTPS.
This repo publishes exactly one: the XIAO ESP32C6 BLE build. Everything lives in `install/`.

### 1. Build

```powershell
pio run -e xiao_esp32c6_ble
```

### 2. Take the merged image

PlatformIO already produces one — no `esptool merge_bin` step needed:

```powershell
copy .pio\build\xiao_esp32c6_ble\firmware.factory.bin install\firmware\xiao-c6-ble.bin
```

`firmware.factory.bin` is bootloader + partition table + application pre-merged, flashable at
offset `0`. Verify it before publishing: **byte 0 must be `0xE9`** on the C6, because its
bootloader lives at `0x0`.

```powershell
'0x{0:X2}' -f [System.IO.File]::ReadAllBytes('install\firmware\xiao-c6-ble.bin')[0]
```

### 3. Manifest

`install/manifest-xiao-c6-ble.json`:

```json
{
  "name": "ESP_RTK_ROVER · Seeed Studio XIAO ESP32C6 (BLE)",
  "version": "1.0.0",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-C6",
      "parts": [{ "path": "firmware/xiao-c6-ble.bin", "offset": 0 }]
    }
  ]
}
```

`new_install_prompt_erase: true` offers a full erase on first install — worth keeping, since
it clears whatever partition layout the board arrived with.

> If your esp-web-tools version rejects `"ESP32-C6"`, upgrade it. C6 support arrived with
> newer esptool-js releases.

### 4. Installer page

`install/index.html`:

```html
<script type="module"
        src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>

<esp-web-install-button manifest="manifest-xiao-c6-ble.json">
  <span slot="unsupported">Use Chrome or Edge on desktop.</span>
  <span slot="not-allowed">This page must be served over HTTPS.</span>
</esp-web-install-button>
```

The `unsupported` and `not-allowed` slots are worth filling in — without them, Safari and
Firefox users see a button that simply does nothing.

> **Publish one board, not five.** The image is keyed to a specific chip *and* a specific
> pinout. The XIAO ESP32C6 is a single, known, unambiguous board, so a prebuilt image is
> safe.

### 5. Publish

GitHub Pages is already serving this repo from **`main` / root**, so committing the
`install/` folder is all it takes — the installer lands at
`https://alkssav.github.io/ESP_RTK_ROVER/install/`, alongside the
[dashboard](https://alkssav.github.io/ESP_RTK_ROVER/web/rtk-monitor.html).

Serve the manifest and the `.bin` from the **same origin** as the page, or configure CORS.
Bump `version` in the manifest on every release so returning users are offered the update.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| No port in the browser's dialog | Charge-only cable, board not in download mode | Swap the cable first — it is usually the cable. Then manual boot mode |
| `Failed to connect: No serial data received` | Not in download mode | Hold BOOT, tap RESET, release BOOT, retry |
| `A fatal error occurred: MD5 of file does not match` | Bad cable or a flaky hub | Short cable, direct port, drop the baud to 115200 |
| Flash succeeds, board boot-loops | Wrong bootloader offset | Full erase, then reflash. C6 = `0x0` |
| `invalid header: 0xffffffff`, `TG0WDT_SYS_RESET` **with the GNSS attached** | **Power sag, not firmware** | [HARDWARE.md → Power](HARDWARE.md#power) |
| Console silent on a XIAO | Native USB re-enumerates after flashing | Reopen the monitor. The firmware waits up to 1.5 s for the host at boot |
| Console prints garbage | Baud mismatch | 115200 |
| `[<] (no response)` on every init line | GNSS not connected, wrong baud, TX/RX swapped | [HARDWARE.md](HARDWARE.md#connections) |
| Board fine on USB, dies when the GNSS is plugged in | Shared power rail | Separate supply, share only GND |

**Bisecting a hardware problem:** flash and verify the board **standalone** first. Only then
add GND, then VCC, then the data lines — one at a time. Whichever step breaks it names the
culprit.

---

Next: back to the **[README](README.md)** for pairing and app setup, or
**[COMMANDS.md](COMMANDS.md)** to change what the receiver outputs.