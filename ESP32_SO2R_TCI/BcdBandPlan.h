// BcdBandPlan.h — frequency → Yaesu band-data BCD code + inhibit signal.
//
// Output convention (matches .support/ESP32MQTTSwitchV2.ino):
//   * BCD lines are ACTIVE-LOW. A bit set in the band code = the
//     corresponding GPIO is driven LOW.
//   * INHIBIT is ACTIVE-LOW. The line is driven LOW when the controller
//     wants the BPF in its bypass / inhibit state (WARC bands, 60 m, 6 m,
//     out-of-band, disconnect, parse failure).
//   * At boot, every output is driven HIGH (= idle). The relay carrier
//     used in this design treats HIGH as relay-OFF.
//
// Yaesu BCD band codes (standard):
//   160m=1 (0001)  80m=2 (0010)  40m=3 (0011)
//   20m=5  (0101)  15m=7 (0111)  10m=9 (1001)
// WARC bands (30/17/12), 60 m, 6 m and out-of-band assert INHIBIT instead.
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

// One BCD output bank (4 BCD pins + 1 inhibit). Active-LOW logic.
struct BcdBank {
  uint8_t pinA, pinB, pinC, pinD, pinInhibit;
};

inline void setupBank(const BcdBank& b) {
  pinMode(b.pinA,       OUTPUT);
  pinMode(b.pinB,       OUTPUT);
  pinMode(b.pinC,       OUTPUT);
  pinMode(b.pinD,       OUTPUT);
  pinMode(b.pinInhibit, OUTPUT);
  // Idle = HIGH (relay OFF on the active-LOW carrier).
  digitalWrite(b.pinA,       HIGH);
  digitalWrite(b.pinB,       HIGH);
  digitalWrite(b.pinC,       HIGH);
  digitalWrite(b.pinD,       HIGH);
  digitalWrite(b.pinInhibit, HIGH);  // not inhibited at boot
}

inline void driveBank(const BcdBank& b, BcdResult r) {
  // Active-LOW: bit set in code → drive line LOW.
  digitalWrite(b.pinA, (r.code & 0x1) ? LOW : HIGH);
  digitalWrite(b.pinB, (r.code & 0x2) ? LOW : HIGH);
  digitalWrite(b.pinC, (r.code & 0x4) ? LOW : HIGH);
  digitalWrite(b.pinD, (r.code & 0x8) ? LOW : HIGH);
  // Inhibit active-LOW.
  digitalWrite(b.pinInhibit, r.inhibit ? LOW : HIGH);
}

}  // namespace bpf

#endif
