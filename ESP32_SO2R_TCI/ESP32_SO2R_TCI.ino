/*
 * BandPassFilterController — ESP32 SO2R / TCI variant
 *
 * One ESP32 carries up to two TCI-WebSocket clients to one or two SunSDR /
 * Expert Electronics radios and emits two independent Yaesu-style BCD
 * band-data buses — one per BPF. Each BCD bank drives one band-pass
 * filter:
 *
 *   Radio 1  --TCI-->  BPF1 (e.g. 5B4AGN TXBPF)    via BCD bank 1
 *   Radio 2  --TCI-->  BPF2 (e.g. another 5B4AGN)  via BCD bank 2
 *
 * Two server modes, picked automatically from config:
 *
 *  - DUAL: Radio 1 and Radio 2 have different host:port pairs. Two TCI
 *    clients are opened; each filters on rig=0 / vfo=0 (main RX VFO A).
 *  - SHARED: Radio 1 and Radio 2 point at the SAME host:port. Only one
 *    TCI client is opened; rig=0 events drive BPF 1, rig=1 events drive
 *    BPF 2. This is the right wiring when a single SunSDR2 PRO (or any
 *    dual-receiver radio) feeds two filters.
 *
 * Each BPF is dedicated to its own receiver (no SO2R cross-swap; that's
 * a separate firmware mode if needed later).
 *
 * BCD encoding: standard Yaesu Table 5 — 160m=1, 80m=2, 40m=3, 30m=4,
 * 20m=5, 17m=6, 15m=7, 12m=8, 10m=9, 6m=10. 60 m / out-of-band /
 * disconnect drop to bypass (all four BCD lines released HIGH on the
 * affected bank). Note: the 5B4AGN / Hamation BPFs only have sections
 * for the six contest bands; on 30/17/12/6 m they'll bypass.
 *
 * Configuration is persisted in flash (EEPROM emulation) and editable
 * from a tiny on-device web portal — first boot raises a SoftAP named
 * `BPF-Setup-XXXXXX` reachable at http://192.168.4.1/.
 *
 * Hardware: ESP32 dev board driving an 8-relay active-LOW carrier.
 * Library: TCI by IW7DMH (bundled in this sketch folder; see README).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>

#include "TCI.h"
#include "Config.h"
#include "BcdBandPlan.h"
#include "WebPortal.h"
#include "LcdDisplay.h"

namespace bpf {

constexpr const char* kDefaultHostname = "SO2R-BPF";
constexpr const char* kFilterLabel     = "ESP32 SO2R / TCI";
// Bump on every meaningful release. Also surfaced in the portal
// banner, in /status, and in /config so an OTA target is unambiguous.
constexpr const char* kFirmwareVersion = "0.5.0";
constexpr const char* kFirmwareBuild   = __DATE__ " " __TIME__;

// Forward + reflected power sensor inputs (ADC1; all four are
// input-only GPIOs so they don't steal an output pin). Wire the
// directional coupler's detector outputs here through a 0..3.3 V
// scaling network. analogReadMilliVolts() returns calibrated mV.
constexpr uint8_t kSensorBpf1Fwd = 34;   // ADC1_CH6
constexpr uint8_t kSensorBpf1Rev = 35;   // ADC1_CH7
constexpr uint8_t kSensorBpf2Fwd = 36;   // ADC1_CH0 (SVP)
constexpr uint8_t kSensorBpf2Rev = 39;   // ADC1_CH3 (SVN)

// Band-change ring buffer. Pushed by applyBand(); read out via
// /status JSON; cleared via POST /history?clear=YES.
struct BandEvent {
  uint32_t uptime_s;   // seconds since boot at the time of the change
  uint8_t  bpf;        // 1 or 2
  long     freq_hz;    // last known freq when the bank was rewritten
  uint8_t  code;       // BCD code emitted
  bool     inhibit;    // was this a bypass push?
  bool     tune;       // was tune active at the time?
};
constexpr size_t kHistoryCap = 50;

// Pin map — active-LOW; HIGH = idle (relay off). See ESP32_SO2R_TCI/README.md.
//
// 8 relays = 8 pins total; 4 per radio. The 5B4AGN side keeps the BCD
// pins (16/17/18/19) inherited from the earlier MQTT-driven ESP32
// build because that wiring is already in place. BPF 2 occupies the
// remaining 4 relays.
constexpr BcdBank kBank1 = { .pinA = 16, .pinB = 17, .pinC = 18, .pinD = 19 };
constexpr BcdBank kBank2 = { .pinA = 33, .pinB = 32, .pinC = 27, .pinD = 26 };

// Globals.
Config        g_cfg;
TCI           g_radio1;
TCI           g_radio2;            // unused in SHARED mode
WebPortal     g_web;
LcdDisplay    g_lcd;
DNSServer     g_dns;
bool          g_apMode    = false;
bool          g_sharedTci = false; // both BPFs served by g_radio1
volatile long g_lastFreq1 = 0;
volatile long g_lastFreq2 = 0;
volatile uint8_t g_lastBand1 = 0;
volatile uint8_t g_lastBand2 = 0;
volatile bool    g_lastInh1  = true;
volatile bool    g_lastInh2  = true;
volatile bool    g_lastLink1 = false;
volatile bool    g_lastLink2 = false;
// Force-bypass flags: true while the radio's TUNE button is engaged.
// Contest BPFs are narrow-band; a wide ATU tuning sweep can push the
// filter section into VSWR limits, so we drop to bypass for the
// duration of the tune cycle.
volatile bool    g_tune1     = false;
volatile bool    g_tune2     = false;

// Band-change ring buffer.
BandEvent g_history[kHistoryCap];
size_t    g_historyHead  = 0;     // next slot to write
size_t    g_historyCount = 0;     // total fill, capped at kHistoryCap

// ADC sensor smoothing: exponential moving average in mV.
// alpha = 1/8 (kept as integer math via right-shift in update path).
volatile int g_sensorMv[4] = {0, 0, 0, 0};   // {bpf1_fwd, bpf1_rev, bpf2_fwd, bpf2_rev}

// WebSocket server for the /live page. Port 81 to keep the HTTP
// portal on 80. Broadcasts a JSON snapshot every 250 ms when any
// client is connected (idle when none — costs almost nothing).
WebSocketsServer g_wsServer(81);

// =============================================================================
// Status JSON for /status.
// =============================================================================

String statusJson() {
  bool r1up = g_radio1.connected();
  bool r2up = g_sharedTci ? r1up : g_radio2.connected();
  String j;
  // Grow a bit; the history block can be ~1.5 KB at full 50 entries.
  j.reserve(2200);
  j += "{";
  j += "\"filter\":\"";  j += kFilterLabel;                                   j += "\",";
  j += "\"version\":\""; j += kFirmwareVersion;                               j += "\",";
  j += "\"build\":\"";   j += kFirmwareBuild;                                 j += "\",";
  j += "\"mode\":\"";    j += (g_sharedTci ? "shared" : "dual");              j += "\",";
  j += "\"ap_mode\":";   j += (g_apMode ? "true" : "false");                  j += ",";
  j += "\"wifi\":\"";    j += (WiFi.status() == WL_CONNECTED ? "up" : "down");j += "\",";
  j += "\"ip\":\"";      j += (g_apMode ? WiFi.softAPIP().toString()
                                        : WiFi.localIP().toString());         j += "\",";
  j += "\"rssi\":";      j += WiFi.RSSI();                                    j += ",";
  j += "\"r1\":{\"connected\":"; j += (r1up ? "true" : "false");
  j += ",\"freq_hz\":";          j += g_lastFreq1;
  j += ",\"band\":\"";           j += bandName(g_lastBand1, g_lastInh1);      j += "\"";
  j += ",\"tuning\":";           j += (g_tune1 ? "true" : "false");           j += "},";
  j += "\"r2\":{\"connected\":"; j += (r2up ? "true" : "false");
  j += ",\"freq_hz\":";          j += g_lastFreq2;
  j += ",\"band\":\"";           j += bandName(g_lastBand2, g_lastInh2);      j += "\"";
  j += ",\"tuning\":";           j += (g_tune2 ? "true" : "false");           j += "},";
  j += "\"sensors\":{";
  j += "\"bpf1_fwd_mv\":"; j += g_sensorMv[0]; j += ",";
  j += "\"bpf1_rev_mv\":"; j += g_sensorMv[1]; j += ",";
  j += "\"bpf2_fwd_mv\":"; j += g_sensorMv[2]; j += ",";
  j += "\"bpf2_rev_mv\":"; j += g_sensorMv[3]; j += "},";

  // Band-change history — oldest first so a UI can append in order.
  j += "\"history\":[";
  size_t start = (g_historyCount == kHistoryCap) ? g_historyHead : 0;
  for (size_t i = 0; i < g_historyCount; i++) {
    const BandEvent& e = g_history[(start + i) % kHistoryCap];
    if (i > 0) j += ",";
    j += "{\"t\":";        j += e.uptime_s;
    j += ",\"bpf\":";      j += e.bpf;
    j += ",\"hz\":";       j += e.freq_hz;
    j += ",\"code\":";     j += e.code;
    j += ",\"inh\":";      j += (e.inhibit ? "true" : "false");
    j += ",\"tune\":";     j += (e.tune ? "true" : "false");
    j += "}";
  }
  j += "],";
  j += "\"history_count\":"; j += g_historyCount; j += ",";
  j += "\"uptime_s\":";      j += (millis() / 1000);
  j += "}";
  return j;
}

// =============================================================================
// BCD drive helpers — only push to the bank when the decoded band changes.
// =============================================================================

// Append to the band-change ring buffer. Overwrites the oldest entry
// when full; callers don't need to check capacity.
void pushHistory(int radioIndex, long hz, const BcdResult& r, bool tuning) {
  BandEvent& e = g_history[g_historyHead];
  e.uptime_s = millis() / 1000;
  e.bpf      = (uint8_t)(radioIndex + 1);
  e.freq_hz  = hz;
  e.code     = r.code;
  e.inhibit  = r.inhibit;
  e.tune     = tuning;
  g_historyHead = (g_historyHead + 1) % kHistoryCap;
  if (g_historyCount < kHistoryCap) g_historyCount++;
}

void applyBand(int radioIndex, long hz) {
  // Decoded band, then bypass override. We never emit 0000 for bypass:
  // the Hamation BandPasser II decodes 0000 as 160 m (engaging the
  // 160 m section instead of bypass), which is dangerous if the radio
  // is tuning at high power on a different band. Instead, when bypass
  // is needed (TUNE pressed, OOB, link down, 60 m) we emit a nearest-
  // WARC code — Hamation has no WARC sections so it falls through to
  // its built-in bypass relay; 5B4AGN also bypasses on unrecognised
  // codes.
  bool tuning = (radioIndex == 0) ? g_tune1 : g_tune2;
  BcdResult r = bcdFor(hz);
  if (tuning || r.inhibit) r = nearestWarcBypass(hz);
  if (radioIndex == 0) {
    if (r.code == g_lastBand1 && r.inhibit == g_lastInh1) return;
    driveBank(kBank1, r);
    g_lastBand1 = r.code;
    g_lastInh1  = r.inhibit;
    Serial.printf("[R1] %s @ %ld Hz (bcd=%u inh=%d tune=%d)\r\n",
                  bandName(r.code, r.inhibit), hz, r.code, r.inhibit, tuning);
  } else {
    if (r.code == g_lastBand2 && r.inhibit == g_lastInh2) return;
    driveBank(kBank2, r);
    g_lastBand2 = r.code;
    g_lastInh2  = r.inhibit;
    Serial.printf("[R2] %s @ %ld Hz (bcd=%u inh=%d tune=%d)\r\n",
                  bandName(r.code, r.inhibit), hz, r.code, r.inhibit, tuning);
  }
  pushHistory(radioIndex, hz, r, tuning);
}

// Tune state change: latch the new flag, re-evaluate the BCD bank
// using the last known frequency, and update the LCD's state column.
void onTuneChange(int radioIndex, bool tuning) {
  Serial.printf("[BPF%d] TUNE change -> %s\r\n",
                radioIndex + 1, tuning ? "ON  (forcing bypass)" : "OFF");
  if (radioIndex == 0) g_tune1 = tuning;
  else                 g_tune2 = tuning;
  long hz = (radioIndex == 0) ? g_lastFreq1 : g_lastFreq2;
  applyBand(radioIndex, hz);
  g_lcd.setTune(radioIndex, tuning);
}

// =============================================================================
// TCI event handlers.
//
// In DUAL mode each filter has its own TCI client and only listens to
// rig=0 / vfo=0 (the main RX, VFO A) of its own client.
// In SHARED mode g_radio1 is the only client, and we demux:
//   senderRig == 0 -> BPF 1
//   senderRig == 1 -> BPF 2
// VFO B (vfoId != 0) is ignored in both modes; the main dial drives the BPF.
// =============================================================================

inline int sharedBpfIndex(int senderRig) {
  // 0 -> BPF 1 (LCD row 0), 1 -> BPF 2 (LCD row 1). Anything else: drop.
  return (senderRig == 0 || senderRig == 1) ? senderRig : -1;
}

void onSharedVfo(const int senderRig, const int senderVfo) {
  if (senderVfo != 0) return;
  int idx = sharedBpfIndex(senderRig);
  if (idx < 0) return;
  long hz = g_radio1.rtx[senderRig].getVfo(senderVfo);
  if (hz <= 0) return;
  if (idx == 0) g_lastFreq1 = hz;
  else          g_lastFreq2 = hz;
  applyBand(idx, hz);
  g_lcd.setFreq(idx, hz);
}

void onSharedModulation(const int senderRig) {
  int idx = sharedBpfIndex(senderRig);
  if (idx < 0) return;
  g_lcd.setMode(idx, g_radio1.rtx[senderRig].getModulation());
}

void onSharedTrx(const int senderRig) {
  int idx = sharedBpfIndex(senderRig);
  if (idx < 0) return;
  g_lcd.setTx(idx, g_radio1.rtx[senderRig].getTrx());
}

void onSharedTune(const int senderRig) {
  bool t = g_radio1.rtx[senderRig].getTune();
  Serial.printf("[tci-shared] tune evt rig=%d state=%d\r\n", senderRig, t);
  int idx = sharedBpfIndex(senderRig);
  if (idx < 0) return;
  onTuneChange(idx, t);
}

void onRadio1Vfo(const int senderRig, const int senderVfo) {
  if (senderRig != 0 || senderVfo != 0) return;
  long hz = g_radio1.rtx[senderRig].getVfo(senderVfo);
  if (hz <= 0) return;
  g_lastFreq1 = hz;
  applyBand(0, hz);
  g_lcd.setFreq(0, hz);
}

void onRadio2Vfo(const int senderRig, const int senderVfo) {
  if (senderRig != 0 || senderVfo != 0) return;
  long hz = g_radio2.rtx[senderRig].getVfo(senderVfo);
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

void onRadio1Tune(const int senderRig) {
  bool t = g_radio1.rtx[senderRig].getTune();
  Serial.printf("[R1] tune evt rig=%d state=%d\r\n", senderRig, t);
  if (senderRig != 0) return;
  onTuneChange(0, t);
}
void onRadio2Tune(const int senderRig) {
  bool t = g_radio2.rtx[senderRig].getTune();
  Serial.printf("[R2] tune evt rig=%d state=%d\r\n", senderRig, t);
  if (senderRig != 0) return;
  onTuneChange(1, t);
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
  Serial.printf("[wifi] AP '%s' at %s\r\n", ssid, ip.toString().c_str());
  g_dns.start(53, "*", ip);
}

// Returns true if both radios resolve to the exact same TCI server.
bool isSharedTciConfig(const Config& c) {
  if (c.radio1_host[0] == '\0' || c.radio2_host[0] == '\0') return false;
  if (c.radio1_port != c.radio2_port) return false;
  return strcasecmp(c.radio1_host, c.radio2_host) == 0;
}

void startTciClients() {
  g_sharedTci = isSharedTciConfig(g_cfg);

  if (g_sharedTci) {
    Serial.printf("[tci] shared server mode -> %s:%u (BPF1=rig0, BPF2=rig1)\r\n",
                  g_cfg.radio1_host, g_cfg.radio1_port);
    g_radio1.set_host(g_cfg.radio1_host);
    g_radio1.set_port(g_cfg.radio1_port);
    g_radio1.set_iaru_region(g_cfg.radio1_iaru);
    g_radio1.attach_conn_disc_event(onRadio1Connected);
    g_radio1.attach_vfo_event(onSharedVfo);
    g_radio1.attach_modulation_event(onSharedModulation);
    g_radio1.attach_trx_event(onSharedTrx);
    g_radio1.attach_tune_event(onSharedTune);
    g_radio1.connect();
    return;
  }

  // DUAL mode: two TCI clients, one per radio/filter.
  Serial.printf("[tci] dual server mode\r\n");
  g_radio1.set_host(g_cfg.radio1_host);
  g_radio1.set_port(g_cfg.radio1_port);
  g_radio1.set_iaru_region(g_cfg.radio1_iaru);
  g_radio1.attach_conn_disc_event(onRadio1Connected);
  g_radio1.attach_vfo_event(onRadio1Vfo);
  g_radio1.attach_modulation_event(onRadio1Modulation);
  g_radio1.attach_trx_event(onRadio1Trx);
  g_radio1.attach_tune_event(onRadio1Tune);
  g_radio1.connect();
  Serial.printf("[R1] TCI connecting to %s:%u (IARU %u)\r\n",
                g_cfg.radio1_host, g_cfg.radio1_port, g_cfg.radio1_iaru);

  g_radio2.set_host(g_cfg.radio2_host);
  g_radio2.set_port(g_cfg.radio2_port);
  g_radio2.set_iaru_region(g_cfg.radio2_iaru);
  g_radio2.attach_conn_disc_event(onRadio2Connected);
  g_radio2.attach_vfo_event(onRadio2Vfo);
  g_radio2.attach_modulation_event(onRadio2Modulation);
  g_radio2.attach_trx_event(onRadio2Trx);
  g_radio2.attach_tune_event(onRadio2Tune);
  g_radio2.connect();
  Serial.printf("[R2] TCI connecting to %s:%u (IARU %u)\r\n",
                g_cfg.radio2_host, g_cfg.radio2_port, g_cfg.radio2_iaru);
}

// Re-apply the configured hostname when the STA interface starts.
// Calling WiFi.setHostname() right after WiFi.mode(WIFI_STA) races with
// the netif being created; many routers end up logging the default
// `esp32-<mac>` because DHCP DISCOVER goes out before the new hostname
// reaches lwIP. Doing it from STA_START is documented as the reliable
// pattern on espressif/arduino-esp32.
void onWifiStaStart(arduino_event_id_t event) {
  if (event == ARDUINO_EVENT_WIFI_STA_START) {
    WiFi.setHostname(g_cfg.hostname);
  }
}

void setupOta();           // forward-declares; full bodies live below
void setupSensors();
void readSensorsTick();
void wsBroadcastIfDue();
void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len);
String liveJsonSnapshot();

void startStaMode() {
  WiFi.onEvent(onWifiStaStart, ARDUINO_EVENT_WIFI_STA_START);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(g_cfg.hostname);   // first attempt; STA_START handler retries
  WiFi.begin(g_cfg.wifi_ssid, g_cfg.wifi_pass);
  Serial.printf("[wifi] STA connecting to '%s'", g_cfg.wifi_ssid);
  uint32_t deadline = millis() + 20000;
  while (WiFi.status() != WL_CONNECTED && (int32_t)(millis() - deadline) < 0) {
    delay(200);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] STA up, ip=%s\r\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin(g_cfg.hostname)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[mdns] http://%s.local/\r\n", g_cfg.hostname);
    }
    setupOta();
    g_wsServer.begin();
    g_wsServer.onEvent(onWsEvent);
    Serial.printf("[ws] live status server on port 81\r\n");
    startTciClients();
  } else {
    Serial.println("[wifi] STA failed; falling back to AP portal");
    startApMode();
  }
}

// =============================================================================
// Over-the-air firmware update (ArduinoOTA).
//
// Once Wi-Fi is up, the device exposes the standard espota service on
// UDP 3232 with mDNS hostname <hostname>.local. arduino-cli picks it
// up exactly like a USB serial port:
//
//   arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs \
//     --upload --port SO2R-BPF.local ESP32_SO2R_TCI
//
// Default partition is min_spiffs so both 1.9 MB OTA slots fit the
// firmware with comfortable headroom (the original default.csv 1.25 MB
// slots were already at 94% before adding OTA).
//
// kOtaPassword is empty by default — the OTA service binds to the LAN
// only, and for a trusted hamshack network that's a reasonable model.
// WARNING: with an empty value, anyone who can reach the board on UDP
// 3232 can replace the firmware with arbitrary code. Set a non-empty
// value if you're on a guest VLAN / shared LAN / anything you don't
// fully control.
//
// `--upload-field password=<value>` is mandatory on every OTA upload
// regardless (arduino-cli prompts interactively otherwise and aborts
// in non-interactive shells); pass an empty value to match the empty
// default.
//
// Rotating the password does NOT need a USB cable: build with the new
// kOtaPassword value, OTA-upload using the OLD password (the running
// firmware still has the old one), and the new password takes effect
// after the device reboots. USB is only required if you've lost the
// current password and can't authenticate to OTA anymore.
// =============================================================================

constexpr const char* kOtaPassword = "";

void setupOta() {
  ArduinoOTA.setHostname(g_cfg.hostname);
  if (kOtaPassword[0] != '\0') {
    ArduinoOTA.setPassword(kOtaPassword);
  }
  ArduinoOTA.onStart([]() {
    const char* type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "fs";
    Serial.printf("[ota] update starting (%s) — forcing both BPFs to bypass\r\n", type);
    // Belt-and-braces: drop both banks to bypass for the duration of
    // the flash so an in-flight ATU tune sweep can't hot-switch
    // a filter section while the firmware is being rewritten.
    onTuneChange(0, true);
    onTuneChange(1, true);
    // Free the WiFi stack for the OTA upload. The TCI WebSocket
    // reader tasks compete with espota for bandwidth and reliably
    // cause a "Broken pipe" partway through the transfer if left
    // running. The device is rebooting at the end of the OTA anyway,
    // so a clean disconnect now is the right move.
    g_radio1.disconnect();
    if (!g_sharedTci) g_radio2.disconnect();
  });
  ArduinoOTA.onProgress([](unsigned int prog, unsigned int total) {
    static int lastPct = -1;
    int pct = (int)((prog * 100UL) / total);
    if (pct != lastPct && (pct % 10) == 0) {  // log every 10%
      Serial.printf("[ota] %d%%\r\n", pct);
      lastPct = pct;
    }
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("[ota] update complete, rebooting");
  });
  ArduinoOTA.onError([](ota_error_t err) {
    const char* msg = "?";
    switch (err) {
      case OTA_AUTH_ERROR:    msg = "auth failed";    break;
      case OTA_BEGIN_ERROR:   msg = "begin failed";   break;
      case OTA_CONNECT_ERROR: msg = "connect failed"; break;
      case OTA_RECEIVE_ERROR: msg = "receive failed"; break;
      case OTA_END_ERROR:     msg = "end failed";     break;
    }
    Serial.printf("[ota] error %u: %s\r\n", err, msg);
  });
  ArduinoOTA.begin();
  Serial.printf("[ota] ready at %s.local:3232 (password=%s)\r\n",
                g_cfg.hostname, kOtaPassword[0] ? "yes" : "none");
}

// =============================================================================
// ADC power sensors. analogReadMilliVolts() returns calibrated mV
// (uses the per-chip eFuse calibration when present, otherwise a
// reasonable default). Each call is ~30 µs; we sample once per tick
// and feed an EMA so /status reports a smooth value rather than a
// jittery instantaneous one.
// =============================================================================

void setupSensors() {
  analogSetAttenuation(ADC_11db);  // 0..3.3 V input range
  pinMode(kSensorBpf1Fwd, INPUT);
  pinMode(kSensorBpf1Rev, INPUT);
  pinMode(kSensorBpf2Fwd, INPUT);
  pinMode(kSensorBpf2Rev, INPUT);
}

// EMA with alpha = 1/8: new = old*7/8 + sample*1/8.
inline int ema(int prev, int sample) {
  return ((prev * 7) + sample) >> 3;
}

void readSensorsTick() {
  g_sensorMv[0] = ema(g_sensorMv[0], (int)analogReadMilliVolts(kSensorBpf1Fwd));
  g_sensorMv[1] = ema(g_sensorMv[1], (int)analogReadMilliVolts(kSensorBpf1Rev));
  g_sensorMv[2] = ema(g_sensorMv[2], (int)analogReadMilliVolts(kSensorBpf2Fwd));
  g_sensorMv[3] = ema(g_sensorMv[3], (int)analogReadMilliVolts(kSensorBpf2Rev));
}

// =============================================================================
// WebSocket live status. One snapshot every 250 ms when at least one
// client is connected — idle otherwise. Snapshot is compact JSON so
// even a flaky phone hotspot can keep up.
// =============================================================================

void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  if (type == WStype_CONNECTED) {
    Serial.printf("[ws] client %u connected\r\n", num);
    // Push an immediate snapshot so the page paints right away.
    String s = liveJsonSnapshot();
    g_wsServer.sendTXT(num, s);
  } else if (type == WStype_DISCONNECTED) {
    Serial.printf("[ws] client %u disconnected\r\n", num);
  }
}

String liveJsonSnapshot() {
  String j;
  j.reserve(420);
  j += "{\"version\":\""; j += kFirmwareVersion; j += "\",";
  j += "\"r1\":{";
  j += "\"link\":";  j += (g_lastLink1 ? "true" : "false");
  j += ",\"freq\":"; j += g_lastFreq1;
  j += ",\"band\":\""; j += bandName(g_lastBand1, g_lastInh1); j += "\"";
  j += ",\"tune\":";  j += (g_tune1 ? "true" : "false");
  j += "},\"r2\":{";
  j += "\"link\":";  j += (g_lastLink2 ? "true" : "false");
  j += ",\"freq\":"; j += g_lastFreq2;
  j += ",\"band\":\""; j += bandName(g_lastBand2, g_lastInh2); j += "\"";
  j += ",\"tune\":";  j += (g_tune2 ? "true" : "false");
  j += "},\"mv\":[";
  j += g_sensorMv[0]; j += ",";
  j += g_sensorMv[1]; j += ",";
  j += g_sensorMv[2]; j += ",";
  j += g_sensorMv[3]; j += "]}";
  return j;
}

void wsBroadcastIfDue() {
  if (g_wsServer.connectedClients() == 0) return;
  String s = liveJsonSnapshot();
  g_wsServer.broadcastTXT(s);
}

// Wipe the band-change ring buffer. Returns the previous count.
size_t clearHistory() {
  size_t prev = g_historyCount;
  g_historyHead  = 0;
  g_historyCount = 0;
  return prev;
}

}  // namespace bpf

// =============================================================================
// Setup / loop.
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.printf("BandPassFilterController :: ESP32 SO2R / TCI  v%s (%s)\r\n",
                bpf::kFirmwareVersion, bpf::kFirmwareBuild);

  // Drive BCD banks to IDLE before WiFi starts.
  bpf::setupBank(bpf::kBank1);
  bpf::setupBank(bpf::kBank2);

  // ADC sensor inputs (forward + reflected power, BPF 1 and BPF 2).
  bpf::setupSensors();

  // LCD — comes up immediately so the user sees "starting..." even if
  // WiFi takes a while.
  bpf::g_lcd.begin();

  if (!bpf::load(bpf::g_cfg)) {
    Serial.println(F("[cfg] no valid config, loading defaults!"));
    bpf::defaults(bpf::g_cfg, bpf::kDefaultHostname);
  }

  if (bpf::g_cfg.wifi_ssid[0] != '\0') bpf::startStaMode();
  else                                  bpf::startApMode();

  bpf::g_web.begin(bpf::g_cfg, bpf::statusJson,
                   [](int bpfIdx, bool on) { bpf::onTuneChange(bpfIdx, on); },
                   []() -> size_t { return bpf::clearHistory(); },
                   bpf::kFirmwareVersion, bpf::kFirmwareBuild);
}

void loop() {
  bpf::g_web.tick();
  // OTA + WS server only matter in STA mode (no point in AP / portal
  // mode — no LAN to upload from / no client likely on a captive AP).
  // ArduinoOTA::handle() and WebSocketsServer::loop() are cheap polls.
  if (!bpf::g_apMode) {
    ArduinoOTA.handle();
    bpf::g_wsServer.loop();
  }
  if (bpf::g_apMode)  bpf::g_dns.processNextRequest();

  // Failsafe + link status: every 250 ms check WiFi + TCI health and force
  // bypass on the affected bank if the link drops. Also push the link
  // status to the LCD so col 0 of each row reflects current health.
  static uint32_t lastCheck = 0;
  uint32_t now = millis();
  if (now - lastCheck >= 250) {
    lastCheck = now;
    bool wifiUp = !bpf::g_apMode && WiFi.status() == WL_CONNECTED;
    bool r1Up   = wifiUp && bpf::g_radio1.connected();
    bool r2Up   = bpf::g_sharedTci ? r1Up
                                   : (wifiUp && bpf::g_radio2.connected());

    if (!r1Up && !bpf::g_lastInh1) bpf::applyBand(0, 0);
    if (!r2Up && !bpf::g_lastInh2) bpf::applyBand(1, 0);

    if (r1Up != bpf::g_lastLink1) {
      bpf::g_lastLink1 = r1Up;
      bpf::g_lcd.setLink(0, r1Up);
    }
    if (r2Up != bpf::g_lastLink2) {
      bpf::g_lastLink2 = r2Up;
      bpf::g_lcd.setLink(1, r2Up);
    }

    // Sensor sampling + WS snapshot ride the same 250 ms tick.
    bpf::readSensorsTick();
    bpf::wsBroadcastIfDue();
  }

  // Serial console:
  //   reset                wipes EEPROM and reboots
  //   status               prints status JSON
  //   bypass{1,2} {on,off} latches manual bypass on the named BPF
  //                        (same effect as TCI tune; for AetherSDR etc.)
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
    } else if (line == "bypass1 on")  { bpf::onTuneChange(0, true);  }
      else if (line == "bypass1 off") { bpf::onTuneChange(0, false); }
      else if (line == "bypass2 on")  { bpf::onTuneChange(1, true);  }
      else if (line == "bypass2 off") { bpf::onTuneChange(1, false); }
  }

  delay(10);  // yield to WiFi / TCI tasks
}
