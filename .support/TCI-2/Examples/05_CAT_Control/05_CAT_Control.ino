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

  //CAT Config
  PORT2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  //This is the receiver Id which the CAT port will respond on
  int rx_id = 1; //1=RX1 - 2=RX2 
  tci.set_cat_port(1,&PORT2,rx_id); //now RX1 is connected to Serial2

  Serial.println("Setup done");
  
  //TCI mandatory methods
  configure_tci_events();
  tci.set_host(tci_host);
  tci.set_port(tci_port);
  tci.set_iaru_region(iaru_region);  
  init_network();

}


void loop() {  
  // Do not use the "delay" command 
  // Use instead the "millis()" command as explained in the following article
  // https://www.digikey.com/en/maker/blogs/2022/how-to-avoid-using-the-delay-function-in-arduino-sketches

  //CAT port communication is handled in a separate task
  //You only need to configure it properly
  
}
