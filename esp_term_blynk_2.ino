#define BLYNK_TEMPLATE_ID "TMPL4e9pxd2Xi"
#define BLYNK_TEMPLATE_NAME "term"
#define BLYNK_AUTH_TOKEN "1gE0QqCW4Pdfp3qXDowz1-oOAD_Rb22E"

#define BLYNK_FIRMWARE_VERSION "0.3.2"  //2. kalibracio

#define BLYNK_PRINT Serial



//ESP32
//#include <Update.h>
//#include <HTTPClient.h>

//ESP8266
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <ESP8266Ping.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiClient.h>
#include "DHT.h"
#include <string.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>

//________________________________________________________________
String overTheAirURL = "";

#define WIFI_SSID "Rozsa2"      //Enter Wifi Name
#define WIFI_PASS "Potrete1"  //Enter wifi Password

#define DHTTYPE DHT11
#define DHTPIN 2    // D4
#define RELAYPIN 0  // D2

#define default_heatOn 14
#define default_delta 1
#define default_interval 5


boolean setBlynk = 0;

float humidity, temp_f, humidity_temp, temp_f_temp, temp_mav;  // Values read from sensor
byte temp_shift = 5;
int avgs = 20, avg_i;
float avg_arr[20];  //avgs

int firstreading = 1;

int send = 1;

byte heatOn = default_heatOn;
byte delta = default_delta;
byte heatOn_min = 3;
byte heatOn_max = 30;
byte delta_min = 1;
byte delta_max = 5;
byte shift_min = 0;
byte shift_max = 10;
int hum_min = 20;
int hum_max = 90;
int temp_min = 0;
int temp_max = 50;
int relayState = 0;

long RSSI;
String WifiQuality;

String stateString;

unsigned long previousMillis = 0;  // will store last temp was read
byte interval = default_interval;  // interval at which to read sensor
byte interval_min = 5;
byte interval_max = 255;

int heatOn_addr = 0;
int delta_addr = 1;
int interval_addr = 2;
int shift_addr = 4;

// Initialize DHT sensor

// This is for the ESP8266 processor on ESP-01
DHT dht(DHTPIN, DHTTYPE, 11);  // 11 works fine for ESP8266



// reads the temp and humidity from the DHT sensor and sets the global variables for both
void gettemperature() {

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  humidity_temp = dht.readHumidity();                     // Read humidity (percent)
  temp_f_temp = dht.readTemperature(false) - temp_shift;  // Read temperature as Celsius
  // Check if any reads failed and exit
  if (isnan(humidity_temp) || isnan(temp_f_temp) || humidity_temp < hum_min || humidity_temp > hum_max || temp_f_temp < temp_min || temp_f_temp > temp_max) {
    //Serial.println("Failed to read from DHT sensor!");
    return;
  }

  humidity = humidity_temp;
  temp_f = temp_f_temp;

  if (firstreading) {
    for (int i = 0; i < avgs; i++) {
      avg_arr[i] = temp_f;
    }
    firstreading = 0;
  } else {
    avg_arr[avg_i] = temp_f;
    avg_i++;
    if (avg_i >= avgs) {
      avg_i = 0;
    }
  }

  temp_mav = 0;
  //average
  for (int i = 0; i < avgs; i++) {
    temp_mav += avg_arr[i];
  }
  temp_mav /= avgs;


  // turn the relay switch Off or On depending on the temperature reading
  if (temp_mav <= heatOn) {
    digitalWrite(RELAYPIN, LOW);
    relayState = 1;
  } else if (temp_mav >= heatOn + delta) {
    digitalWrite(RELAYPIN, HIGH);
    relayState = 0;
  }
}


BLYNK_WRITE(V3) {
  send = 1;
  byte heatOn_temp = (byte)param.asInt();
  if (heatOn_temp <= heatOn_max && heatOn_temp >= heatOn_min && heatOn_temp != heatOn) {
    heatOn = heatOn_temp;
    EEPROM.write(heatOn_addr, heatOn);
    if (EEPROM.commit()) {
      Serial.println("EEPROM successfully committed");
    } else {
      Serial.println("ERROR! EEPROM commit failed");
    }
  }
}
BLYNK_WRITE(V2) {
  send = 1;
  byte interval_temp = (byte)param.asInt();
  if (interval_temp <= interval_max && interval_temp >= interval_min && interval_temp != interval) {
    interval = interval_temp;
    EEPROM.write(interval_addr, interval);
    if (EEPROM.commit()) {
      Serial.println("EEPROM successfully committed");
    } else {
      Serial.println("ERROR! EEPROM commit failed");
    }
  }
}

void setup() {
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, HIGH);

  Serial.begin(115200);
  Serial.print("BLYNK_FIRMWARE_VERSION = ");
  Serial.println(BLYNK_FIRMWARE_VERSION);
  checkConnection();
  checkInternet();

  EEPROM.begin(512);
  byte heatOn_temp = EEPROM.read(heatOn_addr);
  byte delta_temp = EEPROM.read(delta_addr);
  byte interval_temp = EEPROM.read(interval_addr);
  byte shift_temp = EEPROM.read(shift_addr);
  if (heatOn_temp >= heatOn_min && heatOn_temp <= heatOn_max) {
    heatOn = heatOn_temp;
  }
  if (delta_temp >= delta_min && delta_temp <= delta_max) {
    delta = delta_temp;
  }
  if (interval_temp >= interval_min && interval_temp <= interval_max) {
    interval = interval_temp;
  }
  if (shift_temp >= shift_min && shift_temp <= shift_max) {
    temp_shift = shift_temp;
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

boolean checkConnection() {
  Serial.print("Check connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int count = 0;
  while (count < 20) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Connected to ");
      Serial.println(WIFI_SSID);
      Serial.print("Web Server IP Address: ");
      Serial.println(WiFi.localIP());
      Serial.println("");
      return true;
    }
    delay(5000);
    Serial.print(".");
    count++;
  }
  Serial.println("Timed out........ hiddenAP = 0;");
  Serial.println("");
  return false;
}


void checkInternet() {
  Serial.println("");
  bool ret = Ping.ping("www.google.com");
  if (ret) {
    if (setBlynk == 0) {
      Serial.println("ket noi NEW BLYNK NEW");
      Blynk.config(BLYNK_AUTH_TOKEN);
      setBlynk = 1;
    }
    if (!Blynk.connected()) {
      Serial.println("Not connected to Blynk server");
      Blynk.connect();  // try to connect to server with default timeout
    } else {
      Serial.println("Connected to Blynk server");
    }
    Serial.println("");
  }
}

void loop() {
  Blynk.run();
  ArduinoOTA.handle();

  if (!Blynk.connected()) {
    int count = 0;
    while (count < 3) {  //5 lan chay 6s Blynk.run la 30s
      Blynk.run();       //thoi gian chay 6s
      if (Blynk.connected()) {
        Serial.println("!Blynk.connected(); Blynk.connected())");
        Serial.println("");
        delay(500);
        return;
      }
      delay(1000);
      Serial.print(".");
      count++;
    }
    Serial.println("!Blynk.connected(); internetState = 0;");
    Serial.println("");
    delay(500);
  }

  // check timer to see if it's time to update the data file with another DHT reading
  unsigned long currentMillis = millis();

  // cast to unsigned long to handle rollover
  if ((unsigned long)(currentMillis - previousMillis) >= interval * 1000 || send) {
    send = 0;
    // save the last time you read the sensor
    previousMillis = currentMillis;

    gettemperature();
    /*
    Serial.print("Temp: ");
    Serial.println(temp_f);
    Serial.print("Humidity: ");
    Serial.println(humidity);
*/
    RSSI = WiFi.RSSI();
    if (RSSI > -70) WifiQuality = "Excellent";
    else if (RSSI < -70 && RSSI > -80) WifiQuality = "Good";
    else if (RSSI < -80 && RSSI > -90) WifiQuality = "Fair";
    else if (RSSI < -90) WifiQuality = "Weak";

    stateString = "T=";
    stateString += (String)(int)temp_f + ", delta=" + (String)delta + ", h=" + (String)(int)humidity + "%, T-=" + temp_shift + ", WiFi=" + WifiQuality;


    Blynk.virtualWrite(V1, (double)temp_mav);
    Blynk.virtualWrite(V2, interval);
    Blynk.virtualWrite(V3, heatOn);
    Blynk.virtualWrite(V4, relayState);
    Blynk.virtualWrite(V5, stateString);
  }
}



BLYNK_CONNECTED() {
  // Request the latest state from the server
  Blynk.syncVirtual(V1);
  Blynk.syncVirtual(V2);
  Blynk.syncVirtual(V3);
  Blynk.syncVirtual(V4);
  Blynk.syncVirtual(V5);
}

BLYNK_WRITE(InternalPinOTA) {
  Serial.println("OTA Started");
  overTheAirURL = param.asString();
  Serial.print("overTheAirURL = ");
  Serial.println(overTheAirURL);


  WiFiClient my_wifi_client;
  HTTPClient http;
  http.begin(my_wifi_client, overTheAirURL);


  t_httpUpdate_return ret = ESPhttpUpdate.update(my_wifi_client, overTheAirURL);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.println("[update] Update failed.");
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[update] Update no Update.");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("[update] Update ok.");  // may not be called since we reboot the ESP
      break;
  }
}