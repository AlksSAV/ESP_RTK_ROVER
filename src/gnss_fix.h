#pragma once

// Passive GGA reader - tells the caller what the receiver's own fix solution
// currently is, for the status LED and the stats line.
//
// Deliberately not a stream parser: the bridge stays byte-transparent, so this
// only watches the bytes on their way to the phone and never delays, changes or
// drops one. Nothing here can break the RTCM3 downlink or an NMEA sentence that
// is split across two UART reads.

#include <stddef.h>
#include <stdint.h>

enum GnssFix : uint8_t {
  GNSS_FIX_NONE,    // no GGA yet, quality 0, or the stream went quiet
  GNSS_FIX_SINGLE,  // a position, but not an RTK one (quality 1/2/3/6/7/8)
  GNSS_FIX_FLOAT,   // RTK float (quality 5)
  GNSS_FIX_FIXED,   // RTK fixed (quality 4)
};

// Feed whatever was just taken off the GNSS UART, in order and exactly once.
void gnssFixFeed(const uint8_t *data, size_t len);

// Current state, already accounting for a stalled stream: a receiver that has
// stopped talking has no position, whatever the last GGA said.
GnssFix gnssFixState();

// "no fix" / "single" / "float" / "rtk fixed" - for logs.
const char *gnssFixName(GnssFix fix);
