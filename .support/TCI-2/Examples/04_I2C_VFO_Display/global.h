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
#define SDA 21
#define SCL 22

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool refresh_display;                    

#endif
