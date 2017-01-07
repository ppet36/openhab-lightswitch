
/**
 * One light ON/OFF driver for OpenHAB.
*/
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

// PIN with relay
#define LIGHT_PIN 0
// PIN with orginal light switch
#define SWITCH_PIN 2

// Local base URL of OpenHAB (can be changed later on config page)
#define DEFAULT_OPENHAB_HOST "192.168.128.100"
#define DEFAULT_OPENHAB_PORT 9000

// Name of OpenHAB item (can be changed later on config page)
#define DEFAULT_OPENHAB_ITEM "bathroomMirror"

// Update time for item state in seconds;
// switch periodically sends its state to OpenHAB
#define ITEM_UPDATE_TIME 60L

// Local network definition; must have static IP address
#define LOCAL_IP IPAddress(192, 168, 128, 200)
#define GATEWAY IPAddress(192, 168, 128, 1)
#define SUBNETMASK IPAddress(255, 255, 255, 0)

// Local Wifi AP for connect
#define DEFAULT_WIFI_AP "******"
#define DEFAULT_WIFI_PASSWORD "******************"

// Error count for reconnect WIFI
#define ERR_COUNT_FOR_RECONNECT 30
#define HTTP_READ_TIMEOUT       10000
#define HTTP_CONNECT_TIMEOUT    5000

// Local WEB server port
#define WEB_SERVER_PORT 80

// Magic for detecting empty (unconfigured) EEPROM
#define MAGIC 0xAA

// Configuration server
ESP8266WebServer *server = (ESP8266WebServer *) NULL;

// Last interaction time
long lastInteractionTime;

// Number of communication errors
int errorCount = 0;

/** Current light state. */
bool lightState = false;



/**
 * Structure saved to EEPROM.
*/
struct OhConfiguration {
  int magic;
  char apName [24];
  char password [48];
  char openhabItem [50];
  char openhabHost [50];
  unsigned int openhabPort;
};

// Configuration
OhConfiguration config;

/**
 * Setup function.
*/
void setup() {
  // serial
  Serial.begin (115200);
  Serial.println ("setup()");
  
  // light is initialy OFF
  pinMode (LIGHT_PIN, OUTPUT);
  pinMode (SWITCH_PIN, INPUT);
  digitalWrite (LIGHT_PIN, LOW);

  // Read config
  EEPROM.begin (sizeof (OhConfiguration));
  EEPROM.get (0, config);
  if (config.magic != MAGIC) {
    memset (&config, 0, sizeof (OhConfiguration));
    config.magic = MAGIC;
    updateConfigKey (config.apName, 24, String(DEFAULT_WIFI_AP));
    updateConfigKey (config.password, 48, String(DEFAULT_WIFI_PASSWORD));
    updateConfigKey (config.openhabItem, 50, String(DEFAULT_OPENHAB_ITEM));
    updateConfigKey (config.openhabHost, 50, String(DEFAULT_OPENHAB_HOST));
    config.openhabPort = DEFAULT_OPENHAB_PORT;
  }

  reconnectWifi();
  createServer();

  delay (1000);
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
  String hostname = String("openhab_") + String(config.openhabItem);
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
  server->on ("/OFF", wsLightOff);
  server->on ("/ON", wsLightOn);
  server->on ("/", wsConfig);
  server->on ("/update", wsUpdate);
  server->on ("/reconnect", wsReconnect);
  server->begin();

  Serial.println ("Created server...");
}

/**
 * Physically turn light on.
*/
void lightOn() {
  yield();
  digitalWrite (LIGHT_PIN, HIGH);
  lightState = true;
  Serial.println ("Light is ON");
}

/**
 * Physisally turn light off.
*/
void lightOff() {
  yield();
  digitalWrite (LIGHT_PIN, LOW);
  lightState = false;
  Serial.println ("Light is OFF");
}

/**
 * Light OFF for WEB server.
*/
void wsLightOff() {
  lightOff();
  server->send (200, "text/html", "");
}

/**
 * Light ON for WEB server.
*/
void wsLightOn() {
  lightOn();
  server->send (200, "text/html", "");
}

/**
 * Configuration page.
*/
void wsConfig() {
  yield();

  String resp = "<html><head><title>OpenHAB switch configuration</title>";
  resp += "<meta name=\"viewport\" content=\"initial-scale=1.0, width = device-width, user-scalable = no\">";
  resp += "</head><body>";
  resp += "<h1>OpenHAB switch configuration</h1>";
  resp += "<form method=\"post\" action=\"/update\" id=\"form\">";
  resp += "<table border=\"0\" cellspacing=\"0\" cellpadding=\"5\">";
  resp += "<tr><td>AP SSID:</td><td><input type=\"text\" name=\"apName\" value=\"" + String(config.apName) + "\" maxlength=\"24\"></td><td></td></tr>";
  resp += "<tr><td>AP Password:</td><td><input type=\"password\" name=\"password\" value=\"" + String(config.password) + "\" maxlength=\"48\"></td><td></td></tr>";
  resp += "<tr><td>OpenHAB item:</td><td><input type=\"text\" name=\"openhabItem\" value=\"" + String(config.openhabItem) + "\" maxlength=\"50\"></td><td></td></tr>";
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
  String openhabItem = server->arg ("openhabItem");
  String openhabHost = server->arg ("openhabHost");
  unsigned int openhabPort = atoi (server->arg ("openhabPort").c_str());
  
  if (apName.length() > 1) {
    updateConfigKey (config.apName, 24, apName);
    updateConfigKey (config.password, 48, password);
    updateConfigKey (config.openhabItem, 50, openhabItem);
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
 * Updates light state on server.
*/
void updateLightState() {
  communicate (String("/CMD?") + String(config.openhabItem) + String("=") + (lightState ? String("ON") : String("OFF")));
}

/**
 * Turns light on/off.
*/
void toggleState() {
  Serial.print ("Changing state...");
  lightState = !lightState;
  if (lightState) {
    lightOn();
  } else {
    lightOff();
  }
  updateLightState();
}

/**
 * Loop function.
*/
void loop() {
  lastInteractionTime = 0L;
  bool lastSwitchState = digitalRead (SWITCH_PIN);

  updateLightState();
  
  while (true) {
    if (server) {
      server->handleClient();
    }

    bool switchState = digitalRead (SWITCH_PIN);
    if (switchState != lastSwitchState) {
      toggleState();
      lastInteractionTime = millis();
      delay (500);
      lastSwitchState = digitalRead (SWITCH_PIN);
    }
    
    if (millis() - lastInteractionTime > ITEM_UPDATE_TIME * 1000L) {
      if (WiFi.status() == WL_CONNECTED) {
        String resp = communicate (String("/rest/items/") + String(config.openhabItem) + String("/state"));
            
        if (resp.endsWith ("ON")) {
          Serial.print (config.openhabItem);
          lightOn();
          errorCount = 0;
        } else if (resp.endsWith ("OFF")) {
          Serial.print (config.openhabItem);
          lightOff();
          errorCount = 0;
        } else {
          Serial.println ("Unknown value:");
          Serial.println (resp);
          Serial.println ("> updating state...");
          updateLightState();
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
   
    yield();
  }
}

