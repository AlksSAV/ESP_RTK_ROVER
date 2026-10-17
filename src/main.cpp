// ESP_RTK_ROVER - transparent phone <-> UART bridge for an RTK GNSS receiver.
//
//   GNSS TX --> RX pin --> UART --> Bluetooth --> phone   (NMEA / UBX)
//   GNSS RX <-- TX pin <-- UART <-- Bluetooth <-- phone   (RTCM3 corrections)
//
// Byte-transparent: never parses, reframes or waits for line endings, so binary
// RTCM3 and UBX pass through intact. The Bluetooth side is either Classic SPP
// or BLE depending on the build env - see link.h.

#include <Arduino.h>

#include "config.h"
#include "gnss_init.h"
#include "link.h"

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

// Blink pattern encodes the receiver init outcome:
//   linked   → slow blink  (2 s period, 60 ms on)
//   waiting  → fast blink  (600 ms period, 300 ms on)
//   warn     → double flash (1 s period, 2×60 ms on, short pause)
//   no reply → triple flash (1 s period, 3×60 ms on, short pauses)
enum LedPattern : uint8_t {
  LED_PATTERN_LINKED,
  LED_PATTERN_WAITING,
  LED_PATTERN_WARN,
  LED_PATTERN_NO_REPLY,
};
static LedPattern curPattern = LED_PATTERN_WAITING;

static void ledSetPattern(LedPattern p) { curPattern = p; }

// WAITING/LINKED follow the phone; an init-failure pattern is latched and
// survives both connecting and disconnecting - the fault does not go away
// because a phone showed up.
static void ledTask() {
  const uint32_t t = millis();
  LedPattern p = curPattern;
  if (p == LED_PATTERN_LINKED || p == LED_PATTERN_WAITING)
    p = linkReady() ? LED_PATTERN_LINKED : LED_PATTERN_WAITING;

  const bool blinkOn =
      (p == LED_PATTERN_LINKED && (t % 2000) < 60) ||
      (p == LED_PATTERN_WAITING && (t % 600) < 300) ||
      (p == LED_PATTERN_WARN &&
       (t % 1000 < 60 || (t % 1000 > 200 && (t % 1000) < 260))) ||
      (p == LED_PATTERN_NO_REPLY &&
       (t % 1000 < 60 || (t % 1000 > 150 && (t % 1000) < 210) ||
        (t % 1000 > 300 && (t % 1000) < 360)));
  ledSet(blinkOn);
}
#else
static void ledInit() {}
static void ledTask() {}
#endif

// ---------------------------------------------------------------------------
// Uplink: GNSS -> phone
// ---------------------------------------------------------------------------
static bool pumpUplink() {
  static uint8_t buf[BRIDGE_CHUNK];
  static size_t len = 0;
  static uint32_t lastByte = 0;

  // Nobody listening: drain the UART anyway so it never overflows and resumes
  // mid-sentence when a phone does connect.
  if (!linkReady()) {
    bool had = false;
    while (GnssSerial.available()) {
      GnssSerial.read();
      had = true;
    }
    len = 0;
    return had;
  }

  size_t limit = linkMaxChunk();
  if (limit > sizeof(buf)) limit = sizeof(buf);

  while (GnssSerial.available() && len < limit) {
    buf[len++] = (uint8_t)GnssSerial.read();
    lastByte = millis();
  }

  if (len == 0) return false;

  // Send a full chunk at once; hold a partial one briefly in case the rest of
  // the burst is still arriving.
  if (len < limit && (millis() - lastByte) < UPLINK_FLUSH_MS) return true;

  if (linkWrite(buf, len)) {
    bytesToPhone += len;
    len = 0;
  } else {
    delay(2);  // transport congested - keep the data, retry next loop
  }
  return true;
}

// ---------------------------------------------------------------------------
// Downlink: phone -> GNSS
// ---------------------------------------------------------------------------
static bool pumpDownlink() {
  uint8_t buf[BRIDGE_CHUNK];

  size_t room = GnssSerial.availableForWrite();
  if (room == 0) return false;  // let the UART drain, come back next loop
  if (room > sizeof(buf)) room = sizeof(buf);

  size_t n = linkRead(buf, room);
  if (n == 0) return false;

  GnssSerial.write(buf, n);
  bytesToGnss += n;
  return true;
}

// ---------------------------------------------------------------------------
#if DEBUG_USB
static void printStats() {
  static uint32_t last = 0;
  static uint32_t upPrev = 0, downPrev = 0;

  uint32_t now = millis();
  if (now - last < DEBUG_STATS_MS) return;

  float secs = (now - last) / 1000.0f;
  last = now;

  Serial.printf("[STAT] %s %s  up %.0f B/s (%lu)  down %.0f B/s (%lu)",
                linkName(), linkReady() ? "linked" : "waiting",
                (bytesToPhone - upPrev) / secs, (unsigned long)bytesToPhone,
                (bytesToGnss - downPrev) / secs, (unsigned long)bytesToGnss);

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

  // Before the link, not after: the receiver echoes its replies, and a phone
  // that connected early would otherwise be fed the whole command transcript.
  gnssStatus = gnssInit(GnssSerial);

  // Pick an LED pattern that survives into the loop and survives a phone
  // disconnect (a WARN or NO_REPLY is not transient).
  ledSetPattern(gnssStatus.result == GNSS_INIT_NO_REPLY
                    ? LED_PATTERN_NO_REPLY
                    : (gnssStatus.result == GNSS_INIT_WARN ? LED_PATTERN_WARN
                                                           : LED_PATTERN_WAITING));

  linkBegin();

  DBG("\n[SYS] %s via %s - UART%d @ %d (RX=GPIO%d, TX=GPIO%d)",
      linkDeviceName(), linkName(), GNSS_UART_NUM, GNSS_BAUD, PIN_GNSS_RX,
      PIN_GNSS_TX);
  if (gnssStatus.result == GNSS_INIT_WARN)
    DBG(" - init WARN (rejected=%u, noReply=%u)\n", gnssStatus.rejected,
        gnssStatus.noReply);
  else if (gnssStatus.result == GNSS_INIT_NO_REPLY)
    DBG(" - init NO REPLY (receiver not answering)\n");
  else
    DBG("\n");
}

void loop() {
  bool busy = pumpUplink();
  busy |= pumpDownlink();

  ledTask();
#if DEBUG_USB
  printStats();
#endif

  // Yield when there is nothing to move. A tight loop() starves the idle task
  // on this core, which eventually trips the task watchdog. 1 ms is far below
  // the UPLINK_FLUSH_MS window, so it costs no throughput.
  if (!busy) delay(1);
}
