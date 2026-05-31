#include <AceButton.h>
#include "AiEsp32RotaryEncoder.h"

//includes required by the TCI library
#include "global.h"
#include "network.h"
#include "tci_events.h"

//Set TCI parameters in the "global.h" file
//Set pins definition in the "global.h" file

int last_agc_mode;

void handleAgcButtonEvent(AceButton* /* button */, 
                          uint8_t eventType,
                          uint8_t /* buttonState */)
{
  switch (eventType) {
    case AceButton::kEventClicked:  //Single click toggle AGC between FAST and NORMAL
      if (tci.rtx[0].getAgcMode() != 0) {  //If AGC isn't OFF
        if (tci.rtx[0].getAgcMode() == 1) 
          tci.set_agc_mode(0,2);  //2 = AGC_NORMAL
        else  
          tci.set_agc_mode(0,1);  //1 = AGC_FAST  
      }    
      break;
    case AceButton::kEventLongPressed:
      if (tci.rtx[0].getAgcMode() != 0) {  //If AGC isn't OFF
        last_agc_mode = tci.rtx[0].getAgcMode(); //save the current AGC mode
        tci.set_agc_mode(0,0); //Turn AGC off
      } else {
        if (last_agc_mode != 0)
          tci.set_agc_mode(0,last_agc_mode);
        else   
          tci.set_agc_mode(0,2); //NORMAL is the default mode when the rig starts with AGC off
      }      
      break;
    default:
      //other events
      break;  
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  //Led config
  pinMode(TCI_CONNECTED_PIN,OUTPUT);
  digitalWrite(TCI_CONNECTED_PIN, LOW);

  //Button config
  pinMode(ROTARY_ENCODER_BUTTON_PIN,INPUT_PULLUP);
  agc_button.getButtonConfig()->setFeature(ButtonConfig::kFeatureClick);
  agc_button.getButtonConfig()->setFeature(ButtonConfig::kFeatureLongPress);
  agc_button.setEventHandler(handleAgcButtonEvent);


  //Rotary encoder config
  rotaryEncoder.begin();
  rotaryEncoder.setup([] { rotaryEncoder.readEncoder_ISR(); },
                      [] { });

  bool circleValues = false;
  rotaryEncoder.setBoundaries(-20, 120, circleValues); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)
  rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
  //rotaryEncoder.setAcceleration(50); //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration
 

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

  if (rotaryEncoder.encoderChanged())
  {
    tci.set_agc_gain(0,rotaryEncoder.readEncoder());
  }

  agc_button.check();

  /* The same project can be used for a magnetic/optical 
   * high speed rotary encoder that simulate the main
   * VFO Knob. In such a case it is better to send the 
   * set_vfo command at regular time intervals. 
   * This trick will avoid the output buffer overflow
   * making the application more responsive as well
   * To do it:

     //1- Define a timer variable and a sent interval
     unsigned long previousMillis = 0;
     const long send_interval = 100; //ms - means max 10 times per second  
     
     //2- Config the vfo encoder in the following way
     rotaryVfoEncoder.setBoundaries(10000, 150000000, false); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)  
     rotaryVfoEncoder.setAcceleration(50); //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration

     //3- In the loop function use the following code 
      ... 
      if (rotaryVfoEncoder.encoderChanged())
      {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= send_interval) {
          previousMillis = currentMillis;
          tci.set_vfo(0,0,rotaryVfoEncoder.readEncoder());
        }
      }
      ...

  */

       
}
