// ESP_RTK_ROVER - transparent phone <-> UART bridge for an RTK GNSS receiver.
//
//   GNSS TX --> RX pin --> UART --> BLE / Wi-Fi --> client   (NMEA)
//   GNSS RX <-- TX pin <-- UART <-- BLE / Wi-Fi <-- client   (RTCM3 corrections)
//
// Byte-transparent: never parses, reframes or waits for line endings, so binary
// RTCM3 passes through intact. Both transports run at once and the uplink is
// duplicated to each - see bridge.h.

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_system.h>

#include "bridge.h"
#include "config.h"
#include "gnss_fix.h"
#include "gnss_init.h"

static HardwareSerial GnssSerial(GNSS_UART_NUM);

static uint32_t bytesToPhone = 0;
static uint32_t bytesToGnss = 0;

// Kept for the whole run: an init fault has to stay visible in the LED pattern
// and in the periodic stats line, not scroll away with the boot log.
static GnssInitStatus gnssStatus = {GNSS_INIT_SKIPPED, 0, 0};

#if DEBUG_USB
#define DBG(...) Serial.printf(__VA_ARGS__)
#else
#define DBG(...) ((void)0)
#endif

// ---------------------------------------------------------------------------
// Status LED
// ---------------------------------------------------------------------------
// Blink pattern reports the receiver's own solution, read passively from GGA
// (see gnss_fix.cpp) - the LED sits on the pole, so "how good is the fix" is
// the only question worth answering there:
//   rtk fixed → steady on    (nothing else is steady, so it reads at a glance)
//   float     → fast blink   (200 ms period, 100 ms on)
//   single    → slow blink   (2 s period, 100 ms on)
//   no fix    → one short flash every 3 s - alive, but no position
//   warn      → double flash (1 s period, 2×60 ms on, short pause)
//   no reply  → triple flash (1 s period, 3×60 ms on, short pauses)
enum LedPattern : uint8_t {
  LED_PATTERN_FIXED,
  LED_PATTERN_FLOAT,
  LED_PATTERN_SINGLE,
  LED_PATTERN_NO_FIX,
  LED_PATTERN_WARN,
  LED_PATTERN_NO_REPLY,
};

#if USE_STATUS_LED && defined(LED_BUILTIN)
static inline void ledSet(bool on) {
#if LED_ACTIVE_LOW
  digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
#else
  digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
#endif
}
static void ledInit() {
  pinMode(LED_BUILTIN, OUTPUT);
  ledSet(false);
}

// An init fault is latched and survives both a later fix and a client
// connecting: the receiver did not answer at boot, and a phone showing up does
// not repair that. Everything else is recomputed from the live GGA stream.
static bool faultLatched = false;
static LedPattern faultPattern = LED_PATTERN_WARN;

static void ledSetPattern(LedPattern p) {
  faultLatched = true;
  faultPattern = p;
}

static LedPattern patternForFix(GnssFix fix) {
  switch (fix) {
    case GNSS_FIX_FIXED:
      return LED_PATTERN_FIXED;
    case GNSS_FIX_FLOAT:
      return LED_PATTERN_FLOAT;
    case GNSS_FIX_SINGLE:
      return LED_PATTERN_SINGLE;
    default:
      return LED_PATTERN_NO_FIX;
  }
}

static void ledTask() {
  const uint32_t t = millis();
  const LedPattern p =
      faultLatched ? faultPattern : patternForFix(gnssFixState());

  bool on = false;
  switch (p) {
    case LED_PATTERN_FIXED:
      on = true;
      break;
    case LED_PATTERN_FLOAT:
      on = (t % 200) < 100;
      break;
    case LED_PATTERN_SINGLE:
      on = (t % 2000) < 100;
      break;
    case LED_PATTERN_NO_FIX:
      on = (t % 3000) < 60;
      break;
    case LED_PATTERN_WARN:
      on = t % 1000 < 60 || (t % 1000 > 200 && (t % 1000) < 260);
      break;
    case LED_PATTERN_NO_REPLY:
      on = t % 1000 < 60 || (t % 1000 > 150 && (t % 1000) < 210) ||
           (t % 1000 > 300 && (t % 1000) < 360);
      break;
  }
  ledSet(on);
}
#else
static void ledInit() {}
static void ledTask() {}
static void ledSetPattern(LedPattern) {}
#endif

// ---------------------------------------------------------------------------
// Uplink: GNSS -> clients
// ---------------------------------------------------------------------------
static bool pumpUplink() {
  static uint8_t buf[BRIDGE_CHUNK];
  static size_t len = 0;
  static uint32_t lastByte = 0;

  // Nobody listening: drain the UART anyway so it never overflows and resumes
  // mid-sentence when a client does connect. The bytes still go through the
  // fix reader - the LED has to work on a rover nobody is connected to.
  if (!bridgeReady()) {
    bool had = false;
    uint8_t sink[64];
    while (GnssSerial.available()) {
      const size_t n = GnssSerial.read(sink, sizeof(sink));
      if (n == 0) break;
      gnssFixFeed(sink, n);
      had = true;
    }
    len = 0;
    return had;
  }

  size_t limit = bridgeMaxChunk();
  if (limit > sizeof(buf)) limit = sizeof(buf);

  while (GnssSerial.available() && len < limit) {
    const uint8_t b = (uint8_t)GnssSerial.read();
    // Fed here, byte by byte, and not from buf: a chunk the transport refuses
    // stays in buf and is retried, and the same GGA must not be counted twice.
    gnssFixFeed(&b, 1);
    buf[len++] = b;
    lastByte = millis();
  }

  if (len == 0) return false;

  // Send a full chunk at once; hold a partial one briefly in case the rest of
  // the burst is still arriving.
  if (len < limit && (millis() - lastByte) < UPLINK_FLUSH_MS) return true;

  if (bridgeWrite(buf, len)) {
    bytesToPhone += len;
    len = 0;
  } else {
    delay(2);  // transport congested - keep the data, retry next loop
  }
  return true;
}

// ---------------------------------------------------------------------------
// Downlink: clients -> GNSS
// ---------------------------------------------------------------------------
static bool pumpDownlink() {
  uint8_t buf[BRIDGE_CHUNK];

  size_t room = GnssSerial.availableForWrite();
  if (room == 0) return false;  // let the UART drain, come back next loop
  if (room > sizeof(buf)) room = sizeof(buf);

  size_t n = bridgeRead(buf, room);
  if (n == 0) return false;

  GnssSerial.write(buf, n);
  bytesToGnss += n;
  return true;
}

// ---------------------------------------------------------------------------
// 1 PPS input (optional)
// ---------------------------------------------------------------------------
// The receiver's time pulse is a hardware signal, so it cannot travel over the
// byte-transparent Bluetooth bridge. Counting it here and reporting the rate on
// the USB console keeps the bridge pure while still making the timing reference
// verifiable: ~1.000 Hz means the receiver locked and PPSCONTROL took effect.
#if USE_PPS
static volatile uint32_t ppsEdges = 0;

static void IRAM_ATTR ppsIsr() { ppsEdges += 1; }

static void ppsInit() {
  pinMode(PIN_PPS, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_PPS), ppsIsr, RISING);
}

static uint32_t ppsSnapshot() {
  noInterrupts();
  uint32_t val = ppsEdges;
  interrupts();
  return val;
}
#else
static void ppsInit() {}
static uint32_t ppsSnapshot() { return 0; }
#endif

// ---------------------------------------------------------------------------
// Task watchdog - protects loop() from BLE/UART hangs
// ---------------------------------------------------------------------------
#if USE_TASK_WDT
static void watchdogInit() {
  // Reconfigure the TWDT: enable loopTask monitoring, keep panic→reboot.
  esp_task_wdt_config_t cfg = {
      .timeout_ms = TASK_WDT_TIMEOUT_MS,
      .idle_core_mask = 0,        // do NOT watch idle task (normal during NMEA streaming)
      .trigger_panic = true       // matches CONFIG_TASK_WDT_PANIC
  };
  esp_task_wdt_reconfigure(&cfg);

  // Subscribe loopTask explicitly.
  // Arduino core creates the task with WDT disabled (loopTaskWDTEnabled=false)
  // because setup() blocks ~13 s on receiver init and would trip the timer.
  if (esp_task_wdt_add(NULL) != ESP_OK) {
    DBG("[SYS] WDT subscribe failed - loop() is unprotected\n");
  } else {
    DBG("[SYS] watchdog %u ms\n", TASK_WDT_TIMEOUT_MS);
  }
}
static void watchdogFeed() {
  esp_task_wdt_reset();
}
#else
static void watchdogInit() {}
static void watchdogFeed() {}
#endif

// ---------------------------------------------------------------------------
#if DEBUG_USB
static void printStats() {
  static uint32_t last = 0;
  static uint32_t upPrev = 0, downPrev = 0;
#if USE_PPS
  static uint32_t ppsPrev = 0;
#endif

  uint32_t now = millis();
  if (now - last < DEBUG_STATS_MS) return;

  float secs = (now - last) / 1000.0f;
  last = now;

  Serial.printf("[STAT] %s %s  up %.0f B/s (%lu)  down %.0f B/s (%lu)",
                bridgeName(), bridgeReady() ? "linked" : "waiting",
                (bytesToPhone - upPrev) / secs, (unsigned long)bytesToPhone,
                (bytesToGnss - downPrev) / secs, (unsigned long)bytesToGnss);

  // The fix is what the LED shows, so it belongs in the same line: when the LED
  // and the app disagree, this says which one is reading the stream.
  Serial.printf("  fix %s", gnssFixName(gnssFixState()));

#if USE_PPS
  // Snapshot first: the ISR keeps running while the line is being formatted.
  uint32_t edges = ppsSnapshot();
  Serial.printf("  pps %.2f Hz (%lu)", (edges - ppsPrev) / secs,
                (unsigned long)edges);
  ppsPrev = edges;
#endif

  if (gnssStatus.result == GNSS_INIT_NO_REPLY)
    Serial.printf("  GNSS NO REPLY");
  else if (gnssStatus.result == GNSS_INIT_WARN)
    Serial.printf("  GNSS WARN rejected=%u", gnssStatus.rejected);

  Serial.printf("\n");

  upPrev = bytesToPhone;
  downPrev = bytesToGnss;
}
#endif

// ---------------------------------------------------------------------------
void setup() {
  ledInit();

#if DEBUG_USB
  Serial.begin(115200);  // UART0 / USB console, separate from the GNSS UART
  // On USB-CDC boards the port only appears once the host enumerates it, well
  // after boot. Without this the whole receiver init log is written into the
  // void. Bounded, so a board running headless still starts.
  for (uint32_t t0 = millis(); !Serial && millis() - t0 < 1500;) delay(10);
#endif

  GnssSerial.setRxBufferSize(UART_RX_BUF);
  GnssSerial.setTxBufferSize(UART_TX_BUF);
  GnssSerial.begin(GNSS_BAUD, SERIAL_8N1, PIN_GNSS_RX, PIN_GNSS_TX);

  // A WDT reset is the one fault a field user cannot see after the fact: the
  // board comes back looking healthy. Say what rebooted it, once, at boot.
  if (esp_reset_reason() == ESP_RST_TASK_WDT)
    DBG("\n[SYS] previous run ended by watchdog reset (loop() hung)\n");

  // Before the link, not after: the receiver echoes its replies, and a phone
  // that connected early would otherwise be fed the whole command transcript.
  gnssStatus = gnssInit(GnssSerial);

  // After init: PPSCONTROL has been sent by now, so a receiver that supports it
  // starts pulsing while the ISR is already counting.
  ppsInit();

  // Latch a failed init; a healthy receiver needs no latch - from here the LED
  // follows the GGA stream, starting at "no fix" until the first sentence.
  if (gnssStatus.result == GNSS_INIT_NO_REPLY)
    ledSetPattern(LED_PATTERN_NO_REPLY);
  else if (gnssStatus.result == GNSS_INIT_WARN)
    ledSetPattern(LED_PATTERN_WARN);

  bridgeBegin();

  // Subscribe loopTask to TWDT after init completes (init blocks ~13 s and
  // would trip the watchdog).  Must happen before the loop starts.
  watchdogInit();

  DBG("\n[SYS] %s via %s - UART%d @ %d (RX=GPIO%d, TX=GPIO%d)",
      linkDeviceName(), bridgeTransports(), GNSS_UART_NUM, GNSS_BAUD,
      PIN_GNSS_RX, PIN_GNSS_TX);
  if (gnssStatus.result == GNSS_INIT_WARN)
    DBG(" - init WARN (rejected=%u, noReply=%u)\n", gnssStatus.rejected,
        gnssStatus.noReply);
  else if (gnssStatus.result == GNSS_INIT_NO_REPLY)
    DBG(" - init NO REPLY (receiver not answering)\n");
  else
    DBG("\n");
}

void loop() {
  // Feed the task watchdog before we reach for the BLE/UART calls.
  // If loop() hangs on a BLE notify() or a dead UART, the TWDT will
  // force a reboot so the rover comes back on its own.
  watchdogFeed();

  bool busy = pumpUplink();
  busy |= pumpDownlink();

  // Pump the HTTP / WebSocket state machines. Cheap with no clients and
  // never blocking - handleClient() yields rather than waiting.
  bridgeFeed();

  ledTask();
#if DEBUG_USB
  printStats();
#endif

  // Yield when there is nothing to move. A tight loop() starves the idle task
  // on this core, which eventually trips the task watchdog. 1 ms is far below
  // the UPLINK_FLUSH_MS window, so it costs no throughput.
  if (!busy) vTaskDelay(pdMS_TO_TICKS(1));
}
