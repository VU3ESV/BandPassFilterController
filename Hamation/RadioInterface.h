// RadioInterface.h — TCP client that polls a Kenwood IF; CAT server.
// Identical to the 5B4AGN sketch; duplicated so each Arduino sketch folder is
// self-contained.
#ifndef BPF_RADIO_INTERFACE_H
#define BPF_RADIO_INTERFACE_H

#include <Arduino.h>
#include <ESP8266WiFi.h>

namespace bpf {

class RadioInterface {
public:
  void configure(const char* host, uint16_t port, uint16_t pollIntervalMs) {
    host_ = host;
    port_ = port;
    pollIntervalMs_ = pollIntervalMs;
    backoffMs_ = 1000;
    nextConnectAt_ = 0;
    lastPollAt_ = 0;
    lastFreqAt_ = 0;
    frequencyHz_ = 0;
  }

  bool connected() { return client_.connected(); }
  long frequencyHz() const { return frequencyHz_; }

  bool tick() {
    uint32_t now = millis();

    if (!client_.connected()) {
      if ((int32_t)(now - nextConnectAt_) < 0) return false;
      nextConnectAt_ = now + backoffMs_;
      if (client_.connect(host_, port_)) {
        Serial.printf("[radio] connected %s:%u\n", host_, port_);
        backoffMs_ = 1000;
        lastPollAt_ = 0;
      } else {
        Serial.printf("[radio] connect failed (%s:%u), backoff=%lums\n",
                      host_, port_, (unsigned long)backoffMs_);
        uint32_t nb = backoffMs_ * 2;
        if (nb > 8000) nb = 8000;
        backoffMs_ = nb;
        return false;
      }
    }

    if ((int32_t)(now - lastPollAt_) >= (int32_t)pollIntervalMs_) {
      lastPollAt_ = now;
      client_.print("IF;");
    }

    bool gotFresh = false;
    while (client_.available()) {
      String r = client_.readStringUntil(';');
      if (r.length() >= 13 && r.startsWith("IF")) {
        long f = r.substring(2, 13).toInt();
        if (f > 0) {
          frequencyHz_ = f;
          lastFreqAt_ = now;
          gotFresh = true;
        }
      }
    }
    return gotFresh;
  }

  bool stale(uint32_t timeoutMs = 5000) const {
    return (millis() - lastFreqAt_) > timeoutMs;
  }

private:
  WiFiClient  client_;
  const char* host_ = nullptr;
  uint16_t    port_ = 0;
  uint16_t    pollIntervalMs_ = 300;
  uint32_t    backoffMs_ = 1000;
  uint32_t    nextConnectAt_ = 0;
  uint32_t    lastPollAt_ = 0;
  uint32_t    lastFreqAt_ = 0;
  long        frequencyHz_ = 0;
};

}  // namespace bpf

#endif
