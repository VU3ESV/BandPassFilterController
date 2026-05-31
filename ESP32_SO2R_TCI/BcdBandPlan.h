// BcdBandPlan.h — frequency → Yaesu band-data BCD code.
//
// Implements the standard Yaesu "Table 5" BCD band-select assignment:
//
//   Band  BCD (DCBA)  decimal
//   160m  0001        1
//   80m   0010        2
//   40m   0011        3
//   30m   0100        4
//   20m   0101        5
//   17m   0110        6
//   15m   0111        7
//   12m   1000        8
//   10m   1001        9
//   6m    1010        10
//
//   60m / out-of-band / disconnect: 0000  (all lines HIGH = bypass)
//
// Output convention (inherited from the earlier MQTT-driven ESP32
// build, minus the inhibit line — the 8 relays on this carrier are
// fully consumed by the two 4-bit BCD buses):
//   * BCD lines are ACTIVE-LOW. A bit set in the band code = the
//     corresponding GPIO is driven LOW (relay energised on this carrier).
//   * At boot, every output is driven HIGH (= idle / relay OFF).
//   * On 60 m, OOB, disconnect or parse failure the band code is 0,
//     which translates to all four BCD lines HIGH — the bypass state.
//
// Note for contest-BPF use: the 5B4AGN TXBPF and Hamation BandPasser II
// only ship sections for the six contest bands (160/80/40/20/15/10).
// On 30/17/12/6 m the filter will see a valid Yaesu code but no
// matching internal section — it'll bypass (or follow whatever its own
// table maps that code to). Use a downstream BCD switch / antenna
// selector if you need 30/17/12/6 m switching.
#ifndef BPF_BCD_BAND_PLAN_H
#define BPF_BCD_BAND_PLAN_H

#include <Arduino.h>

namespace bpf {

struct BcdResult {
  uint8_t code;     // 4-bit Yaesu BCD code, 0..15. 0 = "no band".
  bool    inhibit;  // true = assert inhibit (BPF should bypass).
};

// Pick a "nearest WARC" BCD code for bypass states (tune-up, OOB,
// 60 m, link-down). Why not just emit 0000? The Hamation BandPasser II
// reads 0000 as "160 m" via its internal decoder — exactly the
// opposite of bypass, and dangerous when the radio is mid-tune at
// high power on a different band. Hamation has its own built-in
// bypass relay that engages when the decoder sees a band the filter
// has no section for; a WARC code triggers it. The 5B4AGN behaves
// identically: with no matching contest section, the bank simply
// stays de-energised (= bypass).
//
// The "nearest" choice is cosmetic — any WARC code triggers Hamation's
// bypass — but picking a band adjacent to the radio's last freq keeps
// the BCD bus visually sensible if you scope it during tune.
inline BcdResult nearestWarcBypass(long hz) {
  if (hz <= 0)            return {4, true};  // unknown freq -> 30 m
  if (hz < 14000000L)     return {4, true};  // 160/80/40 m -> 30 m
  if (hz < 21000000L)     return {6, true};  // 20 m        -> 17 m
  return {8, true};                            // 15/10/6 m   -> 12 m
}

// Hz units. 60 m and out-of-band → inhibit=true, code=0 (bypass).
inline BcdResult bcdFor(long hz) {
  if (hz >= 1800000L  && hz <= 2000000L)  return { 1,  false };  // 160 m
  if (hz >= 3500000L  && hz <= 4000000L)  return { 2,  false };  // 80  m
  if (hz >= 7000000L  && hz <= 7300000L)  return { 3,  false };  // 40  m
  if (hz >= 10100000L && hz <= 10150000L) return { 4,  false };  // 30  m
  if (hz >= 14000000L && hz <= 14350000L) return { 5,  false };  // 20  m
  if (hz >= 18068000L && hz <= 18168000L) return { 6,  false };  // 17  m
  if (hz >= 21000000L && hz <= 21450000L) return { 7,  false };  // 15  m
  if (hz >= 24890000L && hz <= 24990000L) return { 8,  false };  // 12  m
  if (hz >= 28000000L && hz <= 29700000L) return { 9,  false };  // 10  m
  if (hz >= 50000000L && hz <= 54000000L) return { 10, false };  // 6   m
  return { 0, true };                                              // bypass
}

inline const char* bandName(uint8_t code, bool inhibit) {
  if (inhibit) return "bypass";
  switch (code) {
    case 1:  return "160m";
    case 2:  return "80m";
    case 3:  return "40m";
    case 4:  return "30m";
    case 5:  return "20m";
    case 6:  return "17m";
    case 7:  return "15m";
    case 8:  return "12m";
    case 9:  return "10m";
    case 10: return "6m";
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
  // r.code == 0 (OOB / 60 m / disconnect) -> all lines HIGH = bypass.
  // r.inhibit is no longer wired anywhere; it just rides along for status.
  digitalWrite(b.pinA, (r.code & 0x1) ? LOW : HIGH);
  digitalWrite(b.pinB, (r.code & 0x2) ? LOW : HIGH);
  digitalWrite(b.pinC, (r.code & 0x4) ? LOW : HIGH);
  digitalWrite(b.pinD, (r.code & 0x8) ? LOW : HIGH);
}

}  // namespace bpf

#endif
