#include <ESP8266WiFi.h>
#include "uMQTTBroker.h"
#include "DHT.h"
#include <string.h>
#include <EEPROM.h>

#define DHTTYPE DHT11
#define DHTPIN 2    // D4
#define RELAYPIN 0  // D2

/*
 * Your WiFi config here
 */
char ssid[] = "Gazdag";      // your network SSID (name)
char pass[] = "Domesz2018";  // your network password
bool WiFiAP = false;         // Do yo want the ESP as AP?

float humidity, temp_f, humidity_temp, temp_f_temp;  // Values read from sensor

byte heatOn = 18;
byte heatOff = 20;
byte heatOn_min = 0;
byte heatOn_max = 30;
byte heatOff_min = 1;
byte heatOff_max = 31;
int hum_min = 20;
int hum_max = 90;
int temp_min = -10;
int temp_max = 50;
String relayState = "OFF";

unsigned long previousMillis = 0;  // will store last temp was read
long interval = 5000;              // interval at which to read sensor

int heatOn_addr = 0;
int heatOff_addr = 1;

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
  } else if (temp_f >= heatOff) {
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
      int heatOn_temp = atoi(data_str);
      if (heatOn_temp <= heatOn_max && heatOn_temp >= heatOn_min && heatOn_temp < heatOff && heatOn_temp != heatOn) {
        heatOn = heatOn_temp;
        EEPROM.write(heatOn_addr, heatOn);
        if (EEPROM.commit()) {
          Serial.println("EEPROM successfully committed");
        } else {
          Serial.println("ERROR! EEPROM commit failed");
        }
      }
    } else if (topic == "phone/heatOff") {
      int heatOff_temp = atoi(data_str);
      if (heatOff_temp <= heatOff_max && heatOff_temp >= heatOff_min && heatOff_temp > heatOn && heatOff_temp != heatOff) {
        heatOff = heatOff_temp;
        EEPROM.write(heatOff_addr, heatOff);
        if (EEPROM.commit()) {
          Serial.println("EEPROM successfully committed");
        } else {
          Serial.println("ERROR! EEPROM commit failed");
        }
      }
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

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
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

  EEPROM.begin(512);
  int heatOn_temp = EEPROM.read(heatOn_addr);
  int heatOff_temp = EEPROM.read(heatOff_addr);
  if (heatOn_temp >= heatOn_min && heatOn_temp <= heatOn_max) {
    heatOn = heatOn_temp;
  }
  if (heatOff_temp >= heatOff_min && heatOff_temp <= heatOff_max) {
    heatOff = heatOff_temp;
  }
}



void loop() {
  // check timer to see if it's time to update the data file with another DHT reading
  unsigned long currentMillis = millis();

  // cast to unsigned long to handle rollover
  if ((unsigned long)(currentMillis - previousMillis) >= interval) {
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
    myBroker.publish("broker/heatOff", (String)heatOff, 0, 1);
    myBroker.publish("broker/relayState", relayState, 0, 1);
    myBroker.printClients();

    if (WiFi.status() != WL_CONNECTED) {
      startWiFiClient();
    }
  }
}
