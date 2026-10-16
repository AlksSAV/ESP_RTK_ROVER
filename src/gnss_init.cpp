// ComNav K803 - configure as an RTK rover over the same UART the bridge
// then uses. Port of the PC-side Python setup script, so the receiver no longer
// needs a laptop before a survey.
//
// Command reference: ComNav OEM Board Reference Manual V3.1.
// Commands marked with (V3.1) are confirmed in the manual.
// Commands without it were verified on the K803 Lite and likely work on
// the full board too, but are not documented in V3.1.

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

// Order matters. INTERFACEMODE ... AUTO AUTO ON is the point of no return: it
// puts the port into RTCM auto-detect, after which it no longer parses ASCII,
// so every LOG and every setting has to be in place before it.
static const char *const kCommands[] = {
    "UNLOGALL " GNSS_CMD_PORT,       // (V3.1 2.2.31)
    "UNLOGALL COM2",                 // (V3.1 2.2.31) - for legacy boards wired to COM2
    "UNLOGALL COM3",                 // (V3.1 2.2.31) - for legacy boards wired to COM3

    "FIX NONE",                      // clear a leftover base coordinate (V3.1 2.2.6 has FIX position,
                                     // FIX NONE may not be documented but works on shipped units)
    "SET APPSCENE Survey",           // (V3.1 2.2.24) rover survey mode — replaces RTKDYNAMICS
    "UNLOCKOUTALL",                  // (V3.1 2.2.28)

    // IMU / DR - only available on full K803, not K803 Lite
    // If the unit does not support these, they are silently rejected (not fatal).
    "INSCONTROL ENABLE",             // (V3.1 2.2.23) enable INS navigation
    "SET SMOOTHEDDR ON",             // (V3.1 4.5.4) enable dead-reckoning smoothing

    "LOG " GNSS_CMD_PORT " GPGGA ONTIME 1",
    "LOG " GNSS_CMD_PORT " GPGST ONTIME 1",
    "LOG " GNSS_CMD_PORT " GPGSA ONTIME 1",
    "LOG " GNSS_CMD_PORT " GPGSV ONTIME 1",
    "LOG " GNSS_CMD_PORT " GPRMC ONTIME 1",
    "LOG " GNSS_CMD_PORT " GPVTG ONTIME 1",
    "LOG " GNSS_CMD_PORT " GPZDA ONTIME 1",

    "LOG COMCONFIG",  // verify BEFORE the port goes into diff mode

    // LAST - after this the port no longer takes commands
    "INTERFACEMODE " GNSS_CMD_PORT " AUTO AUTO ON",  // (V3.1 2.2.10)
    "SAVECONFIG",                                 // (V3.1 2.2.22) persist config
};

static char resp[1024];  // LOG VERSION can exceed 512 bytes on some firmwares

// Check for "Error!" — the exact rejection prefix in ComNav FW 7.4.6.
// Do NOT search for "ERROR" as a substring: it can appear in normal output
// (e.g. "ERRORCODE 0") and would trigger a false REJECTED.
static bool isRejected(const char *resp) {
  return strstr(resp, "Invalid") != nullptr ||
         strstr(resp, "Error!")   != nullptr;
}

// Mirrors the script's send(): drop whatever was streaming, write the command,
// then collect for the whole window rather than stopping at the first newline -
// the receiver may still be emitting a log while it answers.
static void sendCmd(HardwareSerial &ser, const char *cmd, uint32_t waitMs) {
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
  if (n && isRejected(resp))
    DBG("    ^^ REJECTED: %s\n", cmd);
}

void gnssInit(HardwareSerial &ser) {
  DBG("\n[GNSS] K803 rover init on %s\n", GNSS_CMD_PORT);

  // A previous run left the port in RTCM auto-detect, where it ignores ASCII.
  // This is the escape hatch back to the command console, so it has to go first
  // even on a cold receiver, where it is simply a no-op.
  sendCmd(ser, "INTERFACEMODE COMPASS COMPASS ON", 1000);
  sendCmd(ser, "LOG VERSION", 1000);

  for (size_t i = 0; i < sizeof(kCommands) / sizeof(kCommands[0]); ++i)
    sendCmd(ser, kCommands[i], GNSS_CMD_WAIT_MS);

  while (ser.available()) ser.read();  // drop the echo tail before bridging

  DBG("[GNSS] init done - %s is in RTCM auto-detect mode\n", GNSS_CMD_PORT);
}

#else   // GNSS_INIT_K803

void gnssInit(HardwareSerial &) {}

#endif  // GNSS_INIT_K803
