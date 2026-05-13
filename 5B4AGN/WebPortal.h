// WebPortal.h — minimal config UI served on the ESP-12.
// Routes: GET /, POST /save, GET /status, POST /reboot, POST /factory_reset.
#ifndef BPF_WEB_PORTAL_H
#define BPF_WEB_PORTAL_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <functional>
#include "Config.h"

namespace bpf {

class WebPortal {
public:
  using StatusFn = std::function<String()>;

  void begin(Config& cfg, StatusFn statusJson) {
    cfg_ = &cfg;
    statusJson_ = statusJson;
    server_.on("/",              HTTP_GET,  [this]() { handleRoot(); });
    server_.on("/save",          HTTP_POST, [this]() { handleSave(); });
    server_.on("/status",        HTTP_GET,  [this]() { handleStatus(); });
    server_.on("/reboot",        HTTP_POST, [this]() { handleReboot(); });
    server_.on("/factory_reset", HTTP_POST, [this]() { handleFactoryReset(); });
    server_.onNotFound([this]() { server_.send(404, "text/plain", "not found"); });
    server_.begin();
  }

  void tick() { server_.handleClient(); }

private:
  ESP8266WebServer server_{80};
  Config*  cfg_ = nullptr;
  StatusFn statusJson_;

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

  void copyArg(const char* name, char* dst, size_t cap) {
    if (!server_.hasArg(name)) return;
    String v = server_.arg(name);
    strncpy(dst, v.c_str(), cap - 1);
    dst[cap - 1] = '\0';
  }

  void handleRoot() {
    String h;
    h.reserve(2400);
    h += F("<!doctype html><html><head><meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>BPF Controller</title><style>"
           "body{font-family:sans-serif;max-width:520px;margin:1em auto;padding:0 1em}"
           "label{display:block;margin:.7em 0 .2em;font-size:.9em;color:#555}"
           "input,select{width:100%;padding:.45em;box-sizing:border-box;font-size:1em}"
           "button{margin-top:1em;padding:.6em 1.1em;font-size:1em}"
           ".row{display:flex;gap:.5em}.row>*{flex:1}"
           ".warn{color:#b00;border-color:#b00}hr{margin:1.6em 0}"
           "</style></head><body>");
    h += F("<h1>BandPass Filter Controller</h1>");
    h += F("<p>Host: <code>");          h += esc(cfg_->hostname);   h += F("</code></p>");
    h += F("<form method='POST' action='/save'>");

    h += F("<label>WiFi SSID</label><input name='ssid' maxlength='31' value='");
    h += esc(cfg_->wifi_ssid);   h += F("'>");
    h += F("<label>WiFi password</label><input name='pass' type='password' maxlength='63' value='");
    h += esc(cfg_->wifi_pass);   h += F("'>");
    h += F("<label>Hostname (mDNS)</label><input name='hostname' maxlength='31' value='");
    h += esc(cfg_->hostname);    h += F("'>");

    h += F("<div class='row'><div><label>Radio host</label><input name='radio_host' maxlength='47' value='");
    h += esc(cfg_->radio_host);  h += F("'></div><div><label>Radio port</label>"
                                       "<input name='radio_port' type='number' min='1' max='65535' value='");
    h += String(cfg_->radio_port); h += F("'></div></div>");

    h += F("<label>Protocol</label><select name='proto'>"
           "<option value='0'");
    if (cfg_->radio_protocol == 0) h += F(" selected");
    h += F(">Kenwood IF; over TCP</option>"
           "<option value='1' disabled>TCI WebSocket (planned)</option></select>");

    h += F("<label>Poll interval (ms)</label><input name='poll' type='number' min='100' max='5000' value='");
    h += String(cfg_->poll_interval_ms); h += F("'>");

    h += F("<button type='submit'>Save</button></form>");
    h += F("<hr><form method='POST' action='/reboot' style='display:inline'>"
           "<button>Reboot</button></form> "
           "<form method='POST' action='/factory_reset' style='display:inline' "
           "onsubmit=\"return confirm('Wipe all config and reboot?');\">"
           "<input type='hidden' name='confirm' value='YES'>"
           "<button class='warn'>Factory reset</button></form>"
           "<p><a href='/status'>/status JSON</a></p></body></html>");
    server_.send(200, "text/html", h);
  }

  void handleSave() {
    copyArg("ssid",       cfg_->wifi_ssid,  sizeof(cfg_->wifi_ssid));
    copyArg("pass",       cfg_->wifi_pass,  sizeof(cfg_->wifi_pass));
    copyArg("hostname",   cfg_->hostname,   sizeof(cfg_->hostname));
    copyArg("radio_host", cfg_->radio_host, sizeof(cfg_->radio_host));
    if (server_.hasArg("radio_port")) {
      long p = server_.arg("radio_port").toInt();
      if (p > 0 && p < 65536) cfg_->radio_port = (uint16_t)p;
    }
    if (server_.hasArg("proto")) {
      cfg_->radio_protocol = (uint8_t)server_.arg("proto").toInt();
    }
    if (server_.hasArg("poll")) {
      long pi = server_.arg("poll").toInt();
      if (pi >= 100 && pi <= 5000) cfg_->poll_interval_ms = (uint16_t)pi;
    }
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
