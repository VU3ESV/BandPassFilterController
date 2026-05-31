//includes required by the TCI library
#include "global.h"
#include "network.h"
#include "tci_events.h"

//Set TCI parameters in the "global.h" file
//Set pins definition in the "global.h" file


void setup() {
  Serial.begin(115200);
  delay(2000);

  //Led config
  pinMode(TCI_CONNECTED_PIN,OUTPUT);
  digitalWrite(TCI_CONNECTED_PIN, LOW);

  //TCI mandatory methods
  configure_tci_events();
  tci.set_host(tci_host);
  tci.set_port(tci_port);
  tci.set_iaru_region(iaru_region);  
  init_network();

  Serial.println("Setup done");
}

//Set the main volume
void handle_volume_pot() {
  
  int pot_read = analogRead(VOLUME_PIN);
  int volume_value = map(pot_read,0,4095,-60,0);
  
  if ((volume_value >= tci.get_volume()+2) ||
      (volume_value <= tci.get_volume()-2)) {
    if (tci.connected()) {
      tci.set_volume(volume_value);    
      Serial.printf("pot_read:%d    volume_value:%d     tci.get_volume():%d   \n",
                    pot_read,
                    volume_value,
                    tci.get_volume());
    }
  }  
  delay(100);      
}

void loop() {  
  // Do not use the "delay" command 
  // Use instead the "millis()" command as explained in the following article
  // https://www.digikey.com/en/maker/blogs/2022/how-to-avoid-using-the-delay-function-in-arduino-sketches

  handle_volume_pot();
     
}
