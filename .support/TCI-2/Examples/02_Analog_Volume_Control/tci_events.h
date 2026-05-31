#ifndef TCI_EVENTS_h
#define TCI_EVENTS_h

#include "global.h"
#include "tci.h"

//All the TCI events are handled for demo purpose
//You can comment out all the methods that aren't 
//involved in your project. 
//This will make more responsive the application

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

//
// ********************** INITIALIZATIONS COMMANDS ***********************
//

// ===== VOLUME =====
void on_volume_event() {
    Serial.printf("== TCI: volume event - dB value:%d\n", tci.get_volume());
}

void configure_tci_events()
{
  tci.attach_conn_disc_event(on_connect_disconnect_event);
  tci.attach_volume_event(on_volume_event);
}

#endif
