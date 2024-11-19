/************** 
 * Include Libraries 
 **************/ 
#include <WiFi.h> 
#include <PubSubClient.h> 
#include "DHT.h" 

/************** 
 * Define Constants 
 **************/ 
#define WIFISSID "IoT"          // Nombre de la red WiFi
#define PASSWORD "1t3s0IoT23"   // Contraseña de la red WiFi
#define TOKEN "token" // Ubidots TOKEN  
#define MQTT_CLIENT_NAME "proyecto-iot" // ID único para el cliente MQTT  
#define VARIABLE_LABEL_temp "temperatura" // Variable Temperatura 
#define VARIABLE_LABEL_hum "humedad"      // Variable Humedad 
#define VARIABLE_LABEL_dist_in "distancia_entrada" // Distancia sensor entrada
#define VARIABLE_LABEL_dist_out "distancia_salida" // Distancia sensor salida
#define VARIABLE_LABEL_ldr "luz"          // Variable Luz 
#define VARIABLE_LABEL_count "contador"   // Variable Contador
#define DEVICE_LABEL "esp32-iot"          // Nombre del dispositivo en Ubidots

#define pin1 15 // Pin del sensor DHT11
DHT dht1(pin1, DHT11); // Sensor de temperatura y humedad 

// Pines del primer sensor ultrasónico (entrada)
#define TRIG_PIN_1 4
#define ECHO_PIN_1 5

// Pines del segundo sensor ultrasónico (salida)
#define TRIG_PIN_2 18
#define ECHO_PIN_2 19

#define LDR_PIN 34 // Pin para el sensor LDR

char mqttBroker[] = "industrial.api.ubidots.com"; 
char payload[200]; 
char topic[150]; 

// Variables globales
WiFiClient ubidots; 
PubSubClient client(ubidots); 
int counter = 0;  // Contador para el flujo
bool sensorsActive = true; // Bandera para activar/desactivar sensores

/************** 
 * Funciones auxiliares 
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

  long duration = pulseIn(echoPin, HIGH);
  float distance = (duration * 0.0343) / 2; // Calcula la distancia en cm
  return distance;
}

int getLDRValue() {
  int ldrValue = analogRead(LDR_PIN);
  return ldrValue;
}

/************** 
 * Configuración WiFi 
 **************/ 
void connectToWiFi() {
  WiFi.mode(WIFI_STA); // Configura el WiFi en modo estación
  WiFi.begin(WIFISSID, PASSWORD);

  Serial.println("\nConectando al WiFi...");
  while (WiFi.status() != WL_CONNECTED) { 
    Serial.print("."); 
    delay(500); 
  } 

  Serial.println("\nWiFi conectado.");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());
}

/************** 
 * Main Functions 
 **************/ 
void setup() { 
  Serial.begin(115200); 
  connectToWiFi();

  client.setServer(mqttBroker, 1883); 
  client.setCallback(callback);   

  // Configuración de sensores
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

  Serial.println("Leyendo datos de sensores...");

  // Publicar distancia en ambos sensores ultrasónicos
  float distance1 = getDistance(TRIG_PIN_1, ECHO_PIN_1);
  float distance2 = getDistance(TRIG_PIN_2, ECHO_PIN_2);
  Serial.print("Distancia sensor entrada: "); Serial.println(distance1);
  Serial.print("Distancia sensor salida: "); Serial.println(distance2);

  // Lógica para el contador de flujo
  if (distance1 < 100) { 
    counter++;
    Serial.println("Entrada detectada, incrementando contador.");
  } 
  if (distance2 < 100) { 
    counter--;
    Serial.println("Salida detectada, decrementando contador.");
  }

  // Evita que el contador sea negativo
  counter = max(counter, 0);

  // Publicar datos a Ubidots
  if (sensorsActive) {
    float t1 = dht1.readTemperature();
    float h1 = dht1.readHumidity();
    int ldrValue = getLDRValue();

    Serial.print("Temperatura: "); Serial.println(t1);
    Serial.print("Humedad: "); Serial.println(h1);
    Serial.print("Luz detectada: "); Serial.println(ldrValue);

    // Publica temperatura y humedad
    sprintf(topic, "%s%s", "/v1.6/devices/", DEVICE_LABEL); 
    sprintf(payload, "{\"%s\": {\"value\": %.2f}, \"%s\": {\"value\": %.2f}}", VARIABLE_LABEL_temp, t1, VARIABLE_LABEL_hum, h1);
    client.publish(topic, payload);

    // Publica luz
    sprintf(payload, "{\"%s\": {\"value\": %d}}", VARIABLE_LABEL_ldr, ldrValue);
    client.publish(topic, payload);
  }

  // Publicar el contador
  sprintf(payload, "{\"%s\": {\"value\": %d}}", VARIABLE_LABEL_count, counter);
  client.publish(topic, payload);

  client.loop();

  delay(12000); // Espera 12 segundos antes de la próxima lectura
}
