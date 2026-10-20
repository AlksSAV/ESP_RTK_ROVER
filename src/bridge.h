#pragma once

// Transport bridge - aggregates BLE and optional Wi-Fi WebSocket into one
// interface.  main.cpp talks only to bridge*() and never sees which
// transports are active.
//
// The Wi-Fi functions are implemented in link_wifi.cpp, which compiles to
// no-op stubs when USE_WIFI_AP is 0, so the bridge needs no conditional
// code of its own for them.

#include <stddef.h>
#include <stdint.h>

#include "link.h"

// Start every transport: BLE, and - if enabled - the Wi-Fi AP plus its
// HTTP/WebSocket servers.
void bridgeBegin();

// True when at least one transport has a connected client that can receive
// data.  The main loop uses this to decide whether to drain the GNSS UART.
bool bridgeReady();

// Write a chunk of NMEA to every ready transport.  Returns false when any
// ready transport refused the chunk (BLE congested) - the caller keeps the
// data and retries, exactly as with the single-transport linkWrite().
bool bridgeWrite(const uint8_t *data, size_t len);

// Largest payload worth handing to bridgeWrite() in one call.  A browser
// client has no 20-byte notification limit, so with Wi-Fi connected the
// chunk is only bounded by BRIDGE_CHUNK; with BLE alone it stays at the
// negotiated MTU.
size_t bridgeMaxChunk();

// Read corrections from whichever transport has data.  BLE has priority:
// the phone app pushes RTCM through it and expects low latency.
size_t bridgeRead(uint8_t *buf, size_t max);

// Which transports have a client right now ("BLE", "Wi-Fi", "BLE+Wi-Fi",
// "no link"). For the periodic stats line, where the point is to see who
// is actually attached.
const char *bridgeName();

// Which transports this build runs ("BLE" or "BLE+Wi-Fi"). Independent of
// whether anyone is connected, so the boot log can name the radios before
// the first client arrives - which is what linkName() used to report.
const char *bridgeTransports();

// Pump the HTTP/WebSocket state machines.  A no-op when Wi-Fi is disabled.
void bridgeFeed();

// Implemented in link_wifi.cpp.
void linkWiFiBegin();
bool linkWiFiReady();
bool linkWiFiWrite(const uint8_t *data, size_t len);
size_t linkWiFiRead(uint8_t *buf, size_t max);
void linkWiFiFeed();
