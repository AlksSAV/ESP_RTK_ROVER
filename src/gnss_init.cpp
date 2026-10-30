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
//
// maxRetries — how many times to retry a command that returns no reply.
// Commands that answer "Invalid" are not retried (the receiver is alive and
// the command is genuinely unsupported), so REJECTED never triggers a retry.
struct Cmd {
  const char *text;
  bool expectReply;
  uint8_t maxRetries;
};

// Order matters. INTERFACEMODE ... AUTO AUTO ON is the point of no return: it
// puts the port into RTCM auto-detect, after which it no longer parses ASCII,
// so every LOG and every setting has to be in place before it.
//
// maxRetries=2 on commands that may need a brief settle time (e.g. UNLOGALL
// clears a log before the next command lands). Most commands get one shot.
static const Cmd kCommands[] = {
    {"UNLOGALL " GNSS_CMD_PORT, true, 2},  // (V3.1 2.2.31)
    {"UNLOGALL COM2", false, 1},           // (V3.1 2.2.31) - for legacy boards wired to COM2
    {"UNLOGALL COM3", false, 1},           // (V3.1 2.2.31) - for legacy boards wired to COM3

    {"FIX NONE", true, 1},  // clear a leftover base coordinate (V3.1 2.2.6 has FIX position,
                            // FIX NONE may not be documented but works on shipped units)
    {"SET APPSCENE Survey", true, 2},  // (V3.1 2.2.24) rover survey mode — replaces RTKDYNAMICS
    {"UNLOCKOUTALL", true, 1},         // (V3.1 2.2.28)

    // Filter RTCM by base station ID. "ANY" lets all RTCM3 frames through the
    // RTK engine; without it a station-specific ID silently discards corrections
    // from any other base. Idempotent, accepted on FW 7.4.6.
    {"RTKSOURCE RTCMV3 ANY", true, 1},  // (V3.1 2.2.20)

    // A rover gets corrections over RTCM, not from a geostationary SBAS
    // satellite. Left enabled, SBAS can pull the solution to GGA quality 2
    // (DGPS/SBAS) and compete with the RTCM path. Only this spelling is
    // accepted on 7.4.6 (SBASSYS DISABLE and SET SBAS OFF are rejected), and
    // the setting does not survive SAVECONFIG on an unknown board, so it has
    // to be re-asserted every boot.
    {"SET SIGNAL SBAS OFF", true, 1},  // (V3.1 2.2.24)

    // INS / dead-reckoning is deliberately NOT enabled. The K803 has an IMU but
    // no tilt compensation, so DR adds no accuracy to a stop-and-go pole rover -
    // and once INS is engaged the receiver starts reporting GGA quality 6
    // (GNSS/INS integrated) instead of an honest 4 (RTK FIXED), which a GIS app
    // accepts as a valid fix. See TODO item 5.

    {"LOG " GNSS_CMD_PORT " GPGGA ONTIME 1", true, 1},
    {"LOG " GNSS_CMD_PORT " GPGST ONTIME 1", true, 1},
    {"LOG " GNSS_CMD_PORT " GPGSA ONTIME 1", true, 1},
    {"LOG " GNSS_CMD_PORT " GPGSV ONTIME 1", true, 1},
    {"LOG " GNSS_CMD_PORT " GPRMC ONTIME 1", true, 1},
    {"LOG " GNSS_CMD_PORT " GPVTG ONTIME 1", true, 1},
    {"LOG " GNSS_CMD_PORT " GPZDA ONTIME 1", true, 1},

    {"LOG COMCONFIG", true, 1},  // verify BEFORE the port goes into diff mode

#if USE_PPS
    // (V3.1 2.2.15) 1 Hz pulse, positive polarity, 1000 ms width - the pin is
    // read on the ESP32 side (main.cpp), the receiver only has to emit it.
    // A unit without PPS support answers Invalid; that lands in the WARN count
    // on purpose, so the missing time reference is visible at boot.
    {"PPSCONTROL ENABLE POSITIVE 1 1000 1", true, 1},
#endif

    // SAVECONFIG has to come BEFORE the port switches to RTCM auto-detect.
    // Measured on FW 7.4.6: a port in AUTO parses no ASCII whatsoever - even a
    // garbage keyword draws no reply - so everything sent after INTERFACEMODE
    // is shouted into a port that is not listening. Kept as a real command with
    // expectReply so the boot log proves the config actually persisted.
    // Costs ~400 ms (flash write), the slowest reply in the sequence, which
    // still fits inside GNSS_CMD_WAIT_MS.
    {"SAVECONFIG", true, 1},  // (V3.1 2.2.22) persist config

    // LAST - after this the port no longer takes commands
    {"INTERFACEMODE " GNSS_CMD_PORT " AUTO AUTO ON", false, 1},  // (V3.1 2.2.10)
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
    const Cmd &cmd = kCommands[i];

    // Only silence is retried. A command answered "Invalid" proves the UART is
    // alive - the receiver parses ASCII but does not support this command, so
    // repeating it only costs another settle window. Commands that are not
    // expected to answer get exactly one attempt, silence being their norm.
    Reply r = REPLY_NONE;
    for (uint8_t sent = 0; sent < cmd.maxRetries; ++sent) {
      r = sendCmd(ser, cmd.text, GNSS_CMD_WAIT_MS);
      if (r != REPLY_NONE || !cmd.expectReply) break;
    }

    if (r == REPLY_REJECTED) ++st.rejected;
    else if (r == REPLY_NONE && cmd.expectReply) ++st.noReply;
  }

  while (ser.available()) ser.read();  // drop the echo tail before bridging

  // Three silenced commands after all retries is not a flaky answer, it is a
  // link that died mid-sequence (receiver brownout, loose TX line). Everything
  // after the point where it went quiet is unconfigured, so this is reported as
  // NO_REPLY rather than a soft WARN: a half-configured unit still streams
  // NMEA and still plots a point, but RTK never fixes.
  if (st.noReply >= GNSS_INIT_NO_REPLY_THRESHOLD) {
    st.result = GNSS_INIT_NO_REPLY;
    DBG("[GNSS] NO REPLY - %u command(s) unanswered, receiver link is dead\n",
        st.noReply);
  } else if (st.rejected) {
    // Rejected commands are the silent failure mode: NMEA still streams, a GIS
    // app still plots a point, but a refused FIX NONE leaves the unit in base
    // mode and RTK never fixes. Worth more than a line in a boot log nobody
    // reads while standing in a field, hence the counters reaching all the way
    // to main.
    st.result = GNSS_INIT_WARN;
    DBG("[GNSS] init done with %u REJECTED command(s)\n", st.rejected);
  } else {
    DBG("[GNSS] init done - %s is in RTCM auto-detect mode\n", GNSS_CMD_PORT);
  }
  if (st.noReply && st.result != GNSS_INIT_NO_REPLY)
    DBG("[GNSS] %u command(s) got no reply\n", st.noReply);

  return st;
}

#else  // GNSS_INIT_K803

GnssInitStatus gnssInit(HardwareSerial &) {
  return {GNSS_INIT_SKIPPED, 0, 0};
}

#endif  // GNSS_INIT_K803
