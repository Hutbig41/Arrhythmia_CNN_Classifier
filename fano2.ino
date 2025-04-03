#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <WiFiClientSecure.h>
#include <LiquidCrystal_I2C.h>

// WiFi Configuration
#define WIFISSID "Mywifi"
#define PASSWORD "12345677"

// MQTT Topics
#define ECG_TOPIC "ecg_data"
#define SPO2_TOPIC "spo2_data"
#define PULSE_TOPIC "pulse_data"
#define DEVICE_LABEL "esp8266"

// Hardware Configuration
#define ECG_PIN A0
#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

// HiveMQ Cloud Configuration
const char* mqttServer = "pinktawny-f8qxyf.a01.euc1.aws.hivemq.cloud";
const int mqttPort = 8883;
const char* mqttUser = "hivemq.webclient.1742745732084";
const char* mqttPassword = ".8e9TrLh$C@0lA,7nJEj";

// Sensor Objects
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
MAX30105 particleSensor;
WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

// Sensor Data Buffers
uint32_t irBuffer[100];   // IR LED data
uint32_t redBuffer[100];  // Red LED data

// Sensor Values
float ecgValue = 0;
int32_t spo2Value = 0;
int32_t heartRate = 0;
int8_t validSPO2 = 0;
int8_t validHeartRate = 0;

// Timing Control
unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 10;    // 1 second
unsigned long lastSensorReadTime = 0;
const unsigned long sensorReadInterval = 20; // 4 seconds

void initializeLCD() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("   Dynabook   ");
  delay(1000);
  lcd.clear();
}

void connectToWiFi() {
  WiFi.begin(WIFISSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void initializeMAX30102() {
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    lcd.print("Sensor Error!");
    while (1);
  }

  // Optimal settings for pulse and SpO2
  particleSensor.setup(60, 4, 2, 100, 411, 4096);
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeGreen(0x1F);
}

void readSensors() {
  // Read ECG (continuous)
  ecgValue = analogRead(ECG_PIN);

  // Read MAX30102 data periodically
  if (millis() - lastSensorReadTime >= sensorReadInterval) {
    lastSensorReadTime = millis();
    
    // Collect 100 samples (takes ~4 seconds at 25sps)
    for (byte i = 0; i < 100; i++) {
      while (!particleSensor.available()) {
        particleSensor.check();
      }
      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample();
    }

    // Calculate SpO2 and Heart Rate
    maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer, 
                                         &spo2Value, &validSPO2, &heartRate, &validHeartRate);
    
    // Apply pulse rate division (as requested)
    if (validHeartRate) {
      heartRate = heartRate / 2;
    }
  }
}

void updateDisplay() {
  lcd.setCursor(0, 0);
  lcd.print("Pulse: ");
  lcd.print(validHeartRate ? heartRate : 0);
  lcd.print(" bpm  ");
  
  lcd.setCursor(0, 1);
  lcd.print("SpO2: ");
  lcd.print(validSPO2 ? spo2Value : 0);
  lcd.print("%   ");
}

void publishData() {
  char payload[10];
  
  // Publish ECG
  dtostrf(ecgValue, 4, 2, payload);
  client.publish(ECG_TOPIC, payload);

  // Publish SpO2 if valid
  if (validSPO2) {
    itoa(spo2Value, payload, 10);
    client.publish(SPO2_TOPIC, payload);
  }

  // Publish Pulse if valid
  if (validHeartRate) {
    itoa(heartRate, payload, 10);
    client.publish(PULSE_TOPIC, payload);
  }
}

void reconnectMQTT() {
  while (!client.connected()) {
    if (client.connect(DEVICE_LABEL, mqttUser, mqttPassword)) {
      lcd.setCursor(0, 0);
      lcd.print("MQTT Connected");
      delay(1000);
      lcd.clear();
    } else {
      delay(2000);
    }
  }
}

void setup() {
  initializeLCD();
  connectToWiFi();
  
  wifiClient.setInsecure();
  client.setServer(mqttServer, mqttPort);
  
  initializeMAX30102();
  pinMode(ECG_PIN, INPUT);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  readSensors();
  updateDisplay();

  if (millis() - lastPublishTime >= publishInterval) {
    lastPublishTime = millis();
    publishData();
  }

  delay(50);
}
