
/*  
Skech to control the 2A2R switch and the 5B4AGN BPF filters along with a the Flex 6600 radio. 
Since the Radio Supports SO2R, the combination of 2A2R switch and the Yaesu Band Data will be used to keep the 5B4AGN Filter to the Slice which is not ment for Txing. 
When the Slice 1 is configured for Txing, the BPF will be switched to the Slice 2 and the frequency of Slice 2 will be used to determine the filter, viceversa for the Slice 2 when it is configured for Tx
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>


const char *ssid = "PiHackerNet_Mesh";       // Wifi ssid
const char *pass = "Ea3Hso4x6TtEu1ky123#$";  // Wifi password
const char *mqtt_server = "192.168.86.43";
const char *client_id = "ESP32_1";
const char *user_t = "pi";
const char *password_t = "Fibria123#";

const char *topic_flex_slice_a_freq = "flex/SliceA/freq";
const char *topic_flex_slice_b_freq = "flex/SliceB/freq";
const char *topic_flex_slice_a_active = "flex/SliceA/active";
const char *topic_flex_slice_b_active = "flex/SliceB/active";
const char *topic_flex_slice_a_mode = "flex/SliceA/mode";
const char *topic_flex_slice_b_mode = "flex/SliceB/mode";

const char *topic_flex_slice_active_tx_freq = "flex/SliceActive/transmit/freq";
const char *topic_flex_slice_active_tx_state = "flex/SliceActive/transmit/state";

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
float freq = 0;
float Radio1Frequency;
float Radio2Frequency;
bool Radio1Tx;
bool Radio2Tx;
float RadioActiveSlice_Tx_Freq;
String RadioActiveSlice_Tx_State = "";
String Radio1Mode = "";
String Radio2Mode = "";
int lcdColumns = 16;
int lcdRows = 2;

int bcd_A = 16, bcd_B = 17, bcd_C = 18, bcd_D = 19;
int inhibit = 26, R1_Tx = 27, R2_Tx = 32, relay8 = 33;

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

// define two task handler for using multitasking with ESP32
TaskHandle_t MQTTask;
TaskHandle_t DisplayTask;
SemaphoreHandle_t sema_Data_update;
void SetBPF(float frequency);

void setup() {
  pinMode(bcd_A, OUTPUT);
  pinMode(bcd_B, OUTPUT);
  pinMode(bcd_C, OUTPUT);
  pinMode(bcd_D, OUTPUT);
  pinMode(inhibit, OUTPUT);
  pinMode(R1_Tx, OUTPUT);
  pinMode(R2_Tx, OUTPUT);
  pinMode(relay8, OUTPUT);

// this is needed to keep the relays in inactive state during start
  digitalWrite(bcd_A, HIGH);
  digitalWrite(bcd_B, HIGH);
  digitalWrite(bcd_C, HIGH);
  digitalWrite(bcd_D, HIGH);
  digitalWrite(inhibit, HIGH);  
  digitalWrite(R1_Tx, HIGH);
  digitalWrite(R2_Tx, HIGH);
  digitalWrite(relay8, HIGH);



  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Station Master");
  sema_Data_update = xSemaphoreCreateBinary();
  xSemaphoreGive(sema_Data_update);

  // we use Multitasking with the ESP32
  // Task 1 MQTT Message handling loop
  xTaskCreatePinnedToCore(
    MQTTLoop,      /* Function to implement the task */
    "MQTTask",     /* Name of the task */
    10000,         /* Stack size in words */
    NULL,          /* Task input parameter */
    2,             /* Priority of the task */
    &MQTTask,      /* Task handle. */
    0);            /* Core where the task should run */

  // Task 2 Display loop
  xTaskCreatePinnedToCore(
    DisplayLoop,   /* Function to implement the task */
    "DisplayTask", /* Name of the task */
    10000,         /* Stack size in words */
    NULL,          /* Task input parameter */
    2,             /* Priority of the task */
    &DisplayTask,  /* Task handle. */
    1);            /* Core where the task should run */
}

void MQTTLoop(void *pvParameters) {
  setupWifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  for (;;) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    vTaskDelay(1);
  }
  vTaskDelete(NULL);
}

void DisplayLoop(void *pvParameters) {

  float displayRadio1Frequency = 0.0f;
  float displayRadio2Frequency = 0.0f;
  bool displayRadio1Tx;
  bool displayRadio2Tx;
  String displayRadio1Mode = "";
  String displayRadio2Mode = "";

  for (;;) {

    xSemaphoreTake(sema_Data_update, portMAX_DELAY);
    displayRadio1Frequency = Radio1Frequency;
    displayRadio2Frequency = Radio2Frequency;
    displayRadio1Mode = Radio1Mode;
    displayRadio2Mode = Radio2Mode;
    displayRadio1Tx = Radio1Tx;
    displayRadio2Tx = Radio2Tx;
    xSemaphoreGive(sema_Data_update);

    lcd.setCursor(0, 0);
    lcd.print(displayRadio1Frequency, 6);
    lcd.setCursor(10, 0);
    lcd.print(displayRadio1Mode);
    lcd.setCursor(14, 0);
    if (displayRadio1Tx == true) {
      lcd.print("Tx");
    } else {
      lcd.print("Rx");
    }
    lcd.setCursor(0, 1);
    lcd.print(displayRadio2Frequency, 6);
    lcd.setCursor(10, 1);
    lcd.print(displayRadio2Mode);
    lcd.setCursor(14, 1);
    if (displayRadio2Tx == true) {
      lcd.print("Tx");
    } else {
      lcd.print("Rx");
    }
    vTaskDelay(500);  //task runs approx every 1000 mS
    lcd.clear();
  }
  vTaskDelete(NULL);
}

void setupWifi() {
  WiFi.begin(ssid, pass);
  Serial.print("Connecting Wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(500);
    Serial.print(".");
  }

  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  Serial.println("Wifi Connected");
  Serial.println(WiFi.localIP());
  vTaskDelay(5000);
}

// The 5B4AGN filter will be switched using a 2A2R switch from the VUCG.
// Plan is to keep the BPF in the line of RX Radio
void callback(char *topic, byte *message, unsigned int length) {
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }
  String message_topic = String(topic);

  if (message_topic == topic_flex_slice_a_freq) {
    xSemaphoreTake(sema_Data_update, portMAX_DELAY);
    Radio1Frequency = messageTemp.toFloat();
    // Serial.println(Radio1Frequency, 6);
    xSemaphoreGive(sema_Data_update);
  } else if (message_topic == topic_flex_slice_b_freq) {
    xSemaphoreTake(sema_Data_update, portMAX_DELAY);
    Radio2Frequency = messageTemp.toFloat();
    // Serial.println(Radio2Frequency, 6);
    xSemaphoreGive(sema_Data_update);
  } else if (message_topic == topic_flex_slice_a_active) {
    if (messageTemp == "1") {
      xSemaphoreTake(sema_Data_update, portMAX_DELAY);
      Radio1Tx = true;
      xSemaphoreGive(sema_Data_update);
      digitalWrite(R1_Tx, LOW);
    } else if (messageTemp == "0") {
      xSemaphoreTake(sema_Data_update, portMAX_DELAY);
      Radio1Tx = false;
      xSemaphoreGive(sema_Data_update);
      digitalWrite(R1_Tx, HIGH);
    }
  } else if (message_topic == topic_flex_slice_b_active) {
    if (messageTemp == "1") {
      xSemaphoreTake(sema_Data_update, portMAX_DELAY);
      Radio2Tx = true;
      xSemaphoreGive(sema_Data_update);
      digitalWrite(R2_Tx, LOW);
    } else if (messageTemp == "0") {
      xSemaphoreTake(sema_Data_update, portMAX_DELAY);
      Radio2Tx = false;
      xSemaphoreGive(sema_Data_update);
      digitalWrite(R2_Tx, HIGH);
    }
  } else if (message_topic == topic_flex_slice_active_tx_freq) {
    xSemaphoreTake(sema_Data_update, portMAX_DELAY);
    RadioActiveSlice_Tx_Freq = messageTemp.toFloat();
    xSemaphoreGive(sema_Data_update);
  } else if (message_topic == topic_flex_slice_active_tx_state) {
    xSemaphoreTake(sema_Data_update, portMAX_DELAY);
    RadioActiveSlice_Tx_State = "";
    RadioActiveSlice_Tx_State = messageTemp;
    xSemaphoreGive(sema_Data_update);
  } else if (message_topic == topic_flex_slice_a_mode) {
    xSemaphoreTake(sema_Data_update, portMAX_DELAY);
    Radio1Mode = "";
    Radio1Mode = messageTemp;
    xSemaphoreGive(sema_Data_update);
  } else if (message_topic == topic_flex_slice_b_mode) {
    xSemaphoreTake(sema_Data_update, portMAX_DELAY);
    Radio2Mode = "";
    Radio2Mode = messageTemp;
    xSemaphoreGive(sema_Data_update);
  }

  if (Radio1Tx == true) {
    SetBPF(Radio2Frequency);  // When R1 is set for Tx the filters will be set to the frequency of R2 which is in Rx mode
  } else if (Radio2Tx == true) {
    SetBPF(Radio1Frequency);  // When R2 is set for Tx the filters will be set to the frequency of R1 which is in Rx mode
  } else {
    SetBPF(0.00f);  //Inhibit the filters
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(client_id, user_t, password_t)) {
      Serial.print("connected");
      // Subscribe
      client.subscribe(topic_flex_slice_a_freq);
      client.subscribe(topic_flex_slice_b_freq);
      client.subscribe(topic_flex_slice_a_active);
      client.subscribe(topic_flex_slice_b_active);
      client.subscribe(topic_flex_slice_active_tx_freq);
      client.subscribe(topic_flex_slice_active_tx_state);
      client.subscribe(topic_flex_slice_a_mode);
      client.subscribe(topic_flex_slice_b_mode);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.print(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      vTaskDelay(5000);
    }
  }
}

void loop() {
}

// Function to set the Band Data in 5B4AGN BPF, for the WARC Bands and 6M the inhibit will be active and the band data pins are forced to low
void SetBPF(float frequency) {
  if (frequency >= 1.81 && frequency <= 2.00) {  //160m
    digitalWrite(bcd_A, LOW);
    digitalWrite(bcd_B, HIGH);
    digitalWrite(bcd_C, HIGH);
    digitalWrite(bcd_D, HIGH);
    digitalWrite(inhibit, HIGH);
    return;
  } else if (frequency >= 3.5 && frequency <= 3.80) {  //80m
    digitalWrite(bcd_A, HIGH);
    digitalWrite(bcd_B, LOW);
    digitalWrite(bcd_C, HIGH);
    digitalWrite(bcd_D, HIGH);
    digitalWrite(inhibit, HIGH);
    return;
  } else if (frequency >= 5.35 && frequency <= 5.37) {  //60m
    digitalWrite(bcd_A, HIGH);
    digitalWrite(bcd_B, HIGH);
    digitalWrite(bcd_C, HIGH);
    digitalWrite(bcd_D, HIGH);
    digitalWrite(inhibit, LOW);
    return;
  } else if (frequency >= 7.00 && frequency <= 7.20) {  //40m
    digitalWrite(bcd_A, LOW);
    digitalWrite(bcd_B, LOW);
    digitalWrite(bcd_C, HIGH);
    digitalWrite(bcd_D, HIGH);
    digitalWrite(inhibit, HIGH);
    return;
  } else if (frequency >= 10.10 && frequency <= 10.15) {  //30m
    digitalWrite(bcd_A, HIGH);
    digitalWrite(bcd_B, HIGH);
    digitalWrite(bcd_C, HIGH);
    digitalWrite(bcd_D, HIGH);
    digitalWrite(inhibit, LOW);
    return;
  } else if (frequency >= 14.00 && frequency <= 14.35) {  //20M
    digitalWrite(bcd_A, LOW);
    digitalWrite(bcd_B, HIGH);
    digitalWrite(bcd_C, LOW);
    digitalWrite(bcd_D, HIGH);
    digitalWrite(inhibit, HIGH);
    return;
  } else if (frequency >= 18.06 && frequency <= 18.16) {  //17m
    digitalWrite(bcd_A, HIGH);
    digitalWrite(bcd_B, HIGH);
    digitalWrite(bcd_C, HIGH);
    digitalWrite(bcd_D, HIGH);
    digitalWrite(inhibit, LOW);
    return;
  } else if (frequency >= 21.00 && frequency <= 21.45) {  //15M
    digitalWrite(bcd_A, LOW);
    digitalWrite(bcd_B, LOW);
    digitalWrite(bcd_C, LOW);
    digitalWrite(bcd_D, HIGH);
    digitalWrite(inhibit, HIGH);
    return;
  } else if (frequency >= 24.89 && frequency <= 24.99) {  //12m
    digitalWrite(bcd_A, HIGH);
    digitalWrite(bcd_B, HIGH);
    digitalWrite(bcd_C, HIGH);
    digitalWrite(bcd_D, HIGH);
    digitalWrite(inhibit, LOW);
    return;
  } else if (frequency >= 28.00 && frequency <= 29.70) {  //10M
    digitalWrite(bcd_A, LOW);
    digitalWrite(bcd_B, HIGH);
    digitalWrite(bcd_C, HIGH);
    digitalWrite(bcd_D, LOW);
    digitalWrite(inhibit, HIGH);
    return;
  } else if (frequency >= 50.00 && frequency <= 52.00) {  //6M
    digitalWrite(bcd_A, HIGH);
    digitalWrite(bcd_B, HIGH);
    digitalWrite(bcd_C, HIGH);
    digitalWrite(bcd_D, HIGH);
    digitalWrite(inhibit, LOW);
    return;
  } else {
    digitalWrite(bcd_A, HIGH);
    digitalWrite(bcd_B, HIGH);
    digitalWrite(bcd_C, HIGH);
    digitalWrite(bcd_D, HIGH);
    digitalWrite(inhibit, LOW);
    return;
  }
}