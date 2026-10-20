// Bridge - aggregates BLE and the optional Wi-Fi WebSocket into one
// interface, so main.cpp keeps the single-transport shape it always had.
//
// Uplink is duplicated to every ready transport; downlink is taken from
// whichever has data.

#include <Arduino.h>

#include "bridge.h"
#include "config.h"  // BRIDGE_CHUNK

void bridgeBegin() {
  linkBegin();
  linkWiFiBegin();
}

bool bridgeReady() { return linkReady() || linkWiFiReady(); }

bool bridgeWrite(const uint8_t *data, size_t len) {
  bool bleOk = true;
  bool wifiOk = true;

  if (linkReady()) bleOk = linkWrite(data, len);
  if (linkWiFiReady()) wifiOk = linkWiFiWrite(data, len);

  // OR, not AND: on a false the caller keeps the buffer and retries the
  // whole chunk, and re-notifying a transport that already accepted it
  // would duplicate bytes mid-stream and break the client's NMEA framing.
  // Losing a chunk on the congested transport is the lesser evil and
  // matches what the BLE link already does when its notify queue is full.
  return bleOk || wifiOk;
}

size_t bridgeMaxChunk() {
  // A browser client has no 20-byte notification limit, so with Wi-Fi alone
  // the chunk is bounded only by BRIDGE_CHUNK. With BLE in play the chunk
  // has to fit its negotiated MTU, because a notify() larger than the MTU
  // fails and the caller would retry the same bytes forever.
  if (linkReady()) return linkMaxChunk();
  return BRIDGE_CHUNK;
}

size_t bridgeRead(uint8_t *buf, size_t max) {
  // BLE first: that is where a GNSS app pushes RTCM and it is the link
  // that tolerates latency least.
  size_t n = linkRead(buf, max);
  if (n > 0) return n;
  return linkWiFiRead(buf, max);
}

const char *bridgeName() {
  const bool ble = linkReady();
  const bool wifi = linkWiFiReady();
  if (ble && wifi) return "BLE+Wi-Fi";
  if (ble) return "BLE";
  if (wifi) return "Wi-Fi";
  return "no link";
}

const char *bridgeTransports() {
#if USE_WIFI_AP
  return "BLE+Wi-Fi";
#else
  return "BLE";
#endif
}

void bridgeFeed() { linkWiFiFeed(); }
