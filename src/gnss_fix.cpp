// Passive GGA reader - see gnss_fix.h.
//
// Costs one byte-wise state machine on data that is already flowing to the
// phone, and a ~80-byte scratch buffer. It never blocks, never allocates and
// never touches the bridge: a sentence split across two UART reads simply
// continues in the scratch buffer.

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "gnss_fix.h"

namespace {

// A GGA is ~80 bytes. Anything longer is not one, and holding it would only
// delay the next sentence, so the accumulator gives up instead.
constexpr size_t kMaxSentence = 100;

char line[kMaxSentence];
size_t lineLen = 0;
bool inSentence = false;

uint8_t lastQuality = 0;
uint32_t lastGgaMs = 0;
bool haveGga = false;

int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// NMEA XOR over everything between '$' and '*', then two hex digits.
// Without this a burst of line noise that happens to contain "GGA," could latch
// a fix the receiver never reported.
bool checksumOk(const char *s, size_t len) {
  const char *star = (const char *)memchr(s, '*', len);
  if (!star || (size_t)(star - s) + 3 > len) return false;

  uint8_t sum = 0;
  for (const char *p = s; p < star; ++p) sum ^= (uint8_t)*p;

  const int hi = hexDigit(star[1]);
  const int lo = hexDigit(star[2]);
  if (hi < 0 || lo < 0) return false;
  return sum == (uint8_t)((hi << 4) | lo);
}

// Talker ID is two characters and varies by constellation (GP, GN, GL, GB, ...),
// so only the sentence ID matters.
bool isGga(const char *s, size_t len) {
  const char *comma = (const char *)memchr(s, ',', len);
  if (!comma) return false;
  const size_t idLen = (size_t)(comma - s);
  return idLen == 5 && memcmp(comma - 3, "GGA", 3) == 0;
}

// Field 0 is the sentence ID, so quality is field 6: after UTC, lat, N/S,
// lon and E/W.
const char *fieldAt(const char *s, const char *end, int index) {
  const char *p = s;
  for (int i = 0; i < index; ++i) {
    const char *comma = (const char *)memchr(p, ',', (size_t)(end - p));
    if (!comma) return nullptr;
    p = comma + 1;
  }
  return p;
}

void handleSentence(const char *s, size_t len) {
  if (!checksumOk(s, len) || !isGga(s, len)) return;

  const char *end = s + len;
  const char *q = fieldAt(s, end, 6);
  if (!q || q >= end || *q < '0' || *q > '9') return;  // empty: no solution yet

  lastQuality = (uint8_t)(*q - '0');
  lastGgaMs = millis();
  haveGga = true;
}

GnssFix qualityToFix(uint8_t q) {
  switch (q) {
    case 4:
      return GNSS_FIX_FIXED;
    case 5:
      return GNSS_FIX_FLOAT;
    case 0:
      return GNSS_FIX_NONE;
    default:
      // 1/2/3 autonomous, DGPS, PPS; 6 is GNSS+INS, which gnss_init.cpp
      // deliberately leaves off; 7/8 manual and simulation. None of them is
      // an RTK solution, and calling any of them "fixed" would be a lie.
      return GNSS_FIX_SINGLE;
  }
}

}  // namespace

void gnssFixFeed(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    const char c = (char)data[i];

    if (c == '$') {  // a new sentence starts here, whatever was buffered is lost
      inSentence = true;
      lineLen = 0;
      continue;
    }
    if (!inSentence) continue;
    if (c == '\r') continue;

    if (c == '\n') {
      inSentence = false;
      if (lineLen > 0) handleSentence(line, lineLen);
      continue;
    }

    if (lineLen >= kMaxSentence) {  // not a sentence we can use
      inSentence = false;
      continue;
    }
    line[lineLen++] = c;
  }
}

GnssFix gnssFixState() {
  // A receiver that stopped talking has no position, whatever the last GGA
  // said. Without this the LED would keep showing a fix from a dead receiver.
  if (!haveGga || (millis() - lastGgaMs) > GNSS_FIX_TIMEOUT_MS)
    return GNSS_FIX_NONE;
  return qualityToFix(lastQuality);
}

const char *gnssFixName(GnssFix fix) {
  switch (fix) {
    case GNSS_FIX_FIXED:
      return "rtk fixed";
    case GNSS_FIX_FLOAT:
      return "float";
    case GNSS_FIX_SINGLE:
      return "single";
    default:
      return "no fix";
  }
}
