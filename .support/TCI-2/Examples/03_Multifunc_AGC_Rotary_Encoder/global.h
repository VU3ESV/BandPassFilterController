#ifndef GLOBAL_h
#define GLOBAL_h

#include "TCI.h"

#define TCI_CONNECTED_PIN 2

static char *ap_ssid = "TCI_ESP32";
static char *ap_pass = "12345678";
static char *wlan_ssid = "YOUR_WIFI_SSID";
static char *wlan_pass = "YOUR_WIFI_PASS";
static char *tci_host = "192.168.0.132"; 
static unsigned int tci_port = 40001;
static unsigned int iaru_region = 1;


TCI tci;

//Pin definitions
#define AGC_NORM_LED 12
#define AGC_NORM_FAST 13

#define ROTARY_ENCODER_A_PIN 19
#define ROTARY_ENCODER_B_PIN 18
#define ROTARY_ENCODER_BUTTON_PIN 21
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 1

//instead of changing here, rather change numbers above
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, 
                     ROTARY_ENCODER_B_PIN, -1, 
                     ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

using namespace ace_button;
AceButton agc_button(ROTARY_ENCODER_BUTTON_PIN);
void handleAgcButtonEvent(AceButton*, uint8_t, uint8_t);                     

#endif
