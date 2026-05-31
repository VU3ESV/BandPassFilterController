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
    displayVfo();
}

// ===== Please, for better parameters understanding
// ===== read the TCI official documentation on
// ===== https://github.com/ExpertSDR3/TCI
//
// ===== TCI commands implementation follows the same order 
// ===== described in the above document

// ===== VFO =====
void on_vfo_event(int rtxId, int vfoId) {  
    Serial.printf("== TCI: VFO event - rtxId:%d - vfoId:%d - VFO freq:%d\n", rtxId, vfoId, tci.rtx[rtxId].getVfo(vfoId));
    displayVfo();
}


void configure_tci_events()
{
  tci.attach_conn_disc_event(on_connect_disconnect_event);
  tci.attach_vfo_event(on_vfo_event);
}

#endif
