#ifndef MONITOR_H 
#define MONITOR_H 

#include <Arduino.h>
#include "WiFi.h"

static int wifiStatus = -1;
static IPAddress  ip;
static TaskHandle_t monitor_taskh; // Monitor Task handle
static unsigned int current_heap = 0;

static void monitor_task(void *argp) {
    unsigned int stack_hwm = 0, temp;    
    int tmpStatus;    
    const long interval = 10000;
    unsigned long previousMillis = 0;

    for (;;) {
        
        temp = uxTaskGetStackHighWaterMark(nullptr);
        //CHeck stacksize
        if ( !stack_hwm || temp < stack_hwm ) {
            stack_hwm = temp;
            /*Serial.printf("= Monitor task has stack hwm %u\n",
                            stack_hwm);*/
        }

        tmpStatus = WiFi.status();

        if (tmpStatus != wifiStatus) {

            wifiStatus = tmpStatus;
            switch (wifiStatus) {

                case WL_NO_SHIELD:
                    Serial.printf("Wifi: no shield\n");
                    break;
                case WL_IDLE_STATUS:
                    Serial.printf("Wifi: idle\n");
                    break;                    
                case WL_NO_SSID_AVAIL:
                    Serial.printf("Wifi: no ssid available\n");
                    break;                    
                case WL_SCAN_COMPLETED:
                    Serial.printf("Wifi: scan completed\n");
                    break;                    
                case WL_CONNECTED:
                    ip = WiFi.localIP();
                    Serial.printf("Wifi: connected - IP: %d.%d.%d.%d\n",
                                  ip[0],ip[1],ip[2],ip[3]);
                    Serial.printf("      RSSI %d dBm\n",WiFi.RSSI());                    
                    delay(1000);
                    tci.connect();
                    break;                    
                case WL_CONNECT_FAILED:                    
                    Serial.printf("Wifi: connect failed\n");
                    break;                    
                case WL_CONNECTION_LOST:
                    Serial.printf("Wifi: lost connection\n");
                    break;                    
                case WL_DISCONNECTED:
                    Serial.printf("Wifi: disconnected\n");
                    break;                    
                default:
                    Serial.printf("Wifi: no valid status\n");
                    break;                                 
            }

        }
        
        unsigned long currentMillis = millis();

        if (currentMillis - previousMillis >= interval) {
            previousMillis = currentMillis;
            unsigned int heap = ESP.getFreeHeap();
            if (heap != current_heap) 
            {
                current_heap = heap;
                /*Serial.printf("= Heap: %u\n", current_heap);*/
            }
            
        }

        delay(1);
    }

}

void init_network()
{

    int app_cpu = xPortGetCoreID();
    Serial.printf("app_cpu is %d (%s core)\n", 
                app_cpu,
                app_cpu > 0 ? "Dual" : "Single");

    if (!monitor_taskh) {
        BaseType_t rc_monitor;
        rc_monitor = xTaskCreatePinnedToCore(
            monitor_task,
            "monitor_task",
            2048,
            nullptr,
            1,
            &monitor_taskh,
            app_cpu);
        assert(rc_monitor == pdPASS);
    }

    WiFi.disconnect();
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ap_ssid, ap_pass);
    IPAddress apIp = WiFi.softAPIP();
        
    Serial.printf("Access point SSID: %s\nAccess point PASS: %s\nAccess Point IP address: %d.%d.%d.%d\n", 
                         ap_ssid, ap_pass, apIp[0],apIp[1],apIp[2],apIp[3]);
    delay(1000);

    Serial.print("MAC >> ");
    Serial.println(WiFi.macAddress());
    Serial.printf("Connecting to WiFi: %s \n", wlan_ssid);
    WiFi.begin(wlan_ssid, wlan_pass);

}

#endif
