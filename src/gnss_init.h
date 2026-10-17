#pragma once

// One-shot receiver setup, run from setup() before the bridge starts.
//
// Compiles to an empty call unless GNSS_INIT_K803 is set in config.h, so the
// bridge stays a pure byte pipe for receivers that are already configured.

#include <Arduino.h>

// Outcome of the sequence, reported so the caller can surface it. The bridge
// starts either way - a phone can still push commands over the downlink - but
// a silent or partially rejecting receiver has to be visible, not inferred
// from missing data minutes later.
enum GnssInitResult : uint8_t {
  GNSS_INIT_OK,         // receiver answered, every command accepted
  GNSS_INIT_WARN,       // receiver answered, but some commands were rejected
  GNSS_INIT_NO_REPLY,   // nothing on the UART - wiring, power or a hung unit
  GNSS_INIT_SKIPPED,    // init compiled out, receiver taken as configured
};

struct GnssInitStatus {
  GnssInitResult result;
  uint8_t rejected;  // commands answered with Invalid / Error!
  uint8_t noReply;   // commands that got no answer where one was expected
};

GnssInitStatus gnssInit(HardwareSerial &ser);
