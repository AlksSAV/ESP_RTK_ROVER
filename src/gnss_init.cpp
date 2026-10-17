// ComNav K803 - configure as an RTK rover over the same UART the bridge
// then uses. Port of the PC-side Python setup script, so the receiver no longer
// needs a laptop before a survey.
//
// Command reference: ComNav OEM Board Reference Manual V3.1.
// Commands marked with (V3.1) are confirmed in the manual. Commands without it
// are not documented in V3.1 but are accepted by shipped units.

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "gnss_init.h"

#if GNSS_INIT_K803

#if DEBUG_USB
#define DBG(...) Serial.printf(__VA_ARGS__)
#else
#define DBG(...) ((void)0)
#endif

// What the receiver did in answer to one command. Silence is a real outcome
// here, not a gap in the log: it is the only sign a dead UART link gives.
enum Reply : uint8_t { REPLY_NONE, REPLY_OK, REPLY_REJECTED };

// expectReply is false where silence is the *normal* answer, so those commands
// cannot pollute the no-reply count: the escape hatch is a no-op on a cold
// receiver, the COM2/COM3 unlogs only target legacy boards, and the last two
// run after the port has already left ASCII parsing.
struct Cmd {
  const char *text;
  bool expectReply;
};

// Order matters. INTERFACEMODE ... AUTO AUTO ON is the point of no return: it
// puts the port into RTCM auto-detect, after which it no longer parses ASCII,
// so every LOG and every setting has to be in place before it.
static const Cmd kCommands[] = {
    {"UNLOGALL " GNSS_CMD_PORT, true},  // (V3.1 2.2.31)
    {"UNLOGALL COM2", false},           // (V3.1 2.2.31) - for legacy boards wired to COM2
    {"UNLOGALL COM3", false},           // (V3.1 2.2.31) - for legacy boards wired to COM3

    {"FIX NONE", true},  // clear a leftover base coordinate (V3.1 2.2.6 has FIX position,
                         // FIX NONE may not be documented but works on shipped units)
    {"SET APPSCENE Survey", true},  // (V3.1 2.2.24) rover survey mode — replaces RTKDYNAMICS
    {"UNLOCKOUTALL", true},         // (V3.1 2.2.28)

    // INS / dead-reckoning is deliberately NOT enabled. The K803 has an IMU but
    // no tilt compensation, so DR adds no accuracy to a stop-and-go pole rover -
    // and once INS is engaged the receiver starts reporting GGA quality 6
    // (GNSS/INS integrated) instead of an honest 4 (RTK FIXED), which a GIS app
    // accepts as a valid fix. See TODO item 5.

    {"LOG " GNSS_CMD_PORT " GPGGA ONTIME 1", true},
    {"LOG " GNSS_CMD_PORT " GPGST ONTIME 1", true},
    {"LOG " GNSS_CMD_PORT " GPGSA ONTIME 1", true},
    {"LOG " GNSS_CMD_PORT " GPGSV ONTIME 1", true},
    {"LOG " GNSS_CMD_PORT " GPRMC ONTIME 1", true},
    {"LOG " GNSS_CMD_PORT " GPVTG ONTIME 1", true},
    {"LOG " GNSS_CMD_PORT " GPZDA ONTIME 1", true},

    {"LOG COMCONFIG", true},  // verify BEFORE the port goes into diff mode

    // LAST - after this the port no longer takes commands
    {"INTERFACEMODE " GNSS_CMD_PORT " AUTO AUTO ON", false},  // (V3.1 2.2.10)
    {"SAVECONFIG", false},  // (V3.1 2.2.22) persist config
};

static char resp[1024];  // LOG VERSION can exceed 512 bytes on some firmwares

// Check for the rejection prefixes shipped receivers actually return.
// Do NOT search for "ERROR" as a substring: it can appear in normal output
// (e.g. "ERRORCODE 0") and would trigger a false REJECTED.
static bool isRejected(const char *resp) {
  return strstr(resp, "Invalid") != nullptr ||
         strstr(resp, "Error!")   != nullptr;
}

// Mirrors the script's send(): drop whatever was streaming, write the command,
// then collect for the whole window rather than stopping at the first newline -
// the receiver may still be emitting a log while it answers.
static Reply sendCmd(HardwareSerial &ser, const char *cmd, uint32_t waitMs) {
  while (ser.available()) ser.read();

  ser.write((const uint8_t *)cmd, strlen(cmd));
  ser.write((const uint8_t *)"\r\n", 2);
  ser.flush();
  DBG("[>] %s\n", cmd);

  size_t n = 0;
  const uint32_t deadline = millis() + waitMs;
  while ((int32_t)(millis() - deadline) < 0) {
    while (ser.available()) {
      int c = ser.read();
      if (n < sizeof(resp) - 1) resp[n++] = (char)c;
    }
    delay(2);
  }
  resp[n] = '\0';
  while (n && (resp[n - 1] == '\r' || resp[n - 1] == '\n' || resp[n - 1] == ' '))
    resp[--n] = '\0';

  DBG("[<] %s\n", n ? resp : "(no response)");

  if (!n) return REPLY_NONE;
  if (isRejected(resp)) {
    DBG("    ^^ REJECTED: %s\n", cmd);
    return REPLY_REJECTED;
  }
  return REPLY_OK;
}

GnssInitStatus gnssInit(HardwareSerial &ser) {
  GnssInitStatus st{};
  st.result = GNSS_INIT_OK;

  DBG("\n[GNSS] K803 rover init on %s\n", GNSS_CMD_PORT);

  // A previous run left the port in RTCM auto-detect, where it ignores ASCII.
  // This is the escape hatch back to the command console, so it has to go first
  // even on a cold receiver, where it is simply a no-op.
  sendCmd(ser, "INTERFACEMODE COMPASS COMPASS ON", 1000);

  // The one command that proves the UART is alive. Without a reply the rest of
  // the sequence cannot land either, and burning 15 x 600 ms on it only delays
  // the bridge by ten seconds while telling us nothing new.
  if (sendCmd(ser, "LOG VERSION", 1000) == REPLY_NONE) {
    st.result = GNSS_INIT_NO_REPLY;
    st.noReply = 1;
    DBG("[GNSS] NO REPLY - receiver not answering, init aborted\n");
    return st;
  }

  for (size_t i = 0; i < sizeof(kCommands) / sizeof(kCommands[0]); ++i) {
    Reply r = sendCmd(ser, kCommands[i].text, GNSS_CMD_WAIT_MS);
    if (r == REPLY_REJECTED) ++st.rejected;
    else if (r == REPLY_NONE && kCommands[i].expectReply) ++st.noReply;
  }

  while (ser.available()) ser.read();  // drop the echo tail before bridging

  // Rejected commands are the silent failure mode: NMEA still streams, a GIS
  // app still plots a point, but a refused FIX NONE leaves the unit in base
  // mode and RTK never fixes. Worth more than a line in a boot log nobody reads
  // while standing in a field, hence the counters reaching all the way to main.
  if (st.rejected) {
    st.result = GNSS_INIT_WARN;
    DBG("[GNSS] init done with %u REJECTED command(s)\n", st.rejected);
  } else {
    DBG("[GNSS] init done - %s is in RTCM auto-detect mode\n", GNSS_CMD_PORT);
  }
  if (st.noReply) DBG("[GNSS] %u command(s) got no reply\n", st.noReply);

  return st;
}

#else  // GNSS_INIT_K803

GnssInitStatus gnssInit(HardwareSerial &) {
  return {GNSS_INIT_SKIPPED, 0, 0};
}

#endif  // GNSS_INIT_K803
