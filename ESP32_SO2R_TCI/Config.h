// Config.h — EEPROM-backed configuration for the ESP32 SO2R/TCI controller.
// Holds WiFi credentials + hostname + TWO radio endpoints (one per BPF).
//
// Layout (packed, ESP32 EEPROM emulation):
//   magic    (u32)  = 0xBF50C0DE
//   version  (u16)  = 2  (incremented from the ESP8266 schema)
//   wifi_ssid     [32]
//   wifi_pass     [64]
//   hostname      [32]
//   radio1_host   [48]
//   radio1_port    (u16)
//   radio1_iaru    (u8)   // 1 | 2 | 3
//   radio2_host   [48]
//   radio2_port    (u16)
//   radio2_iaru    (u8)
//   crc32          (u32)  // covers all bytes preceding it
//
// On magic / version / CRC mismatch the controller boots into AP-portal mode.
#ifndef BPF_CONFIG_H
#define BPF_CONFIG_H

#include <Arduino.h>
#include <EEPROM.h>

namespace bpf {

constexpr uint32_t kConfigMagic   = 0xBF50C0DEu;
constexpr uint16_t kConfigVersion = 2;   // distinct from ESP8266 schema
constexpr size_t   kEepromSize    = 384; // generous; fits the struct + slack

struct __attribute__((packed)) Config {
  uint32_t magic;
  uint16_t version;
  char     wifi_ssid[32];
  char     wifi_pass[64];
  char     hostname[32];
  char     radio1_host[48];
  uint16_t radio1_port;
  uint8_t  radio1_iaru;
  char     radio2_host[48];
  uint16_t radio2_port;
  uint8_t  radio2_iaru;
  uint32_t crc32_value;
};

static_assert(sizeof(Config) <= kEepromSize, "Config must fit in EEPROM page");

inline uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc >> 1) ^ (0xEDB88320u & -(int32_t)(crc & 1));
    }
  }
  return ~crc;
}

inline void defaults(Config& c, const char* defHostname) {
  memset(&c, 0, sizeof(c));
  c.magic   = kConfigMagic;
  c.version = kConfigVersion;
  strncpy(c.hostname,    defHostname,    sizeof(c.hostname)    - 1);
  strncpy(c.radio1_host, "192.168.1.20", sizeof(c.radio1_host) - 1);
  c.radio1_port = 50001;        // ExpertSDR3 / SunSDR TCI default
  c.radio1_iaru = 1;
  strncpy(c.radio2_host, "192.168.1.21", sizeof(c.radio2_host) - 1);
  c.radio2_port = 50001;
  c.radio2_iaru = 1;
}

inline bool load(Config& out) {
  EEPROM.begin(kEepromSize);
  for (size_t i = 0; i < sizeof(Config); i++) {
    ((uint8_t*)&out)[i] = EEPROM.read(i);
  }
  EEPROM.end();
  if (out.magic   != kConfigMagic)   return false;
  if (out.version != kConfigVersion) return false;
  uint32_t want = out.crc32_value;
  uint32_t got  = crc32((const uint8_t*)&out, sizeof(Config) - sizeof(uint32_t));
  return got == want;
}

inline void save(Config& c) {
  c.magic   = kConfigMagic;
  c.version = kConfigVersion;
  c.crc32_value = crc32((const uint8_t*)&c, sizeof(Config) - sizeof(uint32_t));
  EEPROM.begin(kEepromSize);
  for (size_t i = 0; i < sizeof(Config); i++) {
    EEPROM.write(i, ((const uint8_t*)&c)[i]);
  }
  EEPROM.commit();
  EEPROM.end();
}

inline void factoryReset() {
  EEPROM.begin(kEepromSize);
  for (size_t i = 0; i < kEepromSize; i++) EEPROM.write(i, 0xFF);
  EEPROM.commit();
  EEPROM.end();
}

}  // namespace bpf

#endif
