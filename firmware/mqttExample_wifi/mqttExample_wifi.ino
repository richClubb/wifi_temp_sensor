/*
  ArduinoMqttClient - WiFi Simple Sender

  This example connects to a MQTT broker and publishes a message to
  a topic once a second.

  The circuit:
  - Arduino MKR 1000, MKR 1010 or Uno WiFi Rev2 board

  This example code is in the public domain.
*/

#define VERSION "0.4"

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
#define NAME_LINE_POS 10
#define ROOM_LINE_POS 30
#define TEMP_LINE_POS 40
#define HUMIDITY_LINE_POS 50
#define RESETTING_LINE_POS 10

#define EEPROM_SIZE 512

#define EEPROM_SSID_LOC 0
#define EEPROM_SSID_SIZE 100
#define EEPROM_SSID_BUFFER_SIZE_LOC 100
#define EEPROM_SSID_BUFFER_SIZE_SIZE 1

#define EEPROM_PASSWORD_LOC 102
#define EEPROM_PASSWORD_SIZE 100
#define EEPROM_PASSWORD_BUFFER_SIZE_LOC 202
#define EEPROM_PASSWORD_BUFFER_SIZE_SIZE 1

#define EEPROM_ROOM_LOC 204
#define EEPROM_ROOM_SIZE 100
#define EEPROM_ROOM_BUFFER_SIZE_LOC 304
#define EEPROM_ROOM_BUFFER_SIZE_SIZE 1

#define EEPROM_BROKER_LOC 306
#define EEPROM_BROKER_SIZE 100
#define EEPROM_BROKER_BUFFER_SIZE_LOC 406
#define EEPROM_BROKER_BUFFER_SIZE_SIZE 1

#define RECONNECT_INTERVAL_MAX ((long int)60000)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_AHTX0 aht;

#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[100] = SECRET_SSID;    // your network SSID (name)
char pass[100] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

// To connect with SSL/TLS:
// 1) Change WiFiClient to WiFiSSLClient.
// 2) Change port value from 1883 to 8883.
// 3) Change broker value to a server with a known SSL/TLS root certificate 
//    flashed in the WiFi module.

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

#define DEFAULT_ROOM_NAME "aroom"

char room[50] = DEFAULT_ROOM_NAME;
char broker[50] = "192.168.1.91";
int        port     = 1883;
char tempTopic[50]  = "";
char humidityTopic[50]  = "";

const long interval = 3000;
unsigned long previousMillis = 0;

long reconnectInterval = 10000;

unsigned long disconnectedTimeLast = 0;
unsigned long previousDisconnectedMillis = 0;

bool wifi_connected = false;
bool mqtt_connected = false;
bool reconnect_required = false;

void(* resetFunc) (void) = 0;

char eeprom_ssid[EEPROM_SSID_SIZE];
char eeprom_password[EEPROM_PASSWORD_SIZE];
char eeprom_room[EEPROM_ROOM_SIZE];
char eeprom_broker[EEPROM_BROKER_SIZE];

char serial_buffer[512];
uint serial_buffer_pos = 0;

void eeprom_read_block(uint start, uint size, char *buffer)
{
  for(int index; index < size; index++)
  {
    *buffer = EEPROM.read(start+index);
  }
}

void initialise_array(uint size, char * buffer, char value)
{
  char * buffer_ptr = buffer;
  for (int index; index < size; index++)
  {
    *buffer = value;
    buffer++;
  }
}

void display_initialising()
{
  delay(500); // Pause for 2 seconds

  display.clearDisplay();
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,INIT_LINE_POS);             // Start at top-left corner
  display.print(F("Initialising"));
  display.setCursor(0,NAME_LINE_POS);
  display.print("WiFi Temp Sensor V");
  display.print(VERSION);
}

void display_network_details(bool wifi_connected, bool mqtt_connected)
{
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
      display.println("Broker: Connected");
    }
  }
  else
  {
    display.setCursor(0, IP_LINE_POS);
    display.print(F("Wifi Disconnected"));
    display.setCursor(0, MQTT_LINE_POS);        
    display.println("Broker: Disconnected");
  }

  display.display();
}

void display_room_info()
{
  display.setCursor(0, ROOM_LINE_POS);
  display.print(F("Room: "));
  display.println(room);
}

void display_temp_and_humidity(sensors_event_t temp, sensors_event_t humidity)
{
  display.setCursor(0,TEMP_LINE_POS); 
  display.print(F("Temperature: "));
  display.println(temp.temperature);
  display.setCursor(0,HUMIDITY_LINE_POS);             // Start at top-left corner
  display.print(F("Humidity: "));
  display.println(humidity.relative_humidity);
  
}

void display_sensor_error()
{
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,INIT_LINE_POS);          
  display.print(F("Sensor Fail"));
  display.setCursor(0,RESETTING_LINE_POS);
  display.print(F("Resetting"));
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

    if ((millis() - start) > 10000)
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

  Serial.begin(115200);
  
  Serial.print("Wifi Temp Sensor V");
  Serial.println(VERSION);
  EEPROM.begin(EEPROM_SIZE);

  uint8_t ssid_buffer_size = EEPROM.read(EEPROM_SSID_BUFFER_SIZE_LOC);
  uint8_t password_buffer_size = EEPROM.read(EEPROM_PASSWORD_BUFFER_SIZE_LOC);
  uint8_t room_buffer_size = EEPROM.read(EEPROM_ROOM_BUFFER_SIZE_LOC);
  uint8_t broker_buffer_size = EEPROM.read(EEPROM_BROKER_BUFFER_SIZE_LOC);

  Serial.println("EEPROM Test");
  Serial.print("ssid buffer size: ");
  Serial.println(ssid_buffer_size);
  Serial.print("password buffer size: ");
  Serial.println(password_buffer_size);
  Serial.print("room buffer size: ");
  Serial.println(room_buffer_size);
  Serial.print("broker buffer size: ");
  Serial.println(room_buffer_size);

  if (ssid_buffer_size != 0)
  {
    for (int index = 0; index < ssid_buffer_size; index++)
    {
      ssid[index] = EEPROM.read(EEPROM_SSID_LOC+index);
    }
    ssid[ssid_buffer_size] = '\0';
    Serial.print("EEPROM ssid: ");
    Serial.println(ssid);
  }

  if (password_buffer_size != 0)
  {
    for (int index = 0; index < password_buffer_size; index++)
    {
      pass[index] = EEPROM.read(EEPROM_PASSWORD_LOC+index);
    }
    pass[password_buffer_size] = '\0';
    Serial.print("EEPROM password: ");
    Serial.println(pass);
  }

  if (room_buffer_size != 0)
  {
    for (int index = 0; index < room_buffer_size; index++)
    {
      room[index] = EEPROM.read(EEPROM_ROOM_LOC+index);
    }
    room[room_buffer_size] = '\0';
    Serial.print("EEPROM room: ");
    Serial.println(room);
    strcat(tempTopic, room);
    strcat(tempTopic, "/temperature");
    strcat(humidityTopic, room);
    strcat(humidityTopic, "/humidity");
  }
  else
  {
    strcat(tempTopic, DEFAULT_ROOM_NAME);
    strcat(tempTopic, "/temperature");
    strcat(humidityTopic, DEFAULT_ROOM_NAME);
    strcat(humidityTopic, "/humidity");
  }

  if (broker_buffer_size != 0)
  {
    for (int index = 0; index < broker_buffer_size; index++)
    {
      broker[index] = EEPROM.read(EEPROM_BROKER_LOC+index);
    }
    broker[broker_buffer_size] = '\0';
    Serial.print("Broker address: ");
    Serial.println(broker);
  }

  Wire.begin(2, 0);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed, could not start screen"));
  }

  display.clearDisplay();
  // Show system initialising
  display_initialising();
  display.display();

  wifi_connected = connect_to_wifi();

  if (wifi_connected) mqtt_connected = connect_to_mqtt();

  display.clearDisplay();
  display_network_details(wifi_connected, mqtt_connected);
  display.display();

  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    display.clearDisplay();
    display_sensor_error();
    display.display();
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

    if (WiFi.isConnected() && !reconnect_required)
    {
      wifi_connected = true;
      Serial.println("WiFi Connected");
      if (!mqtt_connected) mqtt_connected = connect_to_mqtt();
      
      if(mqtt_connected)
      {
        Serial.print("Connected to broker: ");
        Serial.println(broker);
      }
      else
      {
        Serial.println("Broker not connected");
      }
      disconnectedTimeLast = 0;
    }
    else
    {
      Serial.println("WiFi Disconnected");
      mqtt_connected = false;
      if (disconnectedTimeLast == 0)
      {
        disconnectedTimeLast = currentMillis;
      }
      else
      {
        if ((currentMillis - disconnectedTimeLast) > reconnectInterval )
        {
          reconnect_required = true;
          reconnectInterval = max(reconnectInterval * 2, RECONNECT_INTERVAL_MAX);
          disconnectedTimeLast = 0;
        }
      }

      if (reconnect_required)
      {
        WiFi.disconnect();
        Serial.println("Attempting to reconnect to wifi");
        wifi_connected = connect_to_wifi();
        reconnect_required = false;
        disconnectedTimeLast = 0;
      }
    }

    Serial.println();

    display.clearDisplay();

    display_network_details(wifi_connected, mqtt_connected);
    display_room_info();
    display_temp_and_humidity(temp, humidity);

    display.display();
  }

  int serial_bytes_available = Serial.available();
  if (serial_bytes_available)
  {
    char input_buffer[64];
    initialise_array(64, input_buffer, 32);
    Serial.readBytes(input_buffer, serial_bytes_available);

    // store incoming in serial buffer
    for (int index; index < serial_bytes_available; index++)
    {
      serial_buffer[index + serial_buffer_pos] = input_buffer[index];
      serial_buffer_pos++;
    }

    char* result = strchr(serial_buffer, ';');
    if (result != NULL)
    {
      Serial.println("Command found");
      char command[100];
      initialise_array(64, command, '\0');
      uint pos = result - serial_buffer;
      strncpy(command, serial_buffer, pos+1);
      
      //reset serial buffer
      initialise_array(512, serial_buffer, '\0');
      serial_buffer_pos = 0;

      bool command_found = false;

      char* command_result = strstr(command, "ssid");
      if ((!command_found) && command_result != NULL)
      {
        char* colon = strchr(command, ':');
        if (colon != NULL)
        {
          int colon_pos = colon - command;
          if (colon_pos == 5)
          {
            Serial.println("Found ssid write command");
            int ssid_length = pos - colon_pos - 1;;
            for (int index = 0; index < ssid_length; index++)
            {
              EEPROM.write(EEPROM_SSID_LOC+index, command[colon_pos+index+1]);
              ssid[index] = command[colon_pos+index+1];
            }
            ssid[ssid_length] = '\0';
            EEPROM.write(EEPROM_SSID_BUFFER_SIZE_LOC, ssid_length);
            EEPROM.commit();
            Serial.println("Updated ssid eeprom entry");
            reconnect_required = true;
          }
        }
        else
        {
          if (pos == 5)
          {
            Serial.println("Found ssid read command");
            char ssid[EEPROM_SSID_SIZE];
            int ssid_size = EEPROM.read(EEPROM_SSID_BUFFER_SIZE_LOC);
            for (int index = 0; index < ssid_size; index++)
            {
              ssid[index] = EEPROM.read(EEPROM_SSID_LOC + index);
            }
            ssid[ssid_size] = '\0';
            Serial.print("SSID: ");
            Serial.println(ssid);
          }
        }
        command_found = true;
      }

      command_result = strstr(command, "password");
      if ((!command_found) && command_result != NULL)
      {
        char* colon = strchr(command, ':');
        if (colon != NULL)
        {
          int colon_pos = colon - command;
          if (colon_pos == 9)
          {
            Serial.println("Found password write command");
            int password_length = pos - colon_pos - 1;;
            for (int index = 0; index < password_length; index++)
            {
              EEPROM.write(EEPROM_PASSWORD_LOC+index, command[colon_pos+index+1]);
              pass[index] = command[colon_pos+index+1];
            }
            pass[password_length] = '\0';
            EEPROM.write(EEPROM_PASSWORD_BUFFER_SIZE_LOC, password_length);
            EEPROM.commit();
            Serial.println("Updated password eeprom entry");
            reconnect_required = true;
          }
        }
        else
        {
          if (pos == 9)
          {
            Serial.println("Found password read command");
            char password[EEPROM_PASSWORD_SIZE];
            int password_size = EEPROM.read(EEPROM_PASSWORD_BUFFER_SIZE_LOC);
            for (int index = 0; index < password_size; index++)
            {
              password[index] = EEPROM.read(EEPROM_PASSWORD_LOC + index);
            }
            password[password_size] = '\0';
            Serial.print("Password: ");
            Serial.println(password);
          }
        }
        command_found = true;
      }

      command_result = strstr(command, "room");
      if ((!command_found) && command_result != NULL)
      {
        char* colon = strchr(command, ':');
        if (colon != NULL)
        {
          int colon_pos = colon - command;
          if (colon_pos == 5)
          {
            Serial.println("Found room write command");
            int room_length = pos - colon_pos - 1;;
            for (int index = 0; index < room_length; index++)
            {
              EEPROM.write(EEPROM_ROOM_LOC+index, command[colon_pos+index+1]);
              room[index] = command[colon_pos+index+1];
            }
            room[room_length] = '\0';
            *tempTopic = '\0';
            *humidityTopic = '\0';
            strcat(tempTopic, room);
            strcat(tempTopic, "/temperature");
            strcat(humidityTopic, room);
            strcat(humidityTopic, "/humidity");
            EEPROM.write(EEPROM_ROOM_BUFFER_SIZE_LOC, room_length);
            EEPROM.commit();
            Serial.println("Updated room eeprom entry");
            reconnect_required = true;
          }
        }
        else
        {
          if (pos == 5)
          {
            Serial.println("Found room read command");
            char room[EEPROM_ROOM_SIZE];
            int room_size = EEPROM.read(EEPROM_ROOM_BUFFER_SIZE_LOC);
            for (int index = 0; index < room_size; index++)
            {
              room[index] = EEPROM.read(EEPROM_ROOM_LOC + index);
            }
            room[room_size] = '\0';
            Serial.print("Room: ");
            Serial.println(room);
          }
        }
        command_found = true;
      }

      command_result = strstr(command, "broker");
      if ((!command_found) && command_result != NULL)
      {
        char* colon = strchr(command, ':');
        if (colon != NULL)
        {
          int colon_pos = colon - command;
          if (colon_pos == 7)
          {
            Serial.println("Found broker write command");
            int broker_length = pos - colon_pos - 1;;
            for (int index = 0; index < broker_length; index++)
            {
              EEPROM.write(EEPROM_BROKER_LOC+index, command[colon_pos+index+1]);
              broker[index] = command[colon_pos+index+1];
            }
            broker[broker_length] = '\0';
            EEPROM.write(EEPROM_BROKER_BUFFER_SIZE_LOC, broker_length);
            EEPROM.commit();
            Serial.println("Updated broker eeprom entry");
            reconnect_required = true;
          }
        }
        else
        {
          if (pos == 7)
          {
            Serial.println("Found broker read command");
            char broker[EEPROM_BROKER_SIZE];
            int broker_size = EEPROM.read(EEPROM_BROKER_BUFFER_SIZE_LOC);
            for (int index = 0; index < broker_size; index++)
            {
              broker[index] = EEPROM.read(EEPROM_BROKER_LOC + index);
            }
            broker[broker_size] = '\0';
            Serial.print("Broker: ");
            Serial.println(broker);
          }
        }
        command_found = true;
      }

      if (command_found == false)
      {
        Serial.println("No valid command");
      }
    }
  }
}
