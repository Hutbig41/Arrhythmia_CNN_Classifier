#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <WiFiClientSecure.h>

#define WIFISSID "Mywifi"
#define PASSWORD "12345677"

/****************************************
 * Define Constants
 ****************************************/
#define ECG_TOPIC "ecg_data"              // ECG topic
#define SPO2_TOPIC "spo2_data"            // SpO2 topic
#define PULSE_TOPIC "pulse_data"          // Pulse rate topic
#define DEVICE_LABEL "esp8266"            // Device label

#define SENSOR A0                         // ECG analog pin
#define MAX30102_I2C_ADDRESS 0x57         // MAX30102 I2C address

// HiveMQ Cloud settings
const char* mqttServer = "pinktawny-f8qxyf.a01.euc1.aws.hivemq.cloud";
const int mqttPort = 8883;
const char* mqttUser = "hivemq.webclient.1742745732084";
const char* mqttPassword = ".8e9TrLh$C@0lA,7nJEj";

// Space to store values to send
char str_ecg[10];
char str_spo2[10];
char str_pulse[10];

// MAX30102 variables
MAX30105 particleSensor;
#define MAX_BRIGHTNESS 255

uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data

int32_t bufferLength; //data length
int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

// Timing variables
unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 1000; // 1 second between publishes
unsigned long sensorReadTime = 0;
const unsigned long sensorReadInterval = 4000; // Read MAX30102 every 4 seconds

/****************************************
 * Auxiliar Functions
 ****************************************/
WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

void callback(char* topic, byte* payload, unsigned int length) {
  char p[length + 1];
  memcpy(p, payload, length);
  p[length] = NULL;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(p);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection to HiveMQ Cloud...");
    
    // Attempt to connect
    if (client.connect(DEVICE_LABEL, mqttUser, mqttPassword)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("status", "ESP8266 connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(500);
    }
  }
}

void readMAX30102() {
  bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps

  //read the first 100 samples, and determine the signal range
  for (byte i = 0 ; i < bufferLength ; i++) {
    while (particleSensor.available() == false) //do we have new data?
      particleSensor.check(); //Check the sensor for new data

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample(); //We're finished with this sample so move to next sample
  }

  //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
}

/****************************************
 * Main Functions
 ****************************************/
void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFISSID, PASSWORD);
  // Assign the pin as INPUT 
  pinMode(SENSOR, INPUT);

  Serial.println();
  Serial.print("Waiting for WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  Serial.println("");
  Serial.println("WiFi Connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Configure WiFiClientSecure for SSL
  wifiClient.setInsecure(); // Use this for testing only, for production use proper certificates
  
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  // Initialize MAX30102 sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 was not found. Please check wiring/power.");
    while (1);
  }

  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();

  // Read ECG value every loop (fast reading)
  float myecg = analogRead(SENSOR); 
  dtostrf(myecg, 4, 2, str_ecg);

  // Read MAX30102 sensor data less frequently (every 4 seconds)
  if (currentMillis - sensorReadTime >= sensorReadInterval) {
    sensorReadTime = currentMillis;
    readMAX30102();
    
    // Only use valid readings
    if (validHeartRate && validSPO2) {
      dtostrf(spo2, 4, 2, str_spo2);
      dtostrf(heartRate, 4, 2, str_pulse);
    } else {
      // If readings are invalid, send previous values or zeros
      strcpy(str_spo2, "0");
      strcpy(str_pulse, "0");
    }
  }

  // Publish data at regular intervals (1 second)
  if (currentMillis - lastPublishTime >= publishInterval) {
    lastPublishTime = currentMillis;

    // Publish ECG data (fast changing)
    if (client.publish(ECG_TOPIC, str_ecg)) {
      Serial.print("Published ECG: ");
      Serial.println(str_ecg);
    } else {
      Serial.println("ECG publish failed");
    }

    // Publish SpO2 and Pulse data (slower changing)
    if (currentMillis - sensorReadTime < publishInterval) {
      if (client.publish(SPO2_TOPIC, str_spo2)) {
        Serial.print("Published SpO2: ");
        Serial.println(str_spo2);
      } else {
        Serial.println("SpO2 publish failed");
      }

      if (client.publish(PULSE_TOPIC, str_pulse)) {
        Serial.print("Published Pulse: ");
        Serial.println(str_pulse);
      } else {
        Serial.println("Pulse publish failed");
      }
    }
  }

  // Small delay to prevent watchdog timer issues
  delay(10);
}