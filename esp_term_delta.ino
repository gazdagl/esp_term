#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include "uMQTTBroker.h"
#include "DHT.h"
#include <string.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>

#define DHTTYPE DHT11
#define DHTPIN 2    // D4
#define RELAYPIN 0  // D2

#define default_heatOn 14
#define default_delta 1
#define default_interval 60

/*
 * Your WiFi config here
 */
char ssid[] = "Gazdag";      // your network SSID (name)
char pass[] = "Domesz2018";  // your network password
bool WiFiAP = false;         // Do yo want the ESP as AP?

float humidity, temp_f, humidity_temp, temp_f_temp;  // Values read from sensor

int send = 1;

byte heatOn = default_heatOn;
byte delta = default_delta;
byte heatOn_min = 3;
byte heatOn_max = 30;
byte delta_min = 1;
byte delta_max = 5;
int hum_min = 20;
int hum_max = 90;
int temp_min = 0;
int temp_max = 50;
String relayState = "OFF";

unsigned long previousMillis = 0;  // will store last temp was read
byte interval = default_interval;  // interval at which to read sensor
byte interval_min = 5;
byte interval_max = 255;

int heatOn_addr = 0;
int delta_addr = 1;
int interval_addr = 2;

// Initialize DHT sensor

// This is for the ESP8266 processor on ESP-01
DHT dht(DHTPIN, DHTTYPE, 11);  // 11 works fine for ESP8266



// reads the temp and humidity from the DHT sensor and sets the global variables for both
void gettemperature() {

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  humidity_temp = dht.readHumidity();        // Read humidity (percent)
  temp_f_temp = dht.readTemperature(false);  // Read temperature as Celsius
  // Check if any reads failed and exit
  if (isnan(humidity_temp) || isnan(temp_f_temp) || humidity_temp < hum_min || humidity_temp > hum_max || temp_f_temp < temp_min || temp_f_temp > temp_max) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  humidity = humidity_temp;
  temp_f = temp_f_temp;


  // turn the relay switch Off or On depending on the temperature reading
  if (temp_f <= heatOn) {
    digitalWrite(RELAYPIN, LOW);
    relayState = "ON";
  } else if (temp_f >= heatOn+delta) {
    digitalWrite(RELAYPIN, HIGH);
    relayState = "OFF";
  }
}


/*
 * Custom broker class with overwritten callback functions
 */
class myMQTTBroker : public uMQTTBroker {
public:
  virtual bool onConnect(IPAddress addr, uint16_t client_count) {
    Serial.println(addr.toString() + " connected, with client count " + client_count);
    return true;
  }

  virtual void onDisconnect(IPAddress addr, String client_id) {
    Serial.println(addr.toString() + " (" + client_id + ") disconnected");
  }

  virtual bool onAuth(String username, String password, String client_id) {
    Serial.println("Username/Password/ClientId: " + username + "/" + password + "/" + client_id);
    return true;
  }

  virtual void onData(String topic, const char *data, uint32_t length) {
    char data_str[length + 1];
    os_memcpy(data_str, data, length);
    data_str[length] = '\0';

    Serial.println("received topic '" + topic + "' with data '" + (String)data_str + "'");
    //printClients();

    if (topic == "phone/heatOn") {
      send = 1;
      int heatOn_temp = atoi(data_str);
      if (heatOn_temp <= heatOn_max && heatOn_temp >= heatOn_min && heatOn_temp != heatOn) {
        heatOn = heatOn_temp;
        EEPROM.write(heatOn_addr, heatOn);
        if (EEPROM.commit()) {
          Serial.println("EEPROM successfully committed");
        } else {
          Serial.println("ERROR! EEPROM commit failed");
        }
      }
      else
        publish("broker/heatOn", "invalid", 0, 1);
    } else if (topic == "phone/delta") {
      send = 1;
      int delta_temp = atoi(data_str);
      if (delta_temp <= delta_max && delta_temp >= delta_min && delta_temp != delta) {
        delta = delta_temp;
        EEPROM.write(delta_addr, delta);
        if (EEPROM.commit()) {
          Serial.println("EEPROM successfully committed");
        } else {
          Serial.println("ERROR! EEPROM commit failed");
        }
      }
      else
        publish("broker/delta", "invalid", 0, 1);
    } else if (topic == "phone/interval") {
      send = 1;
      int interval_temp = atoi(data_str);
      if (interval_temp <= interval_max && interval_temp >= interval_min && interval_temp != interval) {
        interval = interval_temp;
        EEPROM.write(interval_addr, interval);
        if (EEPROM.commit()) {
          Serial.println("EEPROM successfully committed");
        } else {
          Serial.println("ERROR! EEPROM commit failed");
        }
      }
      else
        publish("broker/interval", "invalid", 0, 1);
    } else if (topic == "phone/reset") {
      heatOn = default_heatOn;
      delta = default_delta;
      interval = default_interval;
      EEPROM.write(heatOn_addr, heatOn);
      EEPROM.write(delta_addr, delta);
      EEPROM.write(interval_addr, interval);
      if (EEPROM.commit()) {
        Serial.println("EEPROM successfully committed");
      } else {
        Serial.println("ERROR! EEPROM commit failed");
      }
      Serial.println("Default values restored. Rebooting...");
      delay(5000);
      ESP.restart();
    }
  }

  // Sample for the usage of the client info methods

  virtual void printClients() {
    for (int i = 0; i < getClientCount(); i++) {
      IPAddress addr;
      String client_id;

      getClientAddr(i, addr);
      getClientId(i, client_id);
      Serial.println("Client " + client_id + " on addr: " + addr.toString());
    }
  }
};

myMQTTBroker myBroker;

/*
 * WiFi init stuff
 */
void startWiFiClient() {
  Serial.println("Connecting to " + (String)ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  /*while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }*/

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("");

  Serial.println("WiFi connected");
  Serial.println("IP address: " + WiFi.localIP().toString());
}

void startWiFiAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, pass);
  Serial.println("AP started");
  Serial.println("IP address: " + WiFi.softAPIP().toString());
}

void setup() {
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, HIGH);

  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // Start WiFi
  if (WiFiAP)
    startWiFiAP();
  else
    startWiFiClient();

  // Start the broker
  Serial.println("Starting MQTT broker");
  myBroker.init();

  /*
 * Subscribe to anything on the phone
 */
  myBroker.subscribe("phone/#");
  //myBroker.publish("broker/status", "boot", 0, 1);

  EEPROM.begin(512);
  int heatOn_temp = EEPROM.read(heatOn_addr);
  int delta_temp = EEPROM.read(delta_addr);
  int interval_temp = EEPROM.read(interval_addr);
  if (heatOn_temp >= heatOn_min && heatOn_temp <= heatOn_max) {
    heatOn = heatOn_temp;
  }
  if (delta_temp >= delta_min && delta_temp <= delta_max) {
    delta = delta_temp;
  }
  if (interval_temp >= interval_min && interval_temp <= interval_max) {
    interval = interval_temp;
  }

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}



void loop() {
  ArduinoOTA.handle();

  // check timer to see if it's time to update the data file with another DHT reading
  unsigned long currentMillis = millis();

  // cast to unsigned long to handle rollover
  if ((unsigned long)(currentMillis - previousMillis) >= (long)interval * 1000 || send) {
    send = 0;
    // save the last time you read the sensor
    previousMillis = currentMillis;

    gettemperature();

    Serial.print("Temp: ");
    Serial.println(temp_f);
    Serial.print("Humidity: ");
    Serial.println(humidity);
    Serial.print("Number of clients: ");
    Serial.println(myBroker.getClientCount());

    myBroker.publish("broker/temp", (String)(int)temp_f, 0, 1);
    myBroker.publish("broker/humi", (String)(int)humidity, 0, 1);
    myBroker.publish("broker/heatOn", (String)heatOn, 0, 1);
    myBroker.publish("broker/delta", (String)delta, 0, 1);
    myBroker.publish("broker/relayState", relayState, 0, 1);
    myBroker.publish("broker/wifiStrength", (String)WiFi.RSSI(), 0, 1);
    myBroker.publish("broker/interval", (String)interval, 0, 1);
    myBroker.printClients();

    if (WiFi.status() != WL_CONNECTED) {
      startWiFiClient();
    }
  }
}
