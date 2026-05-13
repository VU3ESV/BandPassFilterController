// Config.h — EEPROM-backed configuration for the Hamation BandPasser II
// controller. Layout & semantics documented in ../docs/CONFIGURATION.md.
#ifndef BPF_CONFIG_H
#define BPF_CONFIG_H

#include <Arduino.h>
#include <EEPROM.h>

namespace bpf {

constexpr uint32_t kConfigMagic   = 0xBF50C0DEu;
constexpr uint16_t kConfigVersion = 1;
constexpr size_t   kEepromSize    = 256;

enum RadioProtocol : uint8_t {
  PROTO_KENWOOD_IF_TCP = 0,
  PROTO_TCI_WS         = 1,  // reserved; bridge via Node-Red for now
};

struct __attribute__((packed)) Config {
  uint32_t magic;
  uint16_t version;
  char     wifi_ssid[32];
  char     wifi_pass[64];
  char     hostname[32];
  char     radio_host[48];
  uint16_t radio_port;
  uint8_t  radio_protocol;
  uint16_t poll_interval_ms;
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
  strncpy(c.hostname,   defHostname,   sizeof(c.hostname)   - 1);
  strncpy(c.radio_host, "192.168.1.10", sizeof(c.radio_host) - 1);
  c.radio_port       = 13013;
  c.radio_protocol   = PROTO_KENWOOD_IF_TCP;
  c.poll_interval_ms = 300;
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
