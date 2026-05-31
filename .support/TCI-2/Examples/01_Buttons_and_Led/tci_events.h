#ifndef TCI_EVENTS_h
#define TCI_EVENTS_h

#include "global.h"
#include "tci.h"

void on_connect_disconnect_event() {
    if(tci.connected()) {
        digitalWrite(TCI_CONNECTED_PIN, HIGH);        
        Serial.println("== TCI: Connected");
    } else {
        digitalWrite(TCI_CONNECTED_PIN, LOW);
        Serial.println("== TCI: Disconnected");
    }  
}

// ===== Please, for better parameters understanding
// ===== read the TCI official documentation on
// ===== https://github.com/ExpertSDR3/TCI
//
// ===== TCI commands implementation follows the same order 
// ===== described in the above document

// ===== MUTE =====
void on_mute_event() {
    Serial.printf("== TCI: mute event - value:%d\n", tci.is_mute());
    (tci.is_mute()) ? digitalWrite(LED_MUTE, HIGH) : digitalWrite(LED_MUTE, LOW);
}

// ===== RX_APF_ENABLE =====
void on_rx_apf_enable_event(int rtxId) {
    Serial.printf("== TCI: rx_apf_enable event - rtxId:%d - value:%s\n", 
                  rtxId, 
                  tci.rtx[rtxId].isRxApfEnable() ? "true" : "false");
    (tci.rtx[0].isRxApfEnable()) ? digitalWrite(LED_APF, HIGH) : digitalWrite(LED_APF, LOW);                                    
}

// =========================================================
//
// =========================================================

void configure_tci_events()
{
  tci.attach_conn_disc_event(on_connect_disconnect_event);
  tci.attach_mute_event(on_mute_event);
  tci.attach_rx_apf_enable_event(on_rx_apf_enable_event);  
}

#endif
