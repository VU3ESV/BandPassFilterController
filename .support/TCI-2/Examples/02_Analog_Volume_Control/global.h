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
#define VOLUME_PIN 34

#endif
