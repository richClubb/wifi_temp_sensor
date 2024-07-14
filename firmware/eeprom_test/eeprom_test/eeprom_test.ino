#include <EEPROM.h>

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

uint8_t ssid_buffer_size = 0;
char eeprom_ssid[EEPROM_SSID_SIZE];

uint8_t password_buffer_size = 0;
char eeprom_password[EEPROM_PASSWORD_SIZE];

uint8_t room_buffer_size = 0;
char eeprom_room[EEPROM_ROOM_SIZE];

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

void setup() {
  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);

  ssid_buffer_size = EEPROM.read(EEPROM_SSID_BUFFER_SIZE_LOC);
  password_buffer_size = EEPROM.read(EEPROM_PASSWORD_BUFFER_SIZE_LOC);
  room_buffer_size = EEPROM.read(EEPROM_ROOM_BUFFER_SIZE_LOC);

  Serial.println("EEPROM Test");
  Serial.print("ssid buffer size: ");
  Serial.println(ssid_buffer_size);
  Serial.print("password buffer size: ");
  Serial.println(password_buffer_size);
  Serial.print("room buffer size: ");
  Serial.println(room_buffer_size);

  if (ssid_buffer_size != 0)
  {
    char ssid[EEPROM_SSID_SIZE];
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
    char password[EEPROM_PASSWORD_SIZE];
    for (int index = 0; index < password_buffer_size; index++)
    {
      password[index] = EEPROM.read(EEPROM_PASSWORD_LOC+index);
    }
    password[password_buffer_size] = '\0';
    Serial.print("EEPROM password: ");
    Serial.println(password);
  }

  if (room_buffer_size != 0)
  {
    char room[EEPROM_ROOM_SIZE];
    for (int index = 0; index < room_buffer_size; index++)
    {
      room[index] = EEPROM.read(EEPROM_ROOM_LOC+index);
    }
    room[room_buffer_size] = '\0';
    Serial.print("EEPROM room: ");
    Serial.println(room);
  }
}

void loop() {
  // put your main code here, to run repeatedly:

  int serial_bytes_available = Serial.available();
  if (serial_bytes_available)
  {
    char input_buffer[64];
    initialise_array(64, input_buffer, 32);
    Serial.readBytes(input_buffer, serial_bytes_available);

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
      initialise_array(64, command, 32);
      uint pos = result - serial_buffer;
      strncpy(command, serial_buffer, pos+1);
      initialise_array(512, serial_buffer, 32);
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
            }
            EEPROM.write(EEPROM_SSID_BUFFER_SIZE_LOC, ssid_length);
            EEPROM.commit();
            Serial.println("Updated ssid eeprom entry");
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
            }
            EEPROM.write(EEPROM_PASSWORD_BUFFER_SIZE_LOC, password_length);
            EEPROM.commit();
            Serial.println("Updated password eeprom entry");
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
            }
            EEPROM.write(EEPROM_ROOM_BUFFER_SIZE_LOC, room_length);
            EEPROM.commit();
            Serial.println("Updated room eeprom entry");
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

      if (command_found == false)
      {
        Serial.println("No valid command");
      }
    }
  }
}
