/************** 
 * Include Libraries 
 **************/ 
#include <WiFi.h> 
#include <PubSubClient.h> 
#include "DHT.h" 

/************** 
 * Define Constants 
 **************/ 
#define WIFISSID "Luis's iPhone" // WIFI SSID 
#define PASSWORD "BobMataPlagas" // WIFI Password 
#define TOKEN "BBUS-TOMKNN3Ct9luzwVCdhALukvf9wHQyD" // Ubidots TOKEN 
#define MQTT_CLIENT_NAME "proyecto-iot" // Unique MQTT client name 
#define VARIABLE_LABEL_temp "temperatura" 
#define VARIABLE_LABEL_hum "humedad" 
#define VARIABLE_LABEL_dist_in "distancia_entrada" 
#define VARIABLE_LABEL_dist_out "distancia_salida" 
#define VARIABLE_LABEL_ldr "luz" 
#define VARIABLE_LABEL_count "contador" 
#define DEVICE_LABEL "proyecto-iotO2024" 

#define pin1 15 // DHT11 pin
DHT dht1(pin1, DHT11);

// Ultrasonic sensor 1 (entry)
#define TRIG_PIN_1 4
#define ECHO_PIN_1 5

// Ultrasonic sensor 2 (exit)
#define TRIG_PIN_2 18
#define ECHO_PIN_2 19

#define LDR_PIN 34 // LDR sensor pin

char mqttBroker[] = "industrial.api.ubidots.com"; 
char payload[200]; 
char topic[150]; 

// Global variables
WiFiClient ubidots; 
PubSubClient client(ubidots); 
int counter = 0;  
bool sensorsActive = true; 

/************** 
 * Auxiliary Functions 
 **************/ 
void callback(char* topic, byte* payload, unsigned int length) { 
  char p[length + 1]; 
  memcpy(p, payload, length); 
  p[length] = NULL; 
  String message(p); 
  Serial.write(payload, length); 
  Serial.println(topic); 
} 

void reconnect() { 
  while (!client.connected()) { 
    Serial.println("Attempting MQTT connection..."); 
    if (client.connect(MQTT_CLIENT_NAME, TOKEN, "")) { 
      Serial.println("Connected"); 
    } else { 
      Serial.print("Failed, rc="); 
      Serial.print(client.state()); 
      Serial.println(" try again in 2 seconds"); 
      delay(2000); 
    } 
  } 
} 

float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000); // Timeout: 30 ms
  if (duration == 0) {
    return -1; // Return -1 if no echo is detected
  }

  float distance = (duration * 0.0343) / 2; // Calculate distance in cm
  return distance;
}

int getLDRValue() {
  int ldrValue = analogRead(LDR_PIN);
  ldrValue = map(ldrValue, 0, 4095, 0, 1000); // Map to 0-1000 range
  ldrValue -= 100; // Subtract 100 for adjustment
  if (ldrValue < 0) {
    ldrValue = 0; // Prevent negative values
  }
  return ldrValue;
}

/************** 
 * Main Functions 
 **************/ 
void setup() { 
  Serial.begin(115200); 
  WiFi.begin(WIFISSID, PASSWORD); 

  Serial.println(); 
  Serial.print("Wait for WiFi..."); 

  while (WiFi.status() != WL_CONNECTED) { 
    Serial.print("."); 
    delay(500); 
  } 

  Serial.println(""); 
  Serial.println("WiFi Connected"); 
  Serial.println("IP address: "); 
  Serial.println(WiFi.localIP()); 
  client.setServer(mqttBroker, 1883); 
  client.setCallback(callback);   

  // Sensor initialization
  dht1.begin(); 
  pinMode(TRIG_PIN_1, OUTPUT);
  pinMode(ECHO_PIN_1, INPUT);
  pinMode(TRIG_PIN_2, OUTPUT);
  pinMode(ECHO_PIN_2, INPUT);
  pinMode(LDR_PIN, INPUT);
} 

void loop() { 
  if (!client.connected()) { 
    reconnect(); 
  } 

  Serial.println("Sensor data:");

  // Ultrasonic sensors
  float distance1 = getDistance(TRIG_PIN_1, ECHO_PIN_1);
  float distance2 = getDistance(TRIG_PIN_2, ECHO_PIN_2);

  if (distance1 > 0 && distance1 < 400) {
    Serial.print("Entry sensor distance: "); Serial.println(distance1);
    if (distance1 < 100) {
      counter++;
      Serial.println("Entry detected, incrementing counter.");
    }
  } else {
    Serial.println("Error reading entry sensor.");
  }

  if (distance2 > 0 && distance2 < 400) {
    Serial.print("Exit sensor distance: "); Serial.println(distance2);
    if (distance2 < 100) {
      counter--;
      Serial.println("Exit detected, decrementing counter.");
    }
  } else {
    Serial.println("Error reading exit sensor.");
  }

  // Counter logic
  if (counter <= 0) {
    counter = 0; 
    if (sensorsActive) {
      sensorsActive = false; 
      Serial.println("Counter reached 0. Sensors turned off.");
    }
  } else if (!sensorsActive) {
    sensorsActive = true; 
    Serial.println("Counter > 0. Sensors turned on.");
  }

  // Sensor readings and publishing if active
  if (sensorsActive) {
    // Temperature and humidity
    float t1 = dht1.readTemperature();
    float h1 = dht1.readHumidity();
    Serial.print("Temperature: "); Serial.println(t1);
    Serial.print("Humidity: "); Serial.println(h1);

    sprintf(topic, "%s%s", "/v1.6/devices/", DEVICE_LABEL); 
    sprintf(payload, "{\"%s\": {\"value\": %.2f}, \"%s\": {\"value\": %.2f}}", VARIABLE_LABEL_temp, t1, VARIABLE_LABEL_hum, h1);
    client.publish(topic, payload);

    // LDR sensor
    int ldrValue = getLDRValue();
    Serial.print("Light detected (adjusted): "); Serial.println(ldrValue);

    sprintf(topic, "%s%s", "/v1.6/devices/", DEVICE_LABEL); 
    sprintf(payload, "{\"%s\": {\"value\": %d}}", VARIABLE_LABEL_ldr, ldrValue);
    client.publish(topic, payload);
  }

  // Counter publishing
  sprintf(topic, "%s%s", "/v1.6/devices/", DEVICE_LABEL); 
  sprintf(payload, "{\"%s\": {\"value\": %d}}", VARIABLE_LABEL_count, counter);
  client.publish(topic, payload);

  client.loop();
  
  Serial.println("Waiting 12 seconds...");
  delay(12000); 
}

