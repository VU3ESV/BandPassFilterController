// BcdBandPlan.h — frequency → Yaesu band-data BCD code.
//
// Output convention (matches .support/ESP32MQTTSwitchV2.ino, minus the
// inhibit line — the 8 relays on this carrier are fully consumed by the
// two 4-bit BCD buses, leaving no relay for a separate inhibit signal):
//   * BCD lines are ACTIVE-LOW. A bit set in the band code = the
//     corresponding GPIO is driven LOW (relay energised on this carrier).
//   * At boot, every output is driven HIGH (= idle / relay OFF).
//   * On WARC bands, 60 m, 6 m, out-of-band, disconnect, or parse failure
//     the band code is **0**, which translates to all four BCD lines HIGH
//     — the same state the BPF sees with no band data applied, i.e.
//     bypass. Both filters auto-bypass when no band line is asserted.
//
// Yaesu BCD band codes (standard):
//   160m=1 (0001)  80m=2 (0010)  40m=3 (0011)
//   20m=5  (0101)  15m=7 (0111)  10m=9 (1001)
// The `inhibit` flag in BcdResult is retained as an internal status
// signal (drives serial logging and the /status JSON) but no longer
// corresponds to a GPIO.
#ifndef BPF_BCD_BAND_PLAN_H
#define BPF_BCD_BAND_PLAN_H

#include <Arduino.h>

namespace bpf {

struct BcdResult {
  uint8_t code;     // 4-bit Yaesu BCD code, 0..15. 0 = "no band".
  bool    inhibit;  // true = assert inhibit (BPF should bypass).
};

// Hz units. WARC / 6 m / 60 m / out-of-band → inhibit=true, code=0.
inline BcdResult bcdFor(long hz) {
  if (hz >= 1800000L  && hz <= 2000000L)  return { 1, false };  // 160 m
  if (hz >= 3500000L  && hz <= 4000000L)  return { 2, false };  // 80  m
  if (hz >= 7000000L  && hz <= 7300000L)  return { 3, false };  // 40  m
  if (hz >= 14000000L && hz <= 14350000L) return { 5, false };  // 20  m
  if (hz >= 21000000L && hz <= 21450000L) return { 7, false };  // 15  m
  if (hz >= 28000000L && hz <= 29700000L) return { 9, false };  // 10  m
  return { 0, true };                                            // bypass
}

inline const char* bandName(uint8_t code, bool inhibit) {
  if (inhibit) return "bypass";
  switch (code) {
    case 1: return "160m";
    case 2: return "80m";
    case 3: return "40m";
    case 5: return "20m";
    case 7: return "15m";
    case 9: return "10m";
  }
  return "?";
}

// One BCD output bank — 4 BCD pins, active-LOW logic. No inhibit pin
// (all 8 relays on this carrier are spoken for by the two banks combined).
struct BcdBank {
  uint8_t pinA, pinB, pinC, pinD;
};

inline void setupBank(const BcdBank& b) {
  pinMode(b.pinA, OUTPUT);
  pinMode(b.pinB, OUTPUT);
  pinMode(b.pinC, OUTPUT);
  pinMode(b.pinD, OUTPUT);
  // Idle = HIGH (relay OFF on the active-LOW carrier).
  digitalWrite(b.pinA, HIGH);
  digitalWrite(b.pinB, HIGH);
  digitalWrite(b.pinC, HIGH);
  digitalWrite(b.pinD, HIGH);
}

inline void driveBank(const BcdBank& b, BcdResult r) {
  // r.code == 0 (WARC / OOB / disconnect) -> all lines HIGH = bypass.
  // r.inhibit is no longer wired anywhere; it just rides along for status.
  digitalWrite(b.pinA, (r.code & 0x1) ? LOW : HIGH);
  digitalWrite(b.pinB, (r.code & 0x2) ? LOW : HIGH);
  digitalWrite(b.pinC, (r.code & 0x4) ? LOW : HIGH);
  digitalWrite(b.pinD, (r.code & 0x8) ? LOW : HIGH);
}

}  // namespace bpf

#endif
