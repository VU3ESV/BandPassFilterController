// AdminAuth.h — admin password for the web portal and ArduinoOTA.
//
// One password (user `admin`) gates the portal page, every state-changing
// web route and OTA uploads. It lives in NVS (Preferences namespace
// "bpf-auth"), NOT in the EEPROM Config page: adding it needed no Config
// schema bump, so an OTA upgrade from v0.5.x keeps the saved Wi-Fi / radio
// settings, and the secret never has to be compiled into the firmware.
//
// Empty (the state after first flash, factory reset or `auth clear`) means
// no password — the open, pre-0.6.0 behaviour. The portal shows a warning
// banner until one is set.
#ifndef BPF_ADMIN_AUTH_H
#define BPF_ADMIN_AUTH_H

#include <Arduino.h>
#include <Preferences.h>

namespace bpf {

constexpr const char* kAdminUser     = "admin";
constexpr const char* kAuthRealm     = "BandPassFilterController";
constexpr size_t      kAdminPassMax  = 64;
constexpr const char* kAuthNamespace = "bpf-auth";

inline String loadAdminPassword() {
  Preferences p;
  // Read-only begin() fails when the namespace was never written; that
  // is the normal "no password yet" case, not an error.
  if (!p.begin(kAuthNamespace, true)) return String();
  String v = p.getString("pass", "");
  p.end();
  return v;
}

inline void saveAdminPassword(const String& v) {
  Preferences p;
  p.begin(kAuthNamespace, false);
  p.putString("pass", v);
  p.end();
}

inline void clearAdminPassword() {
  Preferences p;
  p.begin(kAuthNamespace, false);
  p.clear();
  p.end();
}

}  // namespace bpf

#endif
