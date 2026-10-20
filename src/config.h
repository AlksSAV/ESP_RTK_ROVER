#pragma once

// ---------------------------------------------------------------------------
// Wiring / UART  -  Seeed XIAO ESP32-C6
// ---------------------------------------------------------------------------
// Defaults below match the XIAO pads; platformio.ini repeats them as -D flags,
// so changing them here and there is redundant but harmless.
//
//   GNSS TX (blue)  ->  D7 / GPIO17  ->  ESP32-C6 RX
//   GNSS RX (green) ->  D6 / GPIO16  ->  ESP32-C6 TX
//
// The C6 has no Bluetooth Classic radio, so the phone link is BLE only.

#ifndef GNSS_UART_NUM
#define GNSS_UART_NUM 1
#endif
#ifndef PIN_GNSS_RX
#define PIN_GNSS_RX 17  // ESP32 in  <- GNSS TX  (NMEA)
#endif
#ifndef PIN_GNSS_TX
#define PIN_GNSS_TX 16  // ESP32 out -> GNSS RX  (RTCM corrections)
#endif

#define GNSS_BAUD 115200

// Tie GNSS GND to the board GND. The receiver draws 150-300 mA during
// acquisition; feed it from VBUS, not from a 3V3 regulator.

// ---------------------------------------------------------------------------
// Receiver init - ComNav K803 as an RTK rover
// ---------------------------------------------------------------------------
// Runs once in setup(), before the bridge starts, and replaces the PC-side
// Python setup script. Safe to repeat every boot: it first pulls the port back
// out of RTCM auto-detect, where the receiver ignores ASCII commands.
//
// Set to 0 for a receiver that is already configured, or one that does not
// speak the ComNav/NovAtel command set (u-blox, Septentrio, ...).
#define GNSS_INIT_K803 1

// The receiver port this ESP32 UART is wired to - LOG targets and the final
// INTERFACEMODE name it. Not the PC's COM port.
#define GNSS_CMD_PORT "COM1"

// Per-command settle time. The whole sequence costs roughly
// 2 s + (commands x this), so ~12 s of boot before the bridge comes up.
#define GNSS_CMD_WAIT_MS 600

// Commands that stay silent after all their retries are counted; once that
// count reaches this threshold the receiver is treated as dead (NO_REPLY)
// rather than merely unhappy (WARN). Below the threshold a silenced command
// is logged but the bridge still starts normally.
#ifndef GNSS_INIT_NO_REPLY_THRESHOLD
#define GNSS_INIT_NO_REPLY_THRESHOLD 3
#endif

// ---------------------------------------------------------------------------
// Phone link name (BLE advertisement)
// ---------------------------------------------------------------------------
// BLE appends "-XXXX", the last two octets of the radio's own address, so two
// rovers in the same field are tellable apart in a scan list and a phone's bond
// for one is never reused for the other.
#define LINK_DEVICE_NAME "ESP32-RTK"

// ---------------------------------------------------------------------------
// BLE
// ---------------------------------------------------------------------------
#define BLE_TX_POWER_DBM 9  // -12..+20; higher = more range, more current
#define BLE_ENABLE_BONDING 1

// Nordic UART Service - what SW Maps, Lefebure, GNSS Master expect over BLE.
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_CHAR_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // phone writes
#define NUS_TX_CHAR_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // phone subscribes

#define BLE_NOTIFY_MAX 244  // payload at MTU 247; clamped to real MTU at runtime
#define BLE_DOWNLINK_BUF 4096

// ---------------------------------------------------------------------------
// Buffering
// ---------------------------------------------------------------------------
#define UART_RX_BUF 4096  // GNSS -> us; must absorb a radio stall
#define UART_TX_BUF 2048  // us -> GNSS
#define BRIDGE_CHUNK 512  // max bytes moved per pump in either direction

// Flush a partial packet after this long with no new GNSS bytes, so the tail of
// an NMEA burst is not held back waiting for a full chunk.
#define UPLINK_FLUSH_MS 12

// ---------------------------------------------------------------------------
// 1 PPS (optional)
// ---------------------------------------------------------------------------
// Enable to capture the 1 PPS time-pulse from the GNSS receiver on a GPIO pin.
// The receiver must be configured to output PPS (PPSCONTROL ENABLE ...).
// Frequency (Hz) is reported in the periodic stats line on the USB console.
// Not needed for basic GIS work; useful for debugging and time-sync projects.
#ifndef USE_PPS
#define USE_PPS 1
#endif
#ifndef PIN_PPS
#define PIN_PPS 2  // GPIO2 / D2 / A2 - interrupt-capable pin on XIAO ESP32-C6
#endif

// ---------------------------------------------------------------------------
// Task watchdog
// ---------------------------------------------------------------------------
// The core leaves loopTask unsubscribed from the task watchdog, so a loop()
// that stops returning - stuck in a BLE call, spinning on a dead UART - leaves
// the rover streaming nothing and giving no sign of life. With the watchdog on,
// that becomes a reboot and a working bridge within a few seconds, which is the
// right outcome for a unit left on a pole.
//
// Only loopTask is subscribed. The idle task is deliberately NOT: while NMEA is
// streaming, loop() returns without yielding on almost every pass, so idle
// would look starved during completely normal operation.
//
// Subscribed at the end of setup(), not the start: receiver init blocks for
// ~12 s in setup() and would trip the watchdog mid-init.
#ifndef USE_TASK_WDT
#define USE_TASK_WDT 1
#endif
#ifndef TASK_WDT_TIMEOUT_MS
#define TASK_WDT_TIMEOUT_MS 5000
#endif

// ---------------------------------------------------------------------------
// Wi-Fi AP + WebSocket (optional)
// ---------------------------------------------------------------------------
// Enable to broadcast NMEA over a Wi-Fi access point in addition to BLE.
// The ESP32-C6 can run BLE and Wi-Fi simultaneously — they share the RF
// front-end but use different time slots, so the UART pipe is not affected.
//
// When enabled, a WebSocket server listens on WEBSOCKET_PORT and mirrors
// every NMEA byte that arrives on the uplink. rtk-monitor.html detects
// the transport and switches between BLE (Web Bluetooth) and WebSocket.
//
// iOS Safari supports WebSockets but drops them when the page goes to the
// background — the browser kills all background tasks. Android Chrome works
// better because it keeps WebSocket open more reliably.
#ifndef USE_WIFI_AP
#define USE_WIFI_AP 1
#endif
#ifndef WIFI_AP_SSID
#define WIFI_AP_SSID "ESP32-RTK"
#endif
#ifndef WIFI_AP_PASS
#define WIFI_AP_PASS "12345678"
#endif
#ifndef WEBSOCKET_PORT
#define WEBSOCKET_PORT 81
#endif

// ---------------------------------------------------------------------------
// Debug / LED
// ---------------------------------------------------------------------------
#define DEBUG_USB 1  // stats + link state on the USB console (UART0)
#define DEBUG_STATS_MS 5000

#define USE_STATUS_LED 1
#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 1  // XIAO ESP32-C6 built-in LED is active-low
#endif
