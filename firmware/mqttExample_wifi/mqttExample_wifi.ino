/*
  ArduinoMqttClient - WiFi Simple Sender

  This example connects to a MQTT broker and publishes a message to
  a topic once a second.

  The circuit:
  - Arduino MKR 1000, MKR 1010 or Uno WiFi Rev2 board

  This example code is in the public domain.
*/

#include <ArduinoMqttClient.h>
#if defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_AVR_UNO_WIFI_REV2)
  #include <WiFiNINA.h>
#elif defined(ARDUINO_SAMD_MKR1000)
  #include <WiFi101.h>
#elif defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_NICLA_VISION) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_GIGA)
  #include <WiFi.h>
#endif

#include <Wire.h>
#include <Adafruit_AHTX0.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <EEPROM.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

#define SSID_LINE_POS 0
#define IP_LINE_POS 10
#define MQTT_LINE_POS 20
#define INIT_LINE_POS 0
#define TEMP_LINE_POS 30
#define HUMIDITY_LINE_POS 40
#define RESETTING_LINE_POS 10

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_AHTX0 aht;

#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

// To connect with SSL/TLS:
// 1) Change WiFiClient to WiFiSSLClient.
// 2) Change port value from 1883 to 8883.
// 3) Change broker value to a server with a known SSL/TLS root certificate 
//    flashed in the WiFi module.

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[50] = "test.mosquitto.org";
int        port     = 1883;
const char tempTopic[50]  = "aroom/temperature";
const char humidityTopic[50]  = "aroom/humidity";

const long interval = 1000;
unsigned long previousMillis = 0;

bool screen_present = true;
bool wifi_connected = false;
bool mqtt_connected = false;


void(* resetFunc) (void) = 0;

void display_initialising()
{
  if (screen_present)
  {
    display.display();
    delay(500); // Pause for 2 seconds

    display.clearDisplay();
    display.setTextSize(1);             // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);        // Draw white text
    display.setCursor(0,INIT_LINE_POS);             // Start at top-left corner
    display.println(F("Initialising"));
    display.display();
  }
}

void display_network_details(bool wifi_connected, bool mqtt_connected)
{
  if (screen_present)
  {
    display.clearDisplay();
    display.setTextSize(1);                     // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);        // Draw white text
    display.setCursor(0,SSID_LINE_POS);           
    display.print(F("SSID: "));
    display.println(ssid);
    
    if (wifi_connected)
    {  
      display.setCursor(0, IP_LINE_POS);            
      display.print(F("IP: "));
      display.println(WiFi.localIP());
      if (mqtt_connected)
      {
        display.setCursor(0, MQTT_LINE_POS);        
        display.print(F("IP: "));
        display.println(WiFi.localIP());
      }
    }
    else
    {
      display.setCursor(0, IP_LINE_POS);
      display.print(F("Wifi Disconnected"));
      display.println(WiFi.localIP());
    }

    display.display();
  }
}

void display_temp_and_humidity(sensors_event_t temp, sensors_event_t humidity)
{
    display.setCursor(0,TEMP_LINE_POS); 
    display.print(F("Temperature: "));
    display.println(temp.temperature);
    display.setCursor(0,HUMIDITY_LINE_POS);             // Start at top-left corner
    display.print(F("Humidity: "));
    display.println(humidity.relative_humidity);
    display.display();
}

void display_sensor_error()
{
  if (screen_present)
  {
    display.clearDisplay();
    display.setTextSize(1);             // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);        // Draw white text
    display.setCursor(0,INIT_LINE_POS);          
    display.print(F("Sensor Fail"));
    display.setCursor(0,RESETTING_LINE_POS);
    display.print(F("Resetting"));

  }
}

bool connect_to_wifi()
{
    // attempt to connect to WiFi network:
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  unsigned long start = millis();
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(2500);

    if ((millis() - start) > 20000)
    {
      Serial.println("Could not connect to WiFi");
      return false;
    }
  }
  Serial.println("You're connected to the network");
  Serial.println();

  return true;
}

bool connect_to_mqtt()
{
  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  // mqttClient.setId("clientId");

  // You can provide a username and password for authentication
  // mqttClient.setUsernamePassword("username", "password");

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    return false;
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  return true;
}

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);

  EEPROM.begin(512);

  Wire.begin(2, 0);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed, could not start screen"));
    screen_present = false;
  }

  // Show system initialising
  display_initialising();

  wifi_connected = connect_to_wifi();

  mqtt_connected = connect_to_mqtt();

  display_network_details(wifi_connected, mqtt_connected);

  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    display_sensor_error();
    delay(2000);
    resetFunc();
  }
  Serial.println("AHT10 or AHT20 found");
 
}

void loop() {
  // call poll() regularly to allow the library to send MQTT keep alives which
  // avoids being disconnected by the broker
  mqttClient.poll();

  // to avoid having delays in loop, we'll use the strategy from BlinkWithoutDelay
  // see: File -> Examples -> 02.Digital -> BlinkWithoutDelay for more info
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval) {
    // save the last time a message was sent
    previousMillis = currentMillis;
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data

    Serial.print("Sending message to topic: ");
    Serial.print(tempTopic);
    Serial.print(": ");
    Serial.println(temp.temperature);
    Serial.print("Sending message to topic: ");
    Serial.print(humidityTopic);
    Serial.print(": ");
    Serial.println(humidity.relative_humidity);

    // send message, the Print interface can be used to set the message contents
    mqttClient.beginMessage(tempTopic);
    mqttClient.print(temp.temperature);
    mqttClient.endMessage();
    
    mqttClient.beginMessage(humidityTopic);
    mqttClient.print(humidity.relative_humidity);
    mqttClient.endMessage();

    Serial.println();

    display.clearDisplay();

    display.setTextSize(1);             // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);        // Draw white text
    display.setCursor(0,0);             // Start at top-left corner
    display.print(F("SSID: "));
    display.println(ssid);
    display.setCursor(0,10);             // Start at top-left corner
    display.print(F("IP: "));
    display.println(WiFi.localIP());

    display_temp_and_humidity(temp, humidity);

  }
}
