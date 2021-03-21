#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServerSecure.h>
#include <ESP8266WebServer.h>
#include <ESP_WiFiManager.h>
#include <ESP8266mDNS.h> 
#include <ArduinoJson.h>


// Saved SSID and PW for home WiFi
String Router_SSID;
String Router_Pass;

// Global variables used by WifiManager
String ssid = "ESP_" + String(ESP_getChipId(), HEX);
#define ESP_getChipId()   (ESP.getChipId())
#define WIFI_CONNECT_TIMEOUT        30000L
#define WHILE_LOOP_DELAY            1000L
#define WHILE_LOOP_STEPS            (WIFI_CONNECT_TIMEOUT / ( 3 * WHILE_LOOP_DELAY ))


// Relay magic numbers
byte relON[] = {0xA0, 0x01, 0x01, 0xA2};  //Hex command to send to serial for open relay
byte relOFF[] = {0xA0, 0x01, 0x00, 0xA1}; //Hex command to send to serial for close relay


// hard coded web auth creds for extra security
const char* www_username = "";
const char* www_password = "";

// door open state
int state=1;

// TLS certificates, stored in PROGMEM
static const uint8_t x509[] PROGMEM = {
};
static const uint8_t rsakey[] PROGMEM = {
};

// HTTPS server
//ESP8266WebServerSecure server(443);
ESP8266WebServer server(80);


// Expaded function to start up WiFi
// It checks the status, and can run a captive portal if no WiFi found
void WiFiSetup() {
  Serial.println("\n\nStart WiFiSetup");
  
  unsigned long startedAt; // timer variable
  ESP_WiFiManager ESP_wifiManager("Gate ESP"); // WiFi manager
  Router_SSID = ESP_wifiManager.WiFi_SSID();         // esblish storage for creds
  Router_Pass = ESP_wifiManager.WiFi_Pass();
  Serial.println("Stored: SSID = [" + Router_SSID + "]");

  // Adding this line to force the configuration portal. suggest leaving it commented out
  //ESP_wifiManager.startConfigPortal((const char *) ssid.c_str());

  // If no creds in memory, start configuration portal to request some
  if (Router_SSID == "")
  {
    Serial.println("We haven't got any access point credentials, so get them now");
    if (!ESP_wifiManager.startConfigPortal((const char *) ssid.c_str()))
      Serial.println("Not connected to WiFi but continuing anyway.");
    else
      Serial.println("WiFi creds established");
  }

  // Start WiFi connection
  startedAt = millis();
   while ( (WiFi.status() != WL_CONNECTED) && (millis() - startedAt < WIFI_CONNECT_TIMEOUT ) )
  {
    // enable WiFi
    WiFi.mode(WIFI_STA);
    WiFi.persistent (true);
    WiFi.setAutoReconnect(true);
    WiFi.begin(Router_SSID.c_str(), Router_Pass.c_str());
    Serial.print("Connecting to " + String(Router_SSID) + ": ");

    int i = 0;
    while ((!WiFi.status() || WiFi.status() >= WL_DISCONNECTED) && i++ < WHILE_LOOP_STEPS)
    {
      Serial.print(String(WiFi.status()));
     digitalWrite(2, HIGH);

      delay(WHILE_LOOP_DELAY);
    }
    Serial.println();
  }
  Serial.println("WiFi Loop complete after " + String((millis() - startedAt) / 1000) + " seconds");
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("Connected. Local IP: ");
    Serial.println(WiFi.localIP());
    digitalWrite(2, LOW);
  }
  else 
  {
    Serial.println("Not Connected: " + String(ESP_wifiManager.getStatus(WiFi.status())));
    Serial.println("Reboot");
    ESP.restart();
  }

  // Setup mDNS as <ssid>.local
   if (!MDNS.begin(ssid)) {
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started as: " + ssid + ".local");
}

// WiFi maintainenace called form the main loop
// Keeps the DNS name alive and checks WiFi status
// Can force a reconnection and reboot if all else fails
void WiFiCheck() {
  MDNS.update();
  if(WiFi.status() != WL_CONNECTED) { // Only need to do any of this if WiFi is not working
    unsigned long startedAt; // timer variable
    // If WiFi isn't connected, display debug message
    if (WiFi.status() == WL_IDLE_STATUS) {
      Serial.println("WL_IDLE_STATUS");
    }
    else if (WiFi.status() == WL_NO_SSID_AVAIL) {
      Serial.println("WL_NO_SSID_AVAIL");
    }
    else if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("WL_CONNECT_FAILED");
    }
    else if (WiFi.status() == WL_CONNECTION_LOST) {
      Serial.println("WL_CONNECTION_LOST");
    }
    else if (WiFi.status() == WL_DISCONNECTED) {
      Serial.println("WL_DISCONNECTED");
    }
    startedAt = millis(); // spend 30 seconds trying to reconnect
    while (WiFi.status() != WL_CONNECTED && (millis() - startedAt < 30000 )) {
      digitalWrite(2, HIGH);
      Serial.println("Trying to reconnect WiFi");
      delay(1000);
    }
    if (WiFi.status() != WL_CONNECTED) { // if reconnection doesn't work, try a reboot.
      ESP.restart();
    } else {
      digitalWrite(2, LOW); // wifi is back
    }
  }
}


void doRoot() {
  Serial.println("Root Call");
}

// Instigate servet with self signed certs.
// Register a couple of functions to call based on queries
void ServerSetup() {
  Serial.println("Starting Server Setup");
  //server.getServer().setServerKeyAndCert_P(rsakey, sizeof(rsakey), x509, sizeof(x509));

  server.on("/", doRoot);
  

  server.on("/setState", []() {
    if (server.authenticate(www_username, www_password)) {  // add some basic web auth for added security
      server.send(200);
      state = 1-state;                                      // flip state
      Serial.println("Changing Gate State");
      Serial.write(relON, sizeof(relON));                   // magic number to open relay
      Serial.println("Open Relay");
      blink();                                              // feedback
      Serial.write(relOFF, sizeof(relOFF));                 // magic number to close relay
      Serial.println("Close Relay");
    }
  });

  server.on("/status", []() { // overkill function to return status in json rather than plaintext
    Serial.println("Status request");
    size_t capacity = JSON_OBJECT_SIZE(1);
    DynamicJsonDocument doc(capacity);
    doc["currentState"] = state;
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  });

  // Start the server
  server.begin();

}

// blinks the ESP LED for 2 seconds for user feedback
void blink() {
   unsigned long startedAt = millis();
   while (millis() - startedAt < 2000) {
    digitalWrite(2, HIGH);
    delay(100);
    digitalWrite(2, LOW);
    delay(100);
   }
}


void setup() {
  Serial.begin(9600); // set this to 9600 to use the relay, or 115200 for deubgging with serial interface
  while (!Serial) ;
  delay(3000);
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH); // HIGH = LED off. set to low when WiFi is established for feedback
  WiFiSetup();
  ServerSetup();
  Serial.println("setup complete");
}


void loop() {
//WiFiCheck();

  MDNS.update();
  server.handleClient();
}
