// BandPlan.h — frequency → contest band → 6 band-select GPIOs.
// Pinout matches the 8-relay ESP-12 carrier from .support/ESP12TCPClientV6.ino.
// WARC bands (30/17/12), 60 m, 6 m, and anything out of band fall through to
// BAND_BYPASS (all outputs LOW) — both filters treat that as their bypass
// state.
#ifndef BPF_BANDPLAN_H
#define BPF_BANDPLAN_H

#include <Arduino.h>

namespace bpf {

enum Band : uint8_t {
  BAND_BYPASS = 0,
  BAND_160M,
  BAND_80M,
  BAND_40M,
  BAND_20M,
  BAND_15M,
  BAND_10M,
};

inline const char* bandName(Band b) {
  switch (b) {
    case BAND_BYPASS: return "bypass";
    case BAND_160M:   return "160m";
    case BAND_80M:    return "80m";
    case BAND_40M:    return "40m";
    case BAND_20M:    return "20m";
    case BAND_15M:    return "15m";
    case BAND_10M:    return "10m";
  }
  return "?";
}

// IARU R1/R2/R3 contest band edges, generous (covers all regions + guard).
inline Band bandFor(long hz) {
  if (hz >= 1800000L  && hz <= 2000000L)  return BAND_160M;
  if (hz >= 3500000L  && hz <= 4000000L)  return BAND_80M;
  if (hz >= 7000000L  && hz <= 7300000L)  return BAND_40M;
  if (hz >= 14000000L && hz <= 14350000L) return BAND_20M;
  if (hz >= 21000000L && hz <= 21450000L) return BAND_15M;
  if (hz >= 28000000L && hz <= 29700000L) return BAND_10M;
  return BAND_BYPASS;
}

constexpr uint8_t kPin160 =  5;  // Relay 1 — GPIO5
constexpr uint8_t kPin80  =  4;  // Relay 2 — GPIO4
constexpr uint8_t kPin40  =  0;  // Relay 3 — GPIO0  (boot-sensitive; carrier pulls high)
constexpr uint8_t kPin20  = 15;  // Relay 4 — GPIO15 (boot-sensitive; carrier pulls low)
constexpr uint8_t kPin15  = 13;  // Relay 5 — GPIO13
constexpr uint8_t kPin10  = 12;  // Relay 6 — GPIO12

inline void pinSetup() {
  pinMode(kPin160, OUTPUT);
  pinMode(kPin80,  OUTPUT);
  pinMode(kPin40,  OUTPUT);
  pinMode(kPin20,  OUTPUT);
  pinMode(kPin15,  OUTPUT);
  pinMode(kPin10,  OUTPUT);
  digitalWrite(kPin160, LOW);
  digitalWrite(kPin80,  LOW);
  digitalWrite(kPin40,  LOW);
  digitalWrite(kPin20,  LOW);
  digitalWrite(kPin15,  LOW);
  digitalWrite(kPin10,  LOW);
}

inline void drive(Band b) {
  digitalWrite(kPin160, b == BAND_160M ? HIGH : LOW);
  digitalWrite(kPin80,  b == BAND_80M  ? HIGH : LOW);
  digitalWrite(kPin40,  b == BAND_40M  ? HIGH : LOW);
  digitalWrite(kPin20,  b == BAND_20M  ? HIGH : LOW);
  digitalWrite(kPin15,  b == BAND_15M  ? HIGH : LOW);
  digitalWrite(kPin10,  b == BAND_10M  ? HIGH : LOW);
}

}  // namespace bpf

#endif
