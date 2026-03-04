#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

WebServer server(80);
Preferences preferences;

const char* apSSID = "ESP32_Setup";
const char* apPassword = "12345678";

String ssid = "";
String password = "";

void startAPMode() {
  WiFi.softAP(apSSID, apPassword);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
}

void handleRoot() {
  String html = "<html><body>";
  html += "<h2>WiFi Setup</h2>";
  html += "<form action='/save'>";
  html += "SSID: <input name='ssid'><br>";
  html += "Password: <input name='pass' type='password'><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form></body></html>";

  server.send(200, "text/html", html);
}

void handleSave() {
  ssid = server.arg("ssid");
  password = server.arg("pass");

  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  preferences.end();

  server.send(200, "text/html", "Saved! Rebooting...");

  delay(2000);
  ESP.restart();
}

bool connectToWiFi() {
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("pass", "");
  preferences.end();

  if (ssid == "") return false;

  WiFi.begin(ssid.c_str(), password.c_str());

  Serial.println("Connecting to WiFi...");

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    Serial.println(WiFi.localIP());
    return true;
  }

  return false;
}

void setup() {
  Serial.begin(115200);

  if (!connectToWiFi()) {
    Serial.println("Starting AP mode...");
    startAPMode();

    server.on("/", handleRoot);
    server.on("/save", handleSave);

    server.begin();
  }
}

void loop() {
  server.handleClient();
}