//includes required by the TCI library
#include "global.h"
#include "network.h"
#include "tci_events.h"

//other includes
#include <AceButton.h>

//Set TCI parameters in the "global.h" file
//Set pins definition in the "global.h" file

//Buttons configuration
using namespace ace_button;
AceButton btn_mute(BTN_MUTE);
void handleMuteEvent(AceButton*, uint8_t, uint8_t);
AceButton btn_apf(BTN_APF);
void handleApfEvent(AceButton*, uint8_t, uint8_t);

//MUTE is a rig related event
void handleMuteEvent(AceButton* /* button */, 
                     uint8_t eventType,
                     uint8_t /* buttonState */) {
  switch (eventType) {
    case AceButton::kEventPressed:      
      if (tci.is_mute()) {
        tci.set_mute(false);
      } else {
        tci.set_mute(true);
      }
      break;
  }
}

//APF is an RX related event
void handleApfEvent(AceButton* /* button */, 
                     uint8_t eventType,
                     uint8_t /* buttonState */) {  
  switch (eventType) {
    case AceButton::kEventPressed:  
      if (tci.rtx[0].isRxApfEnable()) {
        tci.set_rx_apf_enable(0,false);
      } else {
        tci.set_rx_apf_enable(0,true);
      }
      break;
  }
}

void buttonHandler(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  switch (button->getPin()) {
    case BTN_MUTE:
      handleMuteEvent(button, eventType, buttonState);
      break;
    case BTN_APF:
      handleApfEvent(button, eventType, buttonState);
      break;
  }
}

//In the "tci_events.h" file you can handle 
//the MUTE and APF events. 
//In this example two leds will show the MUTE and APF status

void setup() {
  Serial.begin(115200);
  delay(2000);

  //Led config
  pinMode(TCI_CONNECTED_PIN,OUTPUT);
  digitalWrite(TCI_CONNECTED_PIN, LOW);
  pinMode(LED_MUTE,OUTPUT);
  digitalWrite(LED_MUTE, LOW);
  pinMode(LED_APF,OUTPUT);
  digitalWrite(LED_APF, LOW);
  //Button config
  pinMode(BTN_MUTE,INPUT_PULLUP);
  pinMode(BTN_APF,INPUT_PULLUP);
  ButtonConfig* config = ButtonConfig::getSystemButtonConfig();
  config->setEventHandler(buttonHandler);

  //TCI mandatory methods
  configure_tci_events();
  tci.set_host(tci_host);
  tci.set_port(tci_port);
  tci.set_iaru_region(iaru_region);  
  init_network();

  Serial.println("Setup done");
}

void loop() {  
  // Do not use the "delay" command 
  // Use instead the "millis()" command as explained in the following article
  // https://www.digikey.com/en/maker/blogs/2022/how-to-avoid-using-the-delay-function-in-arduino-sketches
  
  btn_mute.check();
  btn_apf.check();
    
}
