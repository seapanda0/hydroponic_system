#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // Make sure to install ArduinoJson via Library Manager

// ─── WiFi Credentials ──────────────────────────────────────────────
const char* ssid = "auto8888";
const char* password = "auto8888";

// ─── MQTT Broker Configuration ─────────────────────────────────────
// From backend/main.js
const char* mqtt_server = "8892a280da7b489e930600e068ffd8a3.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "admin";    // Replace with actual username
const char* mqtt_password = "Admin_1234"; // Replace with actual password

const char* command_topic = "sensors/esp32-001/command";

// ─── Hardware Configuration ────────────────────────────────────────
const int LED_PIN = 2; // Default built-in LED on most ESP32 boards

WiFiClientSecure espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Parse the incoming JSON using ArduinoJson
  // Example payload: {"led": "ON"}
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  // Check if "led" key exists in the JSON payload
  if (doc.containsKey("led")) {
    const char* ledState = doc["led"];
    if (strcmp(ledState, "ON") == 0) {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("Action: LED turned ON");
    } else if (strcmp(ledState, "OFF") == 0) {
      digitalWrite(LED_PIN, LOW);
      Serial.println("Action: LED turned OFF");
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect (using username and password)
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println(" connected!");
      
      // Once connected, subscribe to the command topic
      client.subscribe(command_topic);
      Serial.print("Subscribed to topic: ");
      Serial.println(command_topic);
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize the LED pin as an output
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Start with LED off

  setup_wifi();
  
  // HiveMQ Cloud requires a secure connection. 
  // setInsecure() skips certificate validation for ease of development. 
  // In production, provide the proper CA certificate.
  espClient.setInsecure();
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
