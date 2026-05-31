/*
 * BandPassFilterController — ESP32 SO2R / TCI variant
 *
 * One ESP32 carries two independent TCI-WebSocket clients to two SunSDR /
 * Expert Electronics radios and emits two independent Yaesu-style BCD
 * band-data buses — one per radio. Each BCD bank drives one band-pass
 * filter:
 *
 *   Radio 1  --TCI-->  BPF1 (e.g. 5B4AGN TXBPF)   via BCD bank 1
 *   Radio 2  --TCI-->  BPF2 (e.g. Hamation MBF-100) via BCD bank 2
 *
 * Each BPF is dedicated to its own radio (no SO2R cross-swap; that's a
 * separate firmware mode if needed later — see the MQTT reference at
 * .support/ESP32MQTTSwitchV2.ino).
 *
 * BCD encoding follows the Yaesu band-data convention:
 *   160m=1, 80m=2, 40m=3, 20m=5, 15m=7, 10m=9
 * WARC (30/17/12), 60 m, 6 m, and out-of-band assert INHIBIT instead
 * (BPF goes to bypass).
 *
 * Hardware: ESP32 dev board driving an 8-relay active-LOW carrier.
 * Library: TCI by IW7DMH (in .support/TCI-2/, must be installed into the
 *          Arduino libraries folder; see README in that directory).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <TCI.h>

#include "BcdBandPlan.h"

// =============================================================================
// User configuration — hardcoded for the first cut. Web portal can come later.
// =============================================================================

namespace bpf {

// WiFi
constexpr const char* kWifiSsid = "YOUR_WIFI_SSID";
constexpr const char* kWifiPass = "YOUR_WIFI_PASS";

// Radio 1 — drives BPF 1 (5B4AGN)
constexpr const char* kRadio1Host  = "192.168.1.20";
constexpr uint16_t    kRadio1Port  = 50001;
constexpr unsigned    kRadio1Iaru  = 1;

// Radio 2 — drives BPF 2 (Hamation BandPasser II)
constexpr const char* kRadio2Host  = "192.168.1.21";
constexpr uint16_t    kRadio2Port  = 50001;
constexpr unsigned    kRadio2Iaru  = 1;

// ESP32 GPIO assignments — matches the design agreed in
// docs/ARCHITECTURE.md §"ESP32 SO2R/TCI variant".
constexpr BcdBank kBank1 = {
  .pinA = 16, .pinB = 17, .pinC = 18, .pinD = 19, .pinInhibit = 26,
};
constexpr BcdBank kBank2 = {
  .pinA = 27, .pinB = 32, .pinC = 33, .pinD = 25, .pinInhibit = 14,
};

// =============================================================================
// Two TCI radio instances + per-radio frequency cache for the event callbacks.
// =============================================================================

TCI    g_radio1;
TCI    g_radio2;
volatile long g_lastFreq1 = 0;
volatile long g_lastFreq2 = 0;
volatile uint8_t g_lastBand1 = 0;
volatile uint8_t g_lastBand2 = 0;
volatile bool    g_lastInh1  = true;
volatile bool    g_lastInh2  = true;

// =============================================================================
// Drive helpers.
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

// TCI VFO-change event handlers. The library calls the registered handler
// when the radio reports a new VFO frequency. Each handler reads the
// frequency back via getVfo(vfoId) on its TCI instance.
void onRadio1Vfo(const int senderRig, const int senderVfo) {
  if (senderVfo != 0) return;  // only track VFO A
  long hz = g_radio1.rtx[senderRig].getVfo(0);
  if (hz <= 0) return;
  g_lastFreq1 = hz;
  applyBand(0, hz);
}

void onRadio2Vfo(const int senderRig, const int senderVfo) {
  if (senderVfo != 0) return;
  long hz = g_radio2.rtx[senderRig].getVfo(0);
  if (hz <= 0) return;
  g_lastFreq2 = hz;
  applyBand(1, hz);
}

void onRadio1Connected() {
  Serial.println("[R1] TCI connected");
}
void onRadio2Connected() {
  Serial.println("[R2] TCI connected");
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

  // BCD outputs LOW = active. Set both banks to IDLE before WiFi starts so
  // any spurious GPIO state during boot doesn't engage a band on the BPFs.
  bpf::setupBank(bpf::kBank1);
  bpf::setupBank(bpf::kBank2);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(bpf::kWifiSsid, bpf::kWifiPass);
  Serial.printf("[wifi] connecting to '%s'", bpf::kWifiSsid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  Serial.printf("[wifi] up, ip=%s\n", WiFi.localIP().toString().c_str());

  // TCI radio 1 → BPF 1
  bpf::g_radio1.set_host((char*)bpf::kRadio1Host);
  bpf::g_radio1.set_port(bpf::kRadio1Port);
  bpf::g_radio1.set_iaru_region(bpf::kRadio1Iaru);
  bpf::g_radio1.attach_conn_disc_event(bpf::onRadio1Connected);
  bpf::g_radio1.attach_vfo_event(bpf::onRadio1Vfo);
  bpf::g_radio1.connect();

  // TCI radio 2 → BPF 2
  bpf::g_radio2.set_host((char*)bpf::kRadio2Host);
  bpf::g_radio2.set_port(bpf::kRadio2Port);
  bpf::g_radio2.set_iaru_region(bpf::kRadio2Iaru);
  bpf::g_radio2.attach_conn_disc_event(bpf::onRadio2Connected);
  bpf::g_radio2.attach_vfo_event(bpf::onRadio2Vfo);
  bpf::g_radio2.connect();
}

void loop() {
  // Both TCI instances run their own FreeRTOS tasks (see TCI library);
  // nothing for loop() to do. Yield so the WiFi / TCI tasks run.
  delay(100);

  // Failsafe: if either WiFi or a TCI link drops for > 5 s, force the
  // affected bank to bypass.
  static uint32_t lastCheck = 0;
  uint32_t now = millis();
  if (now - lastCheck < 500) return;
  lastCheck = now;

  if (WiFi.status() != WL_CONNECTED) {
    if (!bpf::g_lastInh1) bpf::applyBand(0, 0);
    if (!bpf::g_lastInh2) bpf::applyBand(1, 0);
    return;
  }
  if (!bpf::g_radio1.connected() && !bpf::g_lastInh1) bpf::applyBand(0, 0);
  if (!bpf::g_radio2.connected() && !bpf::g_lastInh2) bpf::applyBand(1, 0);
}
