// BandPlan.h — frequency → contest band → 6 band-select outputs.
//
// On the Hamation BandPasser II these 6 GPIOs do NOT drive the filter
// directly. Each output gates one channel of the 8-relay carrier; the relay
// contacts switch +5–12 V from the user's bench supply onto the matching pin
// of the BandPasser II's rear-panel control connector. With all outputs LOW
// (all relays de-energised) no band voltage reaches the BandPasser II and it
// auto-bypasses, which is exactly the BAND_BYPASS state.
//
// See ../docs/HARDWARE.md for the relay wiring.
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

inline Band bandFor(long hz) {
  if (hz >= 1800000L  && hz <= 2000000L)  return BAND_160M;
  if (hz >= 3500000L  && hz <= 4000000L)  return BAND_80M;
  if (hz >= 7000000L  && hz <= 7300000L)  return BAND_40M;
  if (hz >= 14000000L && hz <= 14350000L) return BAND_20M;
  if (hz >= 21000000L && hz <= 21450000L) return BAND_15M;
  if (hz >= 28000000L && hz <= 29700000L) return BAND_10M;
  return BAND_BYPASS;
}

constexpr uint8_t kPin160 =  5;  // Relay 1 → 160 m control pin
constexpr uint8_t kPin80  =  4;  // Relay 2 → 80 m  control pin
constexpr uint8_t kPin40  =  0;  // Relay 3 → 40 m  control pin
constexpr uint8_t kPin20  = 15;  // Relay 4 → 20 m  control pin
constexpr uint8_t kPin15  = 13;  // Relay 5 → 15 m  control pin
constexpr uint8_t kPin10  = 12;  // Relay 6 → 10 m  control pin

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
