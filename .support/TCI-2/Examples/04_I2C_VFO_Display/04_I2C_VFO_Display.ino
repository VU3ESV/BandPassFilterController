#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_I2CDevice.h>

void displayVfo();

//includes required by the TCI library
#include "global.h"
#include "network.h"
#include "tci_events.h"

//Set TCI parameters in the "global.h" file
//Set pins definition in the "global.h" file

void displayVfo() {
  display.clearDisplay();
  display.drawFastHLine(0,43,127,WHITE);        
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  //RX1-VFOA
  int val = tci.rtx[0].getVfo(0);
  if (tci.connected()  && 
      val > 0 && val < 100000000) {
    int mhz = val / 1000000;
    int khz = (val - mhz * 1000000) / 1000;
    int hz = val - mhz * 1000000 - khz * 1000;
    char buf[11];
    sprintf(buf,"%02d.%03d.%03d",mhz,khz,hz);
    display.println(buf);
  } else {
    display.println("   .   .   ");
  }
  display.setCursor(0, 21);
  //RX1-VFOB              
  val = tci.rtx[0].getVfo(1);
  if (tci.connected() && 
      val > 0 && val < 100000000) {
    int mhz = val / 1000000;
    int khz = (val - mhz * 1000000) / 1000;
    int hz = val - mhz * 1000000 - khz * 1000;
    char buf[11];
    sprintf(buf,"%02d.%03d.%03d",mhz,khz,hz);
    display.println(buf);
  } else {
    display.println("   .   .   ");
  }              
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  //Led config
  pinMode(TCI_CONNECTED_PIN,OUTPUT);
  digitalWrite(TCI_CONNECTED_PIN, LOW);

  //Display config
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
      Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
  }
  // Clear the buffer.
  display.clearDisplay();
  displayVfo();

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

  //You don't need do display the vfo periodically
  //Just recall the displayVfo() function from the
  //event handler in the "tci_events.h" file
         
}
