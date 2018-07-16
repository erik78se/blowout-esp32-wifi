/*
 * Connects to blowout-api.lonroth.net to get information from http server.
 * 
 * Toggles opens pins X,Y,Z in some fancy way when that happens.
 * 
 * ESP32 How: 
 *    http://dagrende.blogspot.com/2017/01/how-to-use-doit-esp32-devkit.html
 *    
 *    
 * DOIT ESP32 DevKit V1 Pin layout: 
 *    https://docs.zerynth.com/latest/official/board.zerynth.doit_esp32/docs/index.html
 *
 */

#include <WiFi.h>
#include <Arduino.h>
#include <Ticker.h>
#include <ArduinoJson.h>

#define LED_PIN 2
#define RELAY_A 5
#define RELAY_B 17
#define RELAY_C 16


const char* ssid     = "flame";
const char* password = "blowout2018";
const char* host = "blowout-api.lonroth.net";


Ticker blinker;
// Ticker toggler;
// Ticker changer;
float blinkerPace = 0.1;  //seconds
const float togglePeriod = 5; //seconds
bool relays_disarmed = true; //relays initialized as disarmed.


void change() {
  blinkerPace = 0.5;
}

void blink() {
  digitalWrite(LED_PIN, !digitalRead(LED_PIN));
}

void toggle() {
  static bool isBlinking = false;
  if (isBlinking) {
    blinker.detach();
    isBlinking = false;
  }
  else {
    blinker.attach(blinkerPace, blink);
    isBlinking = true;
  }
  digitalWrite(LED_PIN, LOW);  //make sure LED on on after toggling (pin LOW = led ON)
}


void setup()
{
    Serial.begin(115200);
    
    delay(10);
    
    pinMode(LED_PIN, OUTPUT);
    // Set high imediately. Dont want to open relays at setup.
    pinMode(RELAY_A, OUTPUT); digitalWrite(RELAY_A, HIGH);
    pinMode(RELAY_B, OUTPUT); digitalWrite(RELAY_B, HIGH);  
    pinMode(RELAY_C, OUTPUT); digitalWrite(RELAY_C, HIGH);
    
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("");
    
    blinker.attach(blinkerPace, blink);
}


void loop()
{
    delay(5000);

    Serial.print("connecting to ");
    Serial.println(host);

    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(host, httpPort)) {
        Serial.println("connection failed");
        return;
    }

    // We now create a URI for the request
    String url = "/api/info";
    Serial.print("Requesting URL: ");
    Serial.println(url);

    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }

    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
      Serial.print(F("Unexpected response: "));
      Serial.println(status);
      return;
    }

    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders)) {
      Serial.println(F("Invalid response"));
      return;
    }

    /// Handle JSON reply
    const size_t bufferSize = JSON_OBJECT_SIZE(4) + 80;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject& root = jsonBuffer.parseObject(client);
    
    if (!root.success()) {
      Serial.println(F("Parsing failed!"));
    }
    bool blowout = root["blowout"]; // false
    int blowout_weight = root["blowout_weight"]; // 
    int current_weight = root["current_weight"]; // 
    int increment = root["increment"]; // 100
    
    Serial.println(root["blowout"].as<char*>());
    Serial.println(root["blowout_weight"].as<char*>());
    Serial.println(root["current_weight"].as<char*>());
    Serial.println(root["increment"].as<char*>());

    // Only arm if we see a non blowout state first
    // This avoids an accidental blowout in case of a boot
    // where "blowout = true" from server. It requires a cycle
    // first. Avoiding accidents...
    if ( ! blowout ) {
      
      relays_disarmed = false;
      
    }
    
    // Test if we should activate relays for some time.
    if ( blowout && !relays_disarmed ) {
      digitalWrite(RELAY_A, LOW);
      digitalWrite(RELAY_B, LOW);
      digitalWrite(RELAY_C, LOW);
      delay(10000); // Keep valves up 10 sec before closing them.
      digitalWrite(RELAY_A, HIGH);
      digitalWrite(RELAY_B, HIGH);
      digitalWrite(RELAY_C, HIGH);
      relays_disarmed = true; //disarm_relays after opening relays
      // (value-min)/(max-min) => 1-(current_weight / blowout_weight)
    }
    
    // always play safe - turn off valves
    digitalWrite(RELAY_A, HIGH);
    digitalWrite(RELAY_B, HIGH);
    digitalWrite(RELAY_C, HIGH);
    
    blinkerPace = 1.0 - ((double) current_weight / blowout_weight);
    blinker.detach();
    blinker.attach(blinkerPace, blink);      
    
    Serial.println(blinkerPace);
    Serial.println();
    Serial.println("closing connection");
}

