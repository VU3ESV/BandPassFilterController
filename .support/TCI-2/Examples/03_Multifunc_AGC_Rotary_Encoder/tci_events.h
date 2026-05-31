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
        //reset encoder flag
    }  
}

// ===== Please, for better parameters understanding
// ===== read the TCI official documentation on
// ===== https://github.com/ExpertSDR3/TCI
//
// ===== TCI commands implementation follows the same order 
// ===== described in the above document

//
// ********************** INITIALIZATIONS COMMANDS ***********************
//

// ===== AGC_MODE =====
void on_agc_mode_event(int rtxId) {
    switch (tci.rtx[rtxId].getAgcMode()) {
        case 0:
            Serial.printf("== TCI: agc_mode event - rtxId:%d - value:off\n", rtxId);
            break;
        case 1:
            Serial.printf("== TCI: agc_mode event - rtxId:%d - value:fast\n", rtxId);
            break;
        case 2:
            Serial.printf("== TCI: agc_mode event - rtxId:%d - value:normal\n", rtxId);
            break;                         
    }
}

// ===== AGC_GAIN =====
void on_agc_gain_event(int rtxId) {
  rotaryEncoder.setEncoderValue(tci.rtx[rtxId].getAgcGain());    
  Serial.printf("== TCI: agc_gain event - rtxId:%d - dB value:%d\n", rtxId, tci.rtx[rtxId].getAgcGain());
}

// =========================================================
// =========================================================

void configure_tci_events()
{
  tci.attach_conn_disc_event(on_connect_disconnect_event);
  tci.attach_agc_mode_event(on_agc_mode_event);
  tci.attach_agc_gain_event(on_agc_gain_event);
}

#endif
