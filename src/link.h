#pragma once

// Transport abstraction for the phone side of the bridge.
// The only implementation is link_ble.cpp - BLE Nordic UART Service on the
// ESP32-C6, which has no Bluetooth Classic radio.
//
// main.cpp only moves bytes and never knows which transport is in use.

#include <stddef.h>
#include <stdint.h>

void linkBegin();

// True when a phone is connected AND actually able to receive data.
bool linkReady();

// Largest payload linkWrite() will accept in one call.
size_t linkMaxChunk();

// Phone-bound. Returns false if the transport is busy; keep the data and retry.
bool linkWrite(const uint8_t *data, size_t len);

// GNSS-bound. Returns bytes copied, 0 if nothing pending.
size_t linkRead(uint8_t *buf, size_t max);

// Short label for the debug console.
const char *linkName();

// The name the phone actually sees. Not always LINK_DEVICE_NAME: BLE builds
// append a per-board suffix, so this is only valid after linkBegin().
const char *linkDeviceName();
