
/* Arduino Sketch to control the ESP12 based Wifi Relay Board. 
   ESP12 will send the IF; command to the TCP Server Exposed by the Thetis or to the TCP Server Exposed By Node Red
   Port Number will vary based on the TCP Server Configuration.
   Expected to Set Static IP or Reserve an IP for the Thetis PC /Raspberry PI runniung Node Red.
   Chnage the SSID and Password based on the users WIFI settings 
*/
#include <ESP8266WiFi.h>
const char* ssid = "";     // Wifi ssid
const char* pass = "";  // Wifi password

const IPAddress serverIP(192, 168, 86, 40);  // TCP Server IP - Chnage this to the IP address of the PC running Thetis or to the Raspberry PI running Node Red.
uint16_t serverPort = 13013;                // Port 13013 is the default port number used by the TCP Server  on Thetis and for NodeRed Flow it is 7355.

WiFiClient client;

String frequencyData;  //read data kenwood string
long freq = 0;

bool beginRead = false;
int arrayIndex = 0;

#define Relay1 5
#define Relay2 4
#define Relay3 0
#define Relay4 15
#define Relay5 13
#define Relay6 12
#define Relay7 14
#define Relay8 16

void SetBand(long frequency);
void SetOutput(int bandCode);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  Serial.println("Connecting Wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Wifi Connected");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.println("Connecting to TCP Socket...");
  while (!client.connected()) {
    client.connect(serverIP, serverPort);
    delay(1000);
  }
  Serial.println("TCP Socket Connected");

  pinMode(Relay1, OUTPUT);
  pinMode(Relay2, OUTPUT);
  pinMode(Relay3, OUTPUT);
  pinMode(Relay4, OUTPUT);
  pinMode(Relay5, OUTPUT);
  pinMode(Relay6, OUTPUT);
  pinMode(Relay7, OUTPUT);
  pinMode(Relay8, OUTPUT);

  digitalWrite(Relay1, LOW);
  digitalWrite(Relay2, LOW);
  digitalWrite(Relay3, LOW);
  digitalWrite(Relay4, LOW);
  digitalWrite(Relay5, LOW);
  digitalWrite(Relay6, LOW);
  digitalWrite(Relay7, LOW);
  digitalWrite(Relay8, LOW);
}

void loop() {
  while (!client.connected()) {
    client.connect(serverIP, serverPort);
    delay(1000);
    Serial.println("TCP Socket Reconnecting");
  }

  while (client.available()) {
    String readBuffer = client.readStringUntil(';');
    Serial.println(readBuffer);
    if (readBuffer.startsWith("IF")) {
      frequencyData = readBuffer.substring(2, 13);      
      freq = frequencyData.toInt();
      SetBand(freq);      
      frequencyData = "";
    }
  }

  if (client.connected()) {
    client.print("IF;");
    delay(300);
  }
}

void SetBand(long frequency) {
  if (frequency >= 1500000 && frequency <= 2000000) {  // 160m
    digitalWrite(Relay1, HIGH);
    digitalWrite(Relay2, LOW);
    digitalWrite(Relay3, LOW);
    digitalWrite(Relay4, LOW);
    digitalWrite(Relay5, LOW);
    digitalWrite(Relay6, LOW);
    digitalWrite(Relay7, LOW);
    digitalWrite(Relay8, LOW);
    return;
  } else if (frequency >= 3500000 && frequency <= 4000000) {  //  80m
    digitalWrite(Relay1, LOW);
    digitalWrite(Relay2, HIGH);
    digitalWrite(Relay3, LOW);
    digitalWrite(Relay4, LOW);
    digitalWrite(Relay5, LOW);
    digitalWrite(Relay6, LOW);
    digitalWrite(Relay7, LOW);
    digitalWrite(Relay8, LOW);
    return;
  } else if (frequency >= 5400000 && frequency < 12000000) {  //  30m/40m
    digitalWrite(Relay1, LOW);
    digitalWrite(Relay2, LOW);
    digitalWrite(Relay3, HIGH);
    digitalWrite(Relay4, LOW);
    digitalWrite(Relay5, LOW);
    digitalWrite(Relay6, LOW);
    digitalWrite(Relay7, LOW);
    digitalWrite(Relay8, LOW);
    return;
  } else if (frequency >= 12000000 && frequency < 18000000) {  //  20m/17m
    digitalWrite(Relay1, LOW);
    digitalWrite(Relay2, LOW);
    digitalWrite(Relay3, LOW);
    digitalWrite(Relay4, HIGH);
    digitalWrite(Relay5, LOW);
    digitalWrite(Relay6, LOW);
    digitalWrite(Relay7, LOW);
    digitalWrite(Relay8, LOW);
    return;
  } else if (frequency >= 18000000 && frequency <= 30000000) {  //  15m/12m/10m
    digitalWrite(Relay1, LOW);
    digitalWrite(Relay2, LOW);
    digitalWrite(Relay3, LOW);
    digitalWrite(Relay4, LOW);
    digitalWrite(Relay5, HIGH);
    digitalWrite(Relay6, LOW);
    digitalWrite(Relay7, LOW);
    digitalWrite(Relay8, LOW);
    return;
  } else {  // out of range
    digitalWrite(Relay1, LOW);
    digitalWrite(Relay2, LOW);
    digitalWrite(Relay3, LOW);
    digitalWrite(Relay4, LOW);
    digitalWrite(Relay5, LOW);
    digitalWrite(Relay6, LOW);
    digitalWrite(Relay7, LOW);
    digitalWrite(Relay8, LOW);
    return;
  }
}