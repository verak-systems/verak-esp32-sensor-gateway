#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Config.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Secrets.h>
#include <string>
#include <ArduinoJson.h>
#include <time.h>


#define TEMP_PIN 10
#define THERMISTOR_PIN 1
#define SLEEP_TIME 10
#define MAX_WIFI_TRIES 20
#define MAX_MQTT_TRIES 20


OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

const char* ntpServer = "pool.ntp.org";
const int  estOffset_sec = -18000;
const char* unit = "°C";

int status = WL_IDLE_STATUS;
WiFiClient wifiClient;
PubSubClient mqttClient;
JsonDocument data;


String buildJson(JsonDocument& doc, const char* device, const char* key, const float value, const char* unit){
  doc["device"] = device;
  doc[key] = value;
  doc["unit"] = unit;
  // TODO: Add dst check somewhere, or would this be better done on the rpi?
  struct tm timeinfo;
  char timestamp[32];

  if(getLocalTime(&timeinfo)){
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
  }
  else{
    strcpy(timestamp, "Unavailable");
  }

  doc["timestamp"] = timestamp;

  String payload;
  serializeJson(doc, payload);

  return payload;
}

float calcThermistorTemp(float res){

  return 0;
}

void setup() {
  Serial.begin(115200);
  sensors.begin();
  sensors.setResolution(12);
  pinMode(THERMISTOR_PIN, INPUT);
  configTime(estOffset_sec, 0, ntpServer);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int wifiTries = 0;

  while (WiFi.status() != WL_CONNECTED && wifiTries <= MAX_WIFI_TRIES){
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  Serial.print("Wifi connected!\n");

  mqttClient.setClient(wifiClient);
  mqttClient.setServer(BROKER, MQTT_PORT);

  int mqttTries = 0;

  while (!mqttClient.connect("esp32-Pub", MQTT_USER, MQTT_PASS) && mqttTries <= MAX_MQTT_TRIES){
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  Serial.print("Connected to MQTT Broker\n");

  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  int analogValue = analogRead(THERMISTOR_PIN);
  Serial.print("Thermistor ADC Value: ");
  Serial.println(analogValue);

  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: sensor not found. Check wiring and pull-up resistor.");
    data["status"] = "Unavailable";
  }

  String payload = buildJson(data, "ESP32", "digitalTemp", tempC, "°C");
  mqttClient.publish(DIGITAL_TEMP_TOPIC, payload.c_str(), true);
  data.clear();

  esp_deep_sleep(1000000LL * SLEEP_TIME);

}

void loop() {
}
