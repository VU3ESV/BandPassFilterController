/*
 * BandPassFilterController — 5B4AGN TXBPF
 *
 * Watches one radio over TCP (Kenwood IF;) and drives the 6 band-select
 * lines of a 5B4AGN TXBPF for contest bands. WARC bands, out-of-band, parse
 * failures, and disconnects all fall through to bypass (all 6 outputs LOW).
 *
 * Pinout, EEPROM layout, web portal, and radio-protocol details are in the
 * project docs (../docs/) and ../CLAUDE.md.
 *
 * Board: Generic ESP8266 Module (ESP-12E/F) on the 8-relay carrier used by
 *        .support/ESP12TCPClientV6.ino.
 * Core:  ESP8266 Arduino 3.1.x
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>

#include "Config.h"
#include "BandPlan.h"
#include "RadioInterface.h"
#include "WebPortal.h"

namespace bpf {

constexpr const char* kDefaultHostname = "bpf-5b4agn";
constexpr const char* kFilterLabel     = "5B4AGN TXBPF";

Config         g_cfg;
RadioInterface g_radio;
WebPortal      g_web;
DNSServer      g_dns;
bool           g_apMode   = false;
Band           g_lastBand = BAND_BYPASS;

String statusJson() {
  String j;
  j.reserve(220);
  j += "{";
  j += "\"filter\":\"";      j += kFilterLabel;                                       j += "\",";
  j += "\"ap_mode\":";       j += (g_apMode ? "true" : "false");                       j += ",";
  j += "\"wifi\":\"";        j += (WiFi.status() == WL_CONNECTED ? "up" : "down");     j += "\",";
  j += "\"ip\":\"";          j += (g_apMode ? WiFi.softAPIP().toString()
                                            : WiFi.localIP().toString());              j += "\",";
  j += "\"rssi\":";          j += WiFi.RSSI();                                         j += ",";
  j += "\"radio_connected\":"; j += (g_radio.connected() ? "true" : "false");          j += ",";
  j += "\"last_freq_hz\":";  j += g_radio.frequencyHz();                               j += ",";
  j += "\"last_band\":\"";   j += bandName(g_lastBand);                                j += "\",";
  j += "\"uptime_s\":";      j += (millis() / 1000);
  j += "}";
  return j;
}

void startApMode() {
  g_apMode = true;
  WiFi.mode(WIFI_AP);
  char ssid[32];
  uint32_t id = ESP.getChipId();
  snprintf(ssid, sizeof(ssid), "BPF-Setup-%06X", id & 0xFFFFFF);
  WiFi.softAP(ssid);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[wifi] AP '%s' at %s\n", ssid, ip.toString().c_str());
  g_dns.start(53, "*", ip);
}

void startStaMode() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(g_cfg.hostname);
  WiFi.begin(g_cfg.wifi_ssid, g_cfg.wifi_pass);
  Serial.printf("[wifi] STA connecting to '%s'", g_cfg.wifi_ssid);
  uint32_t deadline = millis() + 20000;
  while (WiFi.status() != WL_CONNECTED && (int32_t)(millis() - deadline) < 0) {
    delay(200);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] STA up, ip=%s\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin(g_cfg.hostname)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[mdns] http://%s.local/\n", g_cfg.hostname);
    }
  } else {
    Serial.println("[wifi] STA failed; falling back to AP portal");
    startApMode();
  }
}

}  // namespace bpf

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("BandPassFilterController :: 5B4AGN TXBPF"));

  bpf::pinSetup();  // outputs LOW (bypass) before WiFi starts

  if (!bpf::load(bpf::g_cfg)) {
    Serial.println(F("[cfg] no valid config, loading defaults"));
    bpf::defaults(bpf::g_cfg, bpf::kDefaultHostname);
  }

  if (bpf::g_cfg.wifi_ssid[0] != '\0') bpf::startStaMode();
  else                                  bpf::startApMode();

  bpf::g_radio.configure(bpf::g_cfg.radio_host,
                         bpf::g_cfg.radio_port,
                         bpf::g_cfg.poll_interval_ms);
  bpf::g_web.begin(bpf::g_cfg, bpf::statusJson);
}

void loop() {
  bpf::g_web.tick();
  if (bpf::g_apMode) bpf::g_dns.processNextRequest();
  MDNS.update();

  if (!bpf::g_apMode && WiFi.status() == WL_CONNECTED) {
    bpf::g_radio.tick();
    bpf::Band target = bpf::g_radio.stale()
                         ? bpf::BAND_BYPASS
                         : bpf::bandFor(bpf::g_radio.frequencyHz());
    if (target != bpf::g_lastBand) {
      bpf::drive(target);
      bpf::g_lastBand = target;
      Serial.printf("[band] %s @ %ld Hz\n",
                    bpf::bandName(target), bpf::g_radio.frequencyHz());
    }
  } else if (bpf::g_lastBand != bpf::BAND_BYPASS) {
    bpf::drive(bpf::BAND_BYPASS);
    bpf::g_lastBand = bpf::BAND_BYPASS;
    Serial.println(F("[band] bypass (wifi down or AP mode)"));
  }

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line == "reset") {
      Serial.println(F("[serial] factory reset"));
      bpf::factoryReset();
      delay(200);
      ESP.restart();
    } else if (line == "status") {
      Serial.println(bpf::statusJson());
    }
  }
}
