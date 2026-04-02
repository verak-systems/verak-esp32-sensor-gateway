#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Config.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Secrets.h>
#include <string>
#include <ArduinoJson.h>
#include <time.h>


#define TEMP_PIN D7
#define THERMISTOR_PIN A0

OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

const char* ntpServer = "pool.ntp.org";
const int  estOffset_sec = -18000;

int status = WL_IDLE_STATUS;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
JsonDocument data;


String buildJson(JsonDocument& doc, char* device, char* key, float value, char* unit){
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
  serializeJson(data, payload);

  return payload;
}

void setup() {
  delay(100);
  Serial.begin(115200);
  sensors.begin();
  sensors.setResolution(12);
  Serial.println("DS18B20 ready");
  pinMode(THERMISTOR_PIN, INPUT);


  while (WiFi.status() != WL_CONNECTED){

    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    if (WiFi.status() != WL_CONNECTED){
      Serial.print("No Wifi Connection\n");
    }
    delay(1000);
  }

  Serial.print("Wifi connected!\n");

  configTime(estOffset_sec, 0, ntpServer);

  Serial.print("Connecting to MQTT Broker");

  mqttClient.setServer(BROKER, MQTT_PORT);

  while (!mqttClient.connect("esp32-Pub", MQQT_USER, MQTT_PASS)){
    Serial.print(".");

  }

  Serial.print("Connected to MQTT Broker\n");

  if (!WiFi.isConnected() || !mqttClient.connected()){
    Serial.print("Wifi/MQTT connection Dropped\nRetrying connection");
    while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      WiFi.reconnect();
      delay(1000);
    }

    while(!mqttClient.connected()){
      Serial.print(".");
      if (mqttClient.connect("esp32-Pub", MQQT_USER, MQTT_PASS)){break;}
    }

    Serial.print("RECONNECTED");
  }

  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  int analogValue = analogRead(THERMISTOR_PIN);
  Serial.print("Thermistor ADC Value: ");
  Serial.println(analogValue);

  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: sensor not found. Check wiring and pull-up resistor.");
    data['status'] = "Unavaible";
  } else {
    Serial.print("Temperature: ");
    Serial.print(tempC, 1);
    Serial.println(" °C");
  }

  String payload = buildJson(data, "ESP32", "digitalTemp", tempC, "°C");
  mqttClient.publish(TEMP_TOPIC, payload.c_str(), true);
  data.clear();

  payload = buildJson(data, "ESP32", "analogTemp", 0, "°C");
  mqttClient.publish(TEMP_TOPIC, payload.c_str(), true);
  data.clear();

  ESP.deepSleep(5e6);
}

void loop() {
  
}
