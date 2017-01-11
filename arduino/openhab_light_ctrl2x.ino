
/**
 * Two light ON/OFF drivers with PCA9536.
*/
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <PCA9536.h>

// GPIO0 and GPIO2 used for I2C bus
#define SDA_PIN 0
#define SCL_PIN 2

// Local base URL of OpenHAB (can be changed later on config page)
#define DEFAULT_OPENHAB_HOST "192.168.128.100"
#define DEFAULT_OPENHAB_PORT 9000

// Name of OpenHAB item (can be changed later on config page)
#define DEFAULT_OPENHAB_ITEM1 "kitchenChandelier"
#define DEFAULT_OPENHAB_ITEM2 "WC"

// Update time for item state in seconds;
// switch periodically sends its state to OpenHAB
#define ITEM_UPDATE_TIME 60L

// Local network definition; must have static IP address
#define LOCAL_IP IPAddress(192, 168, 128, 201)
#define GATEWAY IPAddress(192, 168, 128, 1)
#define SUBNETMASK IPAddress(255, 255, 255, 0)

// Local Wifi AP for connect
#define DEFAULT_WIFI_AP "****"
#define DEFAULT_WIFI_PASSWORD "****"

// Error count for reconnect WIFI
#define ERR_COUNT_FOR_RECONNECT 30
#define HTTP_READ_TIMEOUT       10000
#define HTTP_CONNECT_TIMEOUT    5000

// Local WEB server port
#define WEB_SERVER_PORT 80

// Magic for detecting empty (unconfigured) EEPROM
#define MAGIC 0xAC

// Configuration server
ESP8266WebServer *server = (ESP8266WebServer *) NULL;

// Last interaction time
long lastInteractionTime;

// Number of communication errors
int errorCount = 0;

/** Current light states. */
bool lightState [2];

/**
 * Struktura ukladana do EEPROM.
*/
struct OhConfiguration {
  int magic;
  char apName [24];
  char password [48];
  char openhabItem1 [50];
  char openhabItem2 [50];
  char openhabHost [50];
  unsigned int openhabPort;
};

// Configuration
OhConfiguration config;

// PCA9536 communication object
PCA9536 pca;

/**
 * Setup function.
*/
void setup() {
  // serial
  Serial.begin (115200);
  delay (5000);
  Serial.println ("setup()");
  
  Wire.begin (SDA_PIN, SCL_PIN);

  lightState [0] = false;
  lightState [1] = false;

  pcaSetup();

  // Read config
  EEPROM.begin (sizeof (OhConfiguration));
  EEPROM.get (0, config);
  if (config.magic != MAGIC) {
    memset (&config, 0, sizeof (OhConfiguration));
    config.magic = MAGIC;
    updateConfigKey (config.apName, 24, String(DEFAULT_WIFI_AP));
    updateConfigKey (config.password, 48, String(DEFAULT_WIFI_PASSWORD));
    updateConfigKey (config.openhabItem1, 50, String(DEFAULT_OPENHAB_ITEM1));
    updateConfigKey (config.openhabItem2, 50, String(DEFAULT_OPENHAB_ITEM2));
    updateConfigKey (config.openhabHost, 50, String(DEFAULT_OPENHAB_HOST));
    config.openhabPort = DEFAULT_OPENHAB_PORT;
  }

  reconnectWifi();
  createServer();

  delay (1000);
}

/**
 * Configure PCA.
*/
void pcaSetup() {
  Serial.print ("PCA9536 reset ");
  pca.reset();
  pca.setMode (IO0, IO_INPUT);
  pca.setMode (IO1, IO_INPUT);
  pca.setMode (IO2, IO_OUTPUT);
  pca.setMode (IO3, IO_OUTPUT);
  pca.setState (IO2, lightState[0] ? IO_HIGH : IO_LOW);
  pca.setState (IO3, lightState[1] ? IO_HIGH : IO_LOW);

  while (pca.ping()) {
    Serial.println("Not Found");
    delay (100);
  }
  Serial.println("OK");
}

/**
 * Helper routine; updates config key.
 *
 * @param c key.
 * @param len max length.
 * @param val value.
*/
void updateConfigKey (char *c, int len, String val) {
  memset (c, 0, len);
  sprintf (c, "%s", val.c_str());
}

/**
 * Reconects WIFI.
*/
void reconnectWifi() {
  Serial.println ("WiFi disconnected...");
  WiFi.disconnect();
  String hostname = String("openhab_") + String(config.openhabItem1) + String("_") + String(config.openhabItem2);
  WiFi.hostname (hostname);
  WiFi.config (LOCAL_IP, GATEWAY, SUBNETMASK);
  WiFi.mode (WIFI_STA);
  delay (1000);

  Serial.print ("Connecting to "); Serial.print (WIFI_AP); Serial.print (' ');
  WiFi.begin (config.apName, config.password);
  delay (1000);
  while (WiFi.status() != WL_CONNECTED) {
    yield();
    delay (500);
    Serial.print (".");
  }
  delay (500);
  Serial.println();
  Serial.println ("WiFi connected...");
}

/**
 * Creates HTTP server.
*/
void createServer() {
  if (server) {
    server->close();
    delete server;
    server = NULL;
  }
  
  // ... and run HTTP server for setup
  server = new ESP8266WebServer (LOCAL_IP, WEB_SERVER_PORT);
  server->on ("/OFF/1", wsLightOff1);
  server->on ("/ON/1", wsLightOn1);
  server->on ("/OFF/2", wsLightOff2);
  server->on ("/ON/2", wsLightOn2);
  server->on ("/", wsConfig);
  server->on ("/update", wsUpdate);
  server->on ("/reconnect", wsReconnect);
  server->begin();

  Serial.println ("Created server...");
}

/**
 * Physically turn light on.
 * 
 * @param index index.
*/
void lightOn (byte index) {
  yield();
  lightState [index] = true;
  pca.setState (index == 0 ? IO2 : IO3, IO_HIGH);
}

/**
 * Physisally turn light off.
 * 
 * @param index index.
*/
void lightOff (byte index) {
  yield();
  lightState [index] = false;
  pca.setState (index == 0 ? IO2 : IO3, IO_LOW);

  if (!lightState[0] && !lightState[1]) {
    pcaSetup();
  }
}

/**
 * Light OFF for WEB server.
*/
void wsLightOff1() {
  Serial.println ("Received command OFF for light1...");
  lightOff(0);
  server->send (200, "text/html", "");
}

/**
 * Light ON for WEB server.
*/
void wsLightOn1() {
  Serial.println ("Received command ON for light1...");
  lightOn(0);
  server->send (200, "text/html", "");
}

/**
 * Light OFF for WEB server.
*/
void wsLightOff2() {
  Serial.println ("Received command OFF for light2...");
  lightOff(1);
  server->send (200, "text/html", "");
}

/**
 * Light ON for WEB server.
*/
void wsLightOn2() {
  Serial.println ("Received command ON for light2...");
  lightOn(1);
  server->send (200, "text/html", "");
}

/**
 * Returns switch state.
 * 
 * @param index index.
 * @return bool state.
*/
bool getSwitchState (byte index) {
  return pca.getState ((index == 0) ? IO0 : IO1);
}

/**
 * Configuration page.
*/
void wsConfig() {
  yield();

  String resp = "<html><head><title>OpenHAB double switch configuration</title>";
  resp += "<meta name=\"viewport\" content=\"initial-scale=1.0, width = device-width, user-scalable = no\">";
  resp += "</head><body>";
  resp += "<h1>OpenHAB switch configuration</h1>";
  resp += "<form method=\"post\" action=\"/update\" id=\"form\">";
  resp += "<table border=\"0\" cellspacing=\"0\" cellpadding=\"5\">";
  resp += "<tr><td>AP SSID:</td><td><input type=\"text\" name=\"apName\" value=\"" + String(config.apName) + "\" maxlength=\"24\"></td><td></td></tr>";
  resp += "<tr><td>AP Password:</td><td><input type=\"password\" name=\"password\" value=\"" + String(config.password) + "\" maxlength=\"48\"></td><td></td></tr>";
  resp += "<tr><td>OpenHAB item 1:</td><td><input type=\"text\" name=\"openhabItem1\" value=\"" + String(config.openhabItem1) + "\" maxlength=\"50\"></td><td></td></tr>";
  resp += "<tr><td>OpenHAB item 2:</td><td><input type=\"text\" name=\"openhabItem2\" value=\"" + String(config.openhabItem2) + "\" maxlength=\"50\"></td><td></td></tr>";
  resp += "<tr><td>OpenHAB IP address:</td><td><input type=\"text\" name=\"openhabHost\" value=\"" + String(config.openhabHost) + "\" maxlength=\"50\"></td><td></td></tr>";
  resp += "<tr><td>OpenHAB port:</td><td><input type=\"text\" name=\"openhabPort\" value=\"" + String(config.openhabPort) + "\"></td><td></td></tr>";

  resp += "<tr><td colspan=\"3\" align=\"center\"><input type=\"submit\" value=\"Save\"></td></tr>";
  resp += "</table></form>";
  resp += "<p><a href=\"/reconnect\">Reconnect WiFi...</a></p>";
  resp += "</body></html>";

  server->send (200, "text/html", resp);
}

/**
 * Reconnects WiFi with new parameters.
*/
void wsReconnect() {
  yield();
  String resp = "<script>window.alert ('Reconnecting WiFi...'); window.location.replace ('/');</script>";
  server->send (200, "text/html", resp);
  reconnectWifi();
  createServer();
}

/**
 * Saves configuration.
*/
void wsUpdate() {
  yield();

  String apName = server->arg ("apName");
  String password = server->arg ("password");
  String openhabItem1 = server->arg ("openhabItem1");
  String openhabItem2 = server->arg ("openhabItem2");
  String openhabHost = server->arg ("openhabHost");
  unsigned int openhabPort = atoi (server->arg ("openhabPort").c_str());
  
  if (apName.length() > 1) {
    updateConfigKey (config.apName, 24, apName);
    updateConfigKey (config.password, 48, password);
    updateConfigKey (config.openhabItem1, 50, openhabItem1);
    updateConfigKey (config.openhabItem2, 50, openhabItem2);
    updateConfigKey (config.openhabHost, 50, openhabHost);
    config.openhabPort = openhabPort;
  
    // store configuration
    EEPROM.begin (sizeof (OhConfiguration));
    EEPROM.put (0, config);
    EEPROM.end();
  
    String resp = "<script>window.alert ('Configuration updated...'); window.location.replace ('/');</script>";
    server->send (200, "text/html", resp);
  } else {
    server->send (200, "text/html", "");
  }
}

/**
 * Calls URL and reads response.
 * 
 * @param url url.
 * @return String response.
*/
String communicate (String url) {
  WiFiClient client;

  // connect to OpenHAB
  Serial.print ("Connecting to "); Serial.print (config.openhabHost); Serial.print (':'); Serial.print (config.openhabPort); Serial.println ("...");
  if (client.connect (config.openhabHost, config.openhabPort)) {
    // send request
    String req = String("GET ") + url + String (" HTTP/1.1\r\n")
      + String("Host: ") + String (config.openhabHost) + String ("\r\nConnection: close\r\n\r\n");
    client.print (req);
    Serial.print (req);

    bool isError = false;
    
    // wait HTTP_CONNECT_TIMEOUT for response
    unsigned long connectStartTime = millis();
    while (client.available() == 0) {
      if (millis() - connectStartTime > HTTP_CONNECT_TIMEOUT) {
        errorCount++;
        isError = true;
        break;
      }

      yield();
    }

    if (!isError) {
      Serial.print ("Reading response -> ");
      
      // read response lines
      unsigned long readStartTime = millis();

      int ch;
      String resp = "";
      while ((ch = client.read()) != -1) {
        resp += (char)ch;
        
        if (millis() - readStartTime > HTTP_READ_TIMEOUT) {
          errorCount++;
          isError = true;
          break;
        }

        yield();
      }

      Serial.println ("OK");
      
      client.stop();
      return resp;
    } else {
      Serial.println ("ERROR");
    }
  }
  client.stop();
  return String("");
}

/**
 * Returns item name for index.
 * 
 * @param index index.
 * @return OpenHAB item name.
*/
String getOpenhabItem (byte index) {
  return (index == 0) ? String(config.openhabItem1) : String(config.openhabItem2);
}

/**
 * Updates light state on server.
 * 
 * @param lightIndex index.
*/
void updateLightState(byte lightIndex) {
  communicate (String("/CMD?") + getOpenhabItem(lightIndex) + String("=") + (lightState[lightIndex] ? String("ON") : String("OFF")));
}

/**
 * Turns light on/off.
*/
void toggleState (byte lightIndex) {
  Serial.print ("Changing state...");
  lightState[lightIndex] = !lightState[lightIndex];
  if (lightState[lightIndex]) {
    lightOn(lightIndex);
  } else {
    lightOff(lightIndex);
  }
  updateLightState (lightIndex);
}

/**
 * Loop function.
*/
void loop() {
  byte lightIndex;
  lastInteractionTime = 0L;
  bool lastSwitchStates [2];

  for (lightIndex = 0; lightIndex < 2; lightIndex++) {
    lastSwitchStates [lightIndex] = getSwitchState (lightIndex);
    updateLightState (lightIndex);
  }
  
  while (true) {
    if (server) {
      server->handleClient();
    }


    bool doUpdate = (millis() - lastInteractionTime > ITEM_UPDATE_TIME * 1000L);
    
    for (lightIndex = 0; lightIndex < 2; lightIndex++) {
      bool switchState = getSwitchState (lightIndex);
      if (switchState != lastSwitchStates[lightIndex]) {
        toggleState (lightIndex);
        lastInteractionTime = millis();
        delay (200);
        lastSwitchStates[lightIndex] = getSwitchState (lightIndex);
      }
      
      if (doUpdate) {
        if (WiFi.status() == WL_CONNECTED) {
          String resp = communicate (String("/rest/items/") + String(getOpenhabItem (lightIndex)) + String("/state"));
              
          if (resp.endsWith ("ON")) {
            lightOn (lightIndex);
            errorCount = 0;
          } else if (resp.endsWith ("OFF")) {
            lightOff (lightIndex);
            errorCount = 0;
          } else {
            Serial.println ("Unknown value:");
            Serial.println (resp);
            Serial.println ("> updating state...");
            updateLightState (lightIndex);
          }
        } else {
          Serial.println ("Wifi not connected!");
          errorCount++;
        }
        
        // when error count reaches ERR_COUNT_FOR_RECONNECT, reconnect WiFi.
        if (errorCount > ERR_COUNT_FOR_RECONNECT) {
          errorCount = 0;
          reconnectWifi();
          createServer();
        }
  
        lastInteractionTime = millis();
      }
    }
   
    yield();
  }
}

