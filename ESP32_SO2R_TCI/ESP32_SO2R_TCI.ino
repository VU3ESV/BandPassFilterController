/*
 * BandPassFilterController — ESP32 SO2R / TCI variant
 *
 * One ESP32 carries two independent TCI-WebSocket clients to two SunSDR /
 * Expert Electronics radios and emits two independent Yaesu-style BCD
 * band-data buses — one per radio. Each BCD bank drives one band-pass
 * filter:
 *
 *   Radio 1  --TCI-->  BPF1 (e.g. 5B4AGN TXBPF)    via BCD bank 1
 *   Radio 2  --TCI-->  BPF2 (e.g. Hamation MBF-100) via BCD bank 2
 *
 * Each BPF is dedicated to its own radio (no SO2R cross-swap; that's a
 * separate firmware mode if needed later — see the MQTT reference at
 * .support/ESP32MQTTSwitchV2.ino).
 *
 * BCD encoding: Yaesu band data — 160m=1, 80m=2, 40m=3, 20m=5, 15m=7,
 * 10m=9. WARC (30/17/12), 60 m, 6 m and out-of-band assert INHIBIT.
 *
 * Configuration is persisted in flash (EEPROM emulation) and editable
 * from a tiny on-device web portal — first boot raises a SoftAP named
 * `BPF-Setup-XXXXXX` reachable at http://192.168.4.1/.
 *
 * Hardware: ESP32 dev board driving an 8-relay active-LOW carrier.
 * Library: TCI by IW7DMH (in .support/TCI-2/; install into the Arduino
 *          libraries folder — see ESP32_SO2R_TCI/README.md).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <TCI.h>

#include "Config.h"
#include "BcdBandPlan.h"
#include "WebPortal.h"
#include "LcdDisplay.h"

namespace bpf {

constexpr const char* kDefaultHostname = "SO2R-BPF";
constexpr const char* kFilterLabel     = "ESP32 SO2R / TCI";

// Pin map — active-LOW; HIGH = idle (relay off). See ESP32_SO2R_TCI/README.md.
//
// 8 relays = 8 pins total; 4 per radio. The 5B4AGN side keeps the BCD
// pins from .support/ESP32MQTTSwitchV2.ino (16/17/18/19) because that
// wiring is already in place. The Hamation side reuses the freed
// R1_Tx/R2_Tx/relay8 pins from the reference plus one spare GPIO.
//
// No inhibit line: code=0 (WARC / OOB / disconnect) sets all four BCD
// lines HIGH on the affected bank, which presents the BPF with "no band
// data applied" — same as the bypass state.
constexpr BcdBank kBank1 = { .pinA = 16, .pinB = 17, .pinC = 18, .pinD = 19 };
constexpr BcdBank kBank2 = { .pinA = 26, .pinB = 27, .pinC = 32, .pinD = 33 };

// Globals.
Config        g_cfg;
TCI           g_radio1;
TCI           g_radio2;
WebPortal     g_web;
LcdDisplay    g_lcd;
DNSServer     g_dns;
bool          g_apMode    = false;
volatile long g_lastFreq1 = 0;
volatile long g_lastFreq2 = 0;
volatile uint8_t g_lastBand1 = 0;
volatile uint8_t g_lastBand2 = 0;
volatile bool    g_lastInh1  = true;
volatile bool    g_lastInh2  = true;

// =============================================================================
// Status JSON for /status.
// =============================================================================

String statusJson() {
  String j;
  j.reserve(320);
  j += "{";
  j += "\"filter\":\""; j += kFilterLabel;                                  j += "\",";
  j += "\"ap_mode\":";  j += (g_apMode ? "true" : "false");                  j += ",";
  j += "\"wifi\":\"";   j += (WiFi.status() == WL_CONNECTED ? "up" : "down");j += "\",";
  j += "\"ip\":\"";     j += (g_apMode ? WiFi.softAPIP().toString()
                                       : WiFi.localIP().toString());         j += "\",";
  j += "\"rssi\":";     j += WiFi.RSSI();                                    j += ",";
  j += "\"r1\":{\"connected\":"; j += (g_radio1.connected() ? "true" : "false");
  j += ",\"freq_hz\":";          j += g_lastFreq1;
  j += ",\"band\":\"";           j += bandName(g_lastBand1, g_lastInh1);     j += "\"},";
  j += "\"r2\":{\"connected\":"; j += (g_radio2.connected() ? "true" : "false");
  j += ",\"freq_hz\":";          j += g_lastFreq2;
  j += ",\"band\":\"";           j += bandName(g_lastBand2, g_lastInh2);     j += "\"},";
  j += "\"uptime_s\":";          j += (millis() / 1000);
  j += "}";
  return j;
}

// =============================================================================
// BCD drive helpers — only push to the bank when the decoded band changes.
// =============================================================================

void applyBand(int radioIndex, long hz) {
  BcdResult r = bcdFor(hz);
  if (radioIndex == 0) {
    if (r.code == g_lastBand1 && r.inhibit == g_lastInh1) return;
    driveBank(kBank1, r);
    g_lastBand1 = r.code;
    g_lastInh1  = r.inhibit;
    Serial.printf("[R1] %s @ %ld Hz (bcd=%u inh=%d)\n",
                  bandName(r.code, r.inhibit), hz, r.code, r.inhibit);
  } else {
    if (r.code == g_lastBand2 && r.inhibit == g_lastInh2) return;
    driveBank(kBank2, r);
    g_lastBand2 = r.code;
    g_lastInh2  = r.inhibit;
    Serial.printf("[R2] %s @ %ld Hz (bcd=%u inh=%d)\n",
                  bandName(r.code, r.inhibit), hz, r.code, r.inhibit);
  }
}

// =============================================================================
// TCI event handlers.
// =============================================================================

void onRadio1Vfo(const int senderRig, const int senderVfo) {
  long hz = g_radio1.rtx[senderRig].getVfo(senderVfo);
  Serial.printf("[R1] vfo evt rig=%d vfo=%d hz=%ld\n",
                senderRig, senderVfo, hz);
  if (senderRig != 0 || senderVfo != 0) return;  // only main RX, VFO A
  if (hz <= 0) return;
  g_lastFreq1 = hz;
  applyBand(0, hz);
  g_lcd.setFreq(0, hz);
}

void onRadio2Vfo(const int senderRig, const int senderVfo) {
  long hz = g_radio2.rtx[senderRig].getVfo(senderVfo);
  Serial.printf("[R2] vfo evt rig=%d vfo=%d hz=%ld\n",
                senderRig, senderVfo, hz);
  if (senderRig != 0 || senderVfo != 0) return;
  if (hz <= 0) return;
  g_lastFreq2 = hz;
  applyBand(1, hz);
  g_lcd.setFreq(1, hz);
}

void onRadio1Modulation(const int senderRig) {
  g_lcd.setMode(0, g_radio1.rtx[senderRig].getModulation());
}
void onRadio2Modulation(const int senderRig) {
  g_lcd.setMode(1, g_radio2.rtx[senderRig].getModulation());
}

void onRadio1Trx(const int senderRig) {
  g_lcd.setTx(0, g_radio1.rtx[senderRig].getTrx());
}
void onRadio2Trx(const int senderRig) {
  g_lcd.setTx(1, g_radio2.rtx[senderRig].getTrx());
}

void onRadio1Connected() { Serial.println("[R1] TCI conn event"); }
void onRadio2Connected() { Serial.println("[R2] TCI conn event"); }

// =============================================================================
// WiFi modes.
// =============================================================================

void startApMode() {
  g_apMode = true;
  WiFi.mode(WIFI_AP);
  char ssid[32];
  uint64_t mac = ESP.getEfuseMac();
  snprintf(ssid, sizeof(ssid), "BPF-Setup-%06X",
           (unsigned)(mac & 0xFFFFFF));
  WiFi.softAP(ssid);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[wifi] AP '%s' at %s\n", ssid, ip.toString().c_str());
  g_dns.start(53, "*", ip);
}

void startTciClients() {
  // Radio 1
  g_radio1.set_host(g_cfg.radio1_host);
  g_radio1.set_port(g_cfg.radio1_port);
  g_radio1.set_iaru_region(g_cfg.radio1_iaru);
  g_radio1.attach_conn_disc_event(onRadio1Connected);
  g_radio1.attach_vfo_event(onRadio1Vfo);
  g_radio1.attach_modulation_event(onRadio1Modulation);
  g_radio1.attach_trx_event(onRadio1Trx);
  g_radio1.connect();
  Serial.printf("[R1] TCI connecting to %s:%u (IARU %u)\n",
                g_cfg.radio1_host, g_cfg.radio1_port, g_cfg.radio1_iaru);

  // Radio 2
  g_radio2.set_host(g_cfg.radio2_host);
  g_radio2.set_port(g_cfg.radio2_port);
  g_radio2.set_iaru_region(g_cfg.radio2_iaru);
  g_radio2.attach_conn_disc_event(onRadio2Connected);
  g_radio2.attach_vfo_event(onRadio2Vfo);
  g_radio2.attach_modulation_event(onRadio2Modulation);
  g_radio2.attach_trx_event(onRadio2Trx);
  g_radio2.connect();
  Serial.printf("[R2] TCI connecting to %s:%u (IARU %u)\n",
                g_cfg.radio2_host, g_cfg.radio2_port, g_cfg.radio2_iaru);
}

void startStaMode() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(g_cfg.hostname);
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
    startTciClients();
  } else {
    Serial.println("[wifi] STA failed; falling back to AP portal");
    startApMode();
  }
}

}  // namespace bpf

// =============================================================================
// Setup / loop.
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println(F("BandPassFilterController :: ESP32 SO2R / TCI"));

  // Drive BCD banks to IDLE before WiFi starts.
  bpf::setupBank(bpf::kBank1);
  bpf::setupBank(bpf::kBank2);

  // LCD — comes up immediately so the user sees "starting..." even if
  // WiFi takes a while.
  bpf::g_lcd.begin();

  if (!bpf::load(bpf::g_cfg)) {
    Serial.println(F("[cfg] no valid config, loading defaults!"));
    bpf::defaults(bpf::g_cfg, bpf::kDefaultHostname);
  }

  if (bpf::g_cfg.wifi_ssid[0] != '\0') bpf::startStaMode();
  else                                  bpf::startApMode();

  bpf::g_web.begin(bpf::g_cfg, bpf::statusJson);
}

void loop() {
  bpf::g_web.tick();
  if (bpf::g_apMode) bpf::g_dns.processNextRequest();

  // Failsafe: if WiFi or either TCI link drops, force the affected bank to
  // bypass (INHIBIT asserted). Cheap check on a 500 ms cadence.
  static uint32_t lastCheck = 0;
  uint32_t now = millis();
  if (now - lastCheck >= 500) {
    lastCheck = now;
    if (bpf::g_apMode || WiFi.status() != WL_CONNECTED) {
      if (!bpf::g_lastInh1) bpf::applyBand(0, 0);
      if (!bpf::g_lastInh2) bpf::applyBand(1, 0);
    } else {
      if (!bpf::g_radio1.connected() && !bpf::g_lastInh1) bpf::applyBand(0, 0);
      if (!bpf::g_radio2.connected() && !bpf::g_lastInh2) bpf::applyBand(1, 0);
    }
  }

  // Serial console: "reset" wipes config; "status" prints status JSON.
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

  delay(10);  // yield to WiFi / TCI tasks
}
