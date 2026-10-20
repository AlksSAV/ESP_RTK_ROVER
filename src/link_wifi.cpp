// Wi-Fi AP + WebSocket transport - the browser side of the bridge.
//
// Runs in parallel with BLE: the C6 shares one RF front-end between the two
// radios, time-slicing between them, which is fine at NMEA/RTCM byte rates.
//
//   HTTP (port 80)  serves the dashboard page - a phone connects to the AP,
//                   opens http://192.168.4.1/ and gets the same monitor the
//                   PC uses over Web Bluetooth.
//   WS   (port 81)  binary frames: NMEA out, RTCM/commands in.
//
// The page is the same rtk-monitor.html from web/, embedded at build time
// (see tools/embed_html.py) so the rover works with no other infrastructure.

#include <Arduino.h>
#include <freertos/stream_buffer.h>

#include "config.h"

#if USE_WIFI_AP

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#include "web_dashboard.h"  // RTK_DASHBOARD[] PROGMEM - generated from web/rtk-monitor.html

#if DEBUG_USB
#define DBG(...) Serial.printf(__VA_ARGS__)
#else
#define DBG(...) ((void)0)
#endif

static WebSocketsServer wsServer(WEBSOCKET_PORT);
static WebServer httpServer(80);

// Same pattern as the BLE link: the WebSocket callback must not block on the
// UART, so incoming frames land in a stream buffer and the loop drains them.
static StreamBufferHandle_t wsDownlink = nullptr;

// onEvent runs inside wsServer.loop(), i.e. in loopTask - no ISR, no
// cross-task race, so a plain counter is enough.
static uint8_t clientCount = 0;

static void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED:
      clientCount++;
      DBG("[WS] client %u connected (%u total)\n", num, clientCount);
      break;
    case WStype_DISCONNECTED:
      if (clientCount > 0) clientCount--;
      DBG("[WS] client %u disconnected (%u total)\n", num, clientCount);
      break;
    case WStype_TEXT:
    case WStype_BIN:
      if (wsDownlink && len)
        xStreamBufferSend(wsDownlink, payload, len, 0);
      break;
    default:
      break;
  }
}

void linkWiFiBegin() {
  wsDownlink = xStreamBufferCreate(BLE_DOWNLINK_BUF, 1);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
  DBG("[WiFi] AP \"%s\" at %s\n", WIFI_AP_SSID,
      WiFi.softAPIP().toString().c_str());

  // send_P streams from flash in chunks - the page never lands in RAM.
  httpServer.on("/", []() {
    httpServer.send_P(200, "text/html", RTK_DASHBOARD);
  });
  httpServer.begin();

  wsServer.begin();
  wsServer.onEvent(onWsEvent);
  DBG("[WS] listening on port %d\n", WEBSOCKET_PORT);
}

bool linkWiFiReady() { return clientCount > 0; }

bool linkWiFiWrite(const uint8_t *data, size_t len) {
  if (clientCount == 0) return false;
  return wsServer.broadcastBIN(data, len);
}

size_t linkWiFiRead(uint8_t *buf, size_t max) {
  if (!wsDownlink || xStreamBufferBytesAvailable(wsDownlink) == 0) return 0;
  return xStreamBufferReceive(wsDownlink, buf, max, 0);
}

// Pump HTTP and WebSocket state machines; call from loop().
void linkWiFiFeed() {
  httpServer.handleClient();
  wsServer.loop();
}

#else  // USE_WIFI_AP == 0 - stubs so the bridge compiles either way

void linkWiFiBegin() {}
bool linkWiFiReady() { return false; }
bool linkWiFiWrite(const uint8_t *data, size_t len) {
  (void)data;
  (void)len;
  return true;
}
size_t linkWiFiRead(uint8_t *buf, size_t max) {
  (void)buf;
  (void)max;
  return 0;
}
void linkWiFiFeed() {}

#endif
