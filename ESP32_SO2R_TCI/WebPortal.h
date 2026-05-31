// WebPortal.h — minimal config UI for the ESP32 SO2R/TCI controller.
// Routes: GET /, POST /save, GET /status, GET /config, POST /reboot,
//         POST /factory_reset.
//
// Mirrors the ESP8266 sketches' WebPortal.h but uses the ESP32 Arduino core
// (WebServer.h instead of ESP8266WebServer.h) and the 2-radio Config schema
// from Config.h.
#ifndef BPF_WEB_PORTAL_H
#define BPF_WEB_PORTAL_H

#include <Arduino.h>
#include <WebServer.h>
#include <functional>
#include "Config.h"

namespace bpf {

class WebPortal {
public:
  using StatusFn = std::function<String()>;
  // Manual bypass hook: callback(bpfIdx=0|1, on=true|false). Wired to
  // onTuneChange() in the sketch so the LCD/state pipeline reuses the
  // existing tune codepath. Used for radios whose TCI server doesn't
  // emit tune events (e.g. AetherSDR).
  using BypassFn = std::function<void(int, bool)>;

  void begin(Config& cfg, StatusFn statusJson, BypassFn bypassFn) {
    cfg_ = &cfg;
    statusJson_ = statusJson;
    bypassFn_   = bypassFn;
    server_.on("/",              HTTP_GET,  [this]() { handleRoot(); });
    server_.on("/save",          HTTP_POST, [this]() { handleSave(); });
    server_.on("/status",        HTTP_GET,  [this]() { handleStatus(); });
    server_.on("/config",        HTTP_GET,  [this]() { handleConfig(); });
    server_.on("/bypass",        HTTP_POST, [this]() { handleBypass(); });
    server_.on("/reboot",        HTTP_POST, [this]() { handleReboot(); });
    server_.on("/factory_reset", HTTP_POST, [this]() { handleFactoryReset(); });
    server_.onNotFound([this]() { server_.send(404, "text/plain", "not found"); });
    server_.begin();
  }

  void tick() { server_.handleClient(); }

private:
  WebServer server_{80};
  Config*   cfg_ = nullptr;
  StatusFn  statusJson_;
  BypassFn  bypassFn_;

  static String esc(const char* s) {
    String o;
    for (; *s; ++s) {
      switch (*s) {
        case '&': o += "&amp;";  break;
        case '<': o += "&lt;";   break;
        case '>': o += "&gt;";   break;
        case '"': o += "&quot;"; break;
        default:  o += *s;
      }
    }
    return o;
  }

  // Escape a C string for embedding inside a JSON string literal.
  static String jsonEsc(const char* s) {
    String o;
    for (; *s; ++s) {
      switch (*s) {
        case '"':  o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n";  break;
        case '\r': o += "\\r";  break;
        case '\t': o += "\\t";  break;
        default:
          if ((unsigned char)*s < 0x20) {
            char buf[7];
            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*s);
            o += buf;
          } else {
            o += *s;
          }
      }
    }
    return o;
  }

  void copyArg(const char* name, char* dst, size_t cap) {
    if (!server_.hasArg(name)) return;
    String v = server_.arg(name);
    strncpy(dst, v.c_str(), cap - 1);
    dst[cap - 1] = '\0';
  }

  void appendRadioSection(String& h, const char* label,
                          const char* hostField, const char* portField,
                          const char* iaruField,
                          const char* hostVal,  uint16_t portVal,
                          uint8_t iaruVal) {
    h += F("<fieldset><legend>"); h += label; h += F("</legend>");
    h += F("<label>Host</label><input name='");
    h += hostField; h += F("' maxlength='47' value='");
    h += esc(hostVal); h += F("'>");
    h += F("<div class='row'><div><label>Port</label><input name='");
    h += portField;
    h += F("' type='number' min='1' max='65535' value='");
    h += String(portVal); h += F("'></div><div><label>IARU region</label>"
                                  "<select name='");
    h += iaruField; h += F("'>");
    for (int r = 1; r <= 3; r++) {
      h += F("<option value='"); h += String(r); h += F("'");
      if (iaruVal == r) h += F(" selected");
      h += F(">R"); h += String(r); h += F("</option>");
    }
    h += F("</select></div></div>");
    h += F("</fieldset>");
  }

  void handleRoot() {
    String h;
    h.reserve(3200);
    h += F("<!doctype html><html><head><meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>BPF SO2R Controller</title><style>"
           "body{font-family:sans-serif;max-width:560px;margin:1em auto;padding:0 1em}"
           "label{display:block;margin:.7em 0 .2em;font-size:.9em;color:#555}"
           "input,select{width:100%;padding:.45em;box-sizing:border-box;font-size:1em}"
           "button{margin-top:1em;padding:.6em 1.1em;font-size:1em}"
           ".row{display:flex;gap:.5em}.row>*{flex:1}"
           ".warn{color:#b00;border-color:#b00}hr{margin:1.6em 0}"
           "fieldset{margin-top:1em;padding:.6em 1em;border:1px solid #ccc;border-radius:6px}"
           "legend{padding:0 .4em;color:#444;font-weight:bold}"
           "</style></head><body>");
    h += F("<h1>BPF SO2R / TCI Controller</h1>");
    h += F("<p>Host: <code>"); h += esc(cfg_->hostname); h += F("</code></p>");
    h += F("<form method='POST' action='/save'>");

    h += F("<fieldset><legend>WiFi</legend>");
    h += F("<label>SSID</label><input name='ssid' maxlength='31' value='");
    h += esc(cfg_->wifi_ssid); h += F("'>");
    h += F("<label>Password</label><input name='pass' type='password' maxlength='63' value='");
    h += esc(cfg_->wifi_pass); h += F("'>");
    h += F("<label>Hostname (mDNS)</label><input name='hostname' maxlength='31' value='");
    h += esc(cfg_->hostname); h += F("'>");
    h += F("</fieldset>");

    appendRadioSection(h, "Radio 1 (drives BPF 1)",
                       "r1_host", "r1_port", "r1_iaru",
                       cfg_->radio1_host, cfg_->radio1_port, cfg_->radio1_iaru);

    appendRadioSection(h, "Radio 2 (drives BPF 2)",
                       "r2_host", "r2_port", "r2_iaru",
                       cfg_->radio2_host, cfg_->radio2_port, cfg_->radio2_iaru);

    h += F("<p style='font-size:.85em;color:#555'>Tip: if a single radio "
           "(e.g. SunSDR2 PRO) feeds both BPFs, enter the same host/port "
           "in Radio 1 and Radio 2. The firmware opens one TCI client and "
           "routes RX-1 to BPF 1 and RX-2 to BPF 2.</p>");

    h += F("<button type='submit'>Save</button></form>");
    h += F("<hr><fieldset><legend>Manual bypass</legend>"
           "<p style='font-size:.85em;color:#555'>For radios whose TCI "
           "server doesn't emit tune events (e.g. AetherSDR). Click ON "
           "before pressing TUNE on the radio, OFF after the ATU "
           "finishes. While ON, the affected BPF is forced to bypass "
           "and the LCD state column shows TU.</p>"
           "<div class='row'>"
           "<form method='POST' action='/bypass'>"
           "<input type='hidden' name='bpf' value='1'>"
           "<input type='hidden' name='on'  value='1'>"
           "<button type='submit'>BPF 1 bypass ON</button></form>"
           "<form method='POST' action='/bypass'>"
           "<input type='hidden' name='bpf' value='1'>"
           "<input type='hidden' name='on'  value='0'>"
           "<button type='submit'>BPF 1 bypass OFF</button></form>"
           "</div>"
           "<div class='row'>"
           "<form method='POST' action='/bypass'>"
           "<input type='hidden' name='bpf' value='2'>"
           "<input type='hidden' name='on'  value='1'>"
           "<button type='submit'>BPF 2 bypass ON</button></form>"
           "<form method='POST' action='/bypass'>"
           "<input type='hidden' name='bpf' value='2'>"
           "<input type='hidden' name='on'  value='0'>"
           "<button type='submit'>BPF 2 bypass OFF</button></form>"
           "</div>"
           "</fieldset>");
    h += F("<hr><form method='POST' action='/reboot' style='display:inline'>"
           "<button>Reboot</button></form> "
           "<form method='POST' action='/factory_reset' style='display:inline' "
           "onsubmit=\"return confirm('Wipe all config and reboot?');\">"
           "<input type='hidden' name='confirm' value='YES'>"
           "<button class='warn'>Factory reset</button></form>"
           "<p><a href='/status'>/status JSON</a> &middot; "
           "<a href='/config'>/config JSON</a></p></body></html>");
    server_.send(200, "text/html", h);
  }

  void readRadioArgs(const char* hostField, const char* portField,
                     const char* iaruField,
                     char* hostDst, size_t hostCap,
                     uint16_t* portDst, uint8_t* iaruDst) {
    copyArg(hostField, hostDst, hostCap);
    if (server_.hasArg(portField)) {
      long p = server_.arg(portField).toInt();
      if (p > 0 && p < 65536) *portDst = (uint16_t)p;
    }
    if (server_.hasArg(iaruField)) {
      long r = server_.arg(iaruField).toInt();
      if (r >= 1 && r <= 3) *iaruDst = (uint8_t)r;
    }
  }

  void handleSave() {
    copyArg("ssid",     cfg_->wifi_ssid, sizeof(cfg_->wifi_ssid));
    copyArg("pass",     cfg_->wifi_pass, sizeof(cfg_->wifi_pass));
    copyArg("hostname", cfg_->hostname,  sizeof(cfg_->hostname));
    readRadioArgs("r1_host", "r1_port", "r1_iaru",
                  cfg_->radio1_host, sizeof(cfg_->radio1_host),
                  &cfg_->radio1_port, &cfg_->radio1_iaru);
    readRadioArgs("r2_host", "r2_port", "r2_iaru",
                  cfg_->radio2_host, sizeof(cfg_->radio2_host),
                  &cfg_->radio2_port, &cfg_->radio2_iaru);
    save(*cfg_);
    server_.send(200, "text/html",
                 F("<p>Saved.</p>"
                   "<p><a href='/'>Back</a> &middot; "
                   "<form style='display:inline' method='POST' action='/reboot'>"
                   "<button>Reboot now</button></form></p>"));
  }

  void handleStatus() {
    server_.send(200, "application/json", statusJson_ ? statusJson_() : String("{}"));
  }

  // GET /config — the stored configuration as JSON, serialized straight from
  // the same live cfg_ struct the HTML form renders, so the two can never
  // drift. The companion app reads this to populate its Settings draft.
  // The Wi-Fi password is intentionally omitted: it is never needed to
  // re-display config, and the app's "leave blank to keep current" model
  // means it should not round-trip the secret over the LAN.
  void handleConfig() {
    String j;
    j.reserve(360);
    j += F("{\"ssid\":\"");        j += jsonEsc(cfg_->wifi_ssid);
    j += F("\",\"hostname\":\"");  j += jsonEsc(cfg_->hostname);
    j += F("\",\"r1\":{\"host\":\""); j += jsonEsc(cfg_->radio1_host);
    j += F("\",\"port\":");        j += String(cfg_->radio1_port);
    j += F(",\"iaru\":");          j += String(cfg_->radio1_iaru);
    j += F("},\"r2\":{\"host\":\"");  j += jsonEsc(cfg_->radio2_host);
    j += F("\",\"port\":");        j += String(cfg_->radio2_port);
    j += F(",\"iaru\":");          j += String(cfg_->radio2_iaru);
    j += F("}}");
    server_.send(200, "application/json", j);
  }

  void handleBypass() {
    if (!bypassFn_) { server_.send(503, "text/plain", "no bypass handler"); return; }
    if (!server_.hasArg("bpf") || !server_.hasArg("on")) {
      server_.send(400, "text/plain", "need bpf=1|2 and on=0|1");
      return;
    }
    long bpf = server_.arg("bpf").toInt();
    String onStr = server_.arg("on");
    bool   on    = (onStr == "1" || onStr == "true" || onStr == "on");
    if (bpf < 1 || bpf > 2) {
      server_.send(400, "text/plain", "bpf must be 1 or 2");
      return;
    }
    bypassFn_((int)(bpf - 1), on);
    String body = "BPF "; body += String(bpf);
    body += on ? " bypass ON\n" : " bypass OFF\n";
    server_.send(200, "text/plain", body);
  }

  void handleReboot() {
    server_.send(200, "text/plain", "rebooting");
    delay(200);
    ESP.restart();
  }

  void handleFactoryReset() {
    if (server_.arg("confirm") != "YES") {
      server_.send(400, "text/plain", "confirm=YES required");
      return;
    }
    factoryReset();
    server_.send(200, "text/plain", "wiped; rebooting");
    delay(200);
    ESP.restart();
  }
};

}  // namespace bpf

#endif
