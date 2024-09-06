
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoBLE.h>
// #include <WiFiManager.h>
#include <Preferences.h>
#include <RTClib.h>
#include <Firebase_ESP_Client.h>
#include <Adafruit_NeoPixel.h>
#include <DHT.h>
#include "Adafruit_MAX1704X.h"
#include "Adafruit_MPR121.h"
#include "Audio.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include <Wire.h>
#include <EEPROM.h>
#include <time.h>
#include "OTA_cert.h"
#include <HTTPUpdate.h>

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif
#define debug true

// pin definitions
#define MG_SIG 42   // Solenoid
#define BATSDA 8    // Battery Monitor
#define BATSCL 9    // Battery Monitor
#define ALRT 41     // Battery Monitor
#define DS_DT 37    // Temp Sensor
#define GRNLed 40   // Battery LEDs
#define REDLed 39   // Battery LEDs
#define BTN 1       // Push Button
#define DOOR_DAT 38 // Door Pin
#define DIN 18      // MP3 Player
#define LRCLK 12    // MP3 Player
#define BCLK 14     // MP3 Player
#define NEO_RX 5    // GPS
#define NEO_TX 6    // GPS
#define MPRSDA 8    // Keypad
#define MPRSCL 9    // Keypad
#define MPRIRQ 11   // Keypad
#define WS2812 16   // Keypad LEDs
#define SD_MOSI 48  // card reader
#define SD_MISO 21  // card reader
#define SD_CSK 47   // card reader
#define SD_CS 13    // card reader
#define FAN 6       // Fan pin

#define DHTTYPE DHT22

//********************************BLE parameters ********************************
BLEService deviceService("19B10010-E8F2-537E-4F6C-D104768A1214"); // create service

BLEStringCharacteristic deviceCharacteristic("183E", BLENotify, 100);
BLEStringCharacteristic devicegetCharacteristic("216A", BLEWrite, 100);

// ****************************************************************

// Firebase parameters
#define DATABASE_URL "https://ddbox-427e4-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define API_KEY "AIzaSyCBYQsRGkWn8mBq6veOKHSA2pHxyDRb6qc"
#define USER_EMAIL "fareed@test.com"
#define USER_PASSWORD "112233"
String userID = "";

// OTA parameters ---------------------------------------------------------------------------------
String FirmwareVer = {"1.3"};
#define URL_fw_Version "https://raw.githubusercontent.com/Usamamughal00/MyDrop_OTA/master/firmware_version.txt"
#define URL_fw_Bin "https://raw.githubusercontent.com/Usamamughal00/MyDrop_OTA/master/fw/firmware.bin"
void firmwareUpdate();
int FirmwareVersionCheck();

unsigned long previousMillis = 0; // will store last time LED was updated
unsigned long previousMillis_2 = 0;
const long interval = 300000;
const long mini_interval = 60000;

void repeatedCall();
void firmwareUpdate(void);
int FirmwareVersionCheck(void);

//----------------------------------------------------------------

// Your WiFi credentials
String ssid = "";
String pass = "";
bool cred_saved = false;

// WiFiManager wm;
// WiFiManagerParameter EMAIL("1", "userID", "", 40);
String email = "";
// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

// Define Firebase Data object
FirebaseData fbdo, stream;
FirebaseAuth auth;
FirebaseConfig config;

Preferences preferences;

// Components Objects
DHT dht(DS_DT, DHTTYPE);
Adafruit_NeoPixel led_keypad = Adafruit_NeoPixel(12, WS2812, NEO_GRB + NEO_KHZ800);
Adafruit_MPR121 cap = Adafruit_MPR121();
Adafruit_MAX17048 maxlipo;
Audio audio;

const char *ntpServer = "pool.ntp.org";

unsigned long sendDataPrevMillis = 0;
String path = "/users/users_details/" + userID;

// status variables
bool isStreamData;
unsigned long otp_time = 0;
bool otp_setup = false;
char raw_otp[4];
int raw_otp_count = 0;
bool old_door_status = false;
float old_temp = 0;
float old_hum = 0;
int old_battery_percent = 0;
int autoLockTime = 0;
bool autoLock = false;
unsigned long buttonStatusTime = 0;
bool buttonStatus = false;
unsigned long bat_timer = 0;

// firebase data variables
bool lock, unlock, otp_gen;
bool lock_status = true;
String language;
char otp[4];

// debounce variables
int buttonPrevious = HIGH;
unsigned long buttonTime = 0;
const long dobounce = 250;
long countJson = 0;
// heartbeat variable
unsigned long heartbeat_timer = 0;
unsigned long heartbeat_timer_delay = 15000;

// keypad variables
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

// Functions definitions
void lock_action(bool state);
void buttonDebounce();
void gen_otp();
void playSound(int number);
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void updateLog(String action, String method);
String getTime();
int mapLEDwithButton(int pressButton);
void saveParamsCallback();
int writeStringToEEPROM(int addrOffset, const String &strToWrite);
int readStringFromEEPROM(int addrOffset, String *strToRead);
void blePeripheralConnectHandler(BLEDevice central);
void blePeripheralDisconnectHandler(BLEDevice central);
void deviceCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic);
void initWiFi();
void initFirebase();
////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(150);
  preferences.begin("appStorage", false);
  countJson = preferences.getLong("JSONcounter", 0);
  preferences.end();
  if (!BLE.begin())
  {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
  }
  BLE.setLocalName("BLE Scanner");
  // set the UUID for the service this peripheral advertises:
  BLE.setAdvertisedService(deviceService);
  // add the characteristics to the service
  deviceService.addCharacteristic(deviceCharacteristic);
  deviceService.addCharacteristic(devicegetCharacteristic);
  // add the service
  BLE.addService(deviceService);
  // ledCharacteristic.writeValue(0);
  // buttonCharacteristic.writeValue(0);
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
  devicegetCharacteristic.setEventHandler(BLEWritten, deviceCharacteristicWritten);
  // set an initial value for the characteristic
  devicegetCharacteristic.writeValue("?");
  // start advertising
  BLE.advertise();
  Serial.println("Bluetooth® device active, waiting for connections...");
  cred_saved = EEPROM.read(48);
  Serial.print("Cred ");
  Serial.println(cred_saved);
  if (cred_saved)
  {
    readStringFromEEPROM(0, &ssid);
    readStringFromEEPROM(25, &pass);
    readStringFromEEPROM(50, &userID);
    Serial.println(ssid);
    Serial.println(pass);
    Serial.println(userID);
    path = "/users/users_details/" + userID;
    Serial.println(path);
    initWiFi();
    if (WiFi.status() == WL_CONNECTED)
      initFirebase();
  }

  pinMode(BTN, INPUT_PULLUP);
  pinMode(DOOR_DAT, INPUT_PULLDOWN);
  pinMode(GRNLed, OUTPUT);
  pinMode(REDLed, OUTPUT);
  pinMode(MG_SIG, OUTPUT);
  // digitalWrite(MG_SIG, LOW);  // Solenoid energized when RESET
  digitalWrite(MG_SIG, HIGH);
  pinMode(FAN, OUTPUT);
  digitalWrite(FAN, LOW);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SD_CSK, SD_MISO, SD_MOSI);
  SPI.setFrequency(1000000);
  // Serial.begin(115200);
  SD.begin(SD_CS);

  dht.begin();

  led_keypad.begin();
  led_keypad.show();

  Wire.begin();
  if (!cap.begin(0x5A))
  {
    Serial.println("MPR121 not found, check wiring?");
    while (1)
      ;
  }
  while (!maxlipo.begin())
  {
    Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
    delay(2000);
  }

  digitalWrite(REDLed, HIGH);

  /*
    // WiFi Manager part
    // wm.resetSettings();


    wm.addParameter(&EMAIL);
    wm.setSaveParamsCallback(saveParamsCallback);
    std::vector<const char *> menu = {"wifi", "sep", "restart", "exit"};
    wm.setMenu(menu);
    wm.setClass("invert");

    bool res;
    res = wm.autoConnect("myBox", "12345678");

    if (!res)
    {
      Serial.println("Failed to connect");
    }
    else
    {
      Serial.println("Connected with Router");
    }
    readStringFromEEPROM(1, &email);
   //Need to correct the path.
   // path = "/users/users_details/" + email; // String(userID);

   */
  // WiFi.begin(ssid, password);
  // Serial.print("Connecting to Wi-Fi");
  // while (WiFi.status() != WL_CONNECTED) {
  //   Serial.print(".");
  //   delay(300);
  // }
  digitalWrite(GRNLed, HIGH);
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  for (int i = 0; i < 13; i++)
  {
    led_keypad.setPixelColor(i, 0, 0, 255);
    led_keypad.setPixelColor(i - 1, 0, 0, 0);
    led_keypad.show();
    delay(400);
  }
  digitalWrite(REDLed, LOW);

  audio.setPinout(BCLK, LRCLK, DIN);
  audio.setVolume(21); // 0...21

  /******************** Firebase Setup******************/
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi is not connected");
    BLE.poll();
    return;
  }
  repeatedCall();
  BLE.poll();
  if (Firebase.ready())
  {
    /*********************** Stream Actions **************************/
    if (isStreamData)
    {
      // if (lock) {
      //   Serial.println("App, Lock");
      //   lock_action(false);
      //   if (Firebase.ready()) {
      //     Serial.printf("Set lock... %s\n", Firebase.RTDB.setBool(&fbdo, path + "/app_commands/lock", false) ? "ok" : fbdo.errorReason().c_str());
      //   } else {
      //     Serial.println("Issue");
      //   }
      //   updateLog("lock", "app");
      // }
      if (unlock)
      {
        Serial.println("App, Unlock");
        lock_action(true);
        if (Firebase.ready())
        {
          Serial.printf("Set unlock... %s\n", Firebase.RTDB.setBool(&fbdo, path + "/app_commands/unlock", false) ? "ok" : fbdo.errorReason().c_str());
        }
        else
        {
          Serial.println("Issue");
        }
        updateLog("unlock", "app");
      }
      if (otp_gen)
      {
        Serial.print("OTP Generation: ");
        gen_otp();
        if (Firebase.ready())
        {
          Serial.printf("Set otp... %s\n", Firebase.RTDB.setBool(&fbdo, path + "/app_commands/otp", false) ? "ok" : fbdo.errorReason().c_str());
        }
        else
        {
          Serial.println("Issue");
        }
      }
      //    Serial.print("Language: ");
      //    Serial.println(language);
      isStreamData = false;
    }
    /*********************** Stream Actions **************************/

    /************************* Check OTP *****************************/
    if (otp_setup)
    {
      // logic for keypad and check for OTP
      if (millis() - otp_time > (5 * 60 * 1000))
      {
        otp_setup = false;
        Serial.printf("Set Pincode... %s\n", Firebase.RTDB.setString(&fbdo, path + "/status/pincode", "None") ? "ok" : fbdo.errorReason().c_str());
        delay(100);
        Serial.printf("Set Pincode... %s\n", Firebase.RTDB.setString(&fbdo, path + "/status/OTP_Start_Time", "None") ? "ok" : fbdo.errorReason().c_str());
        otp[0] = '0';
        otp[1] = '0';
        otp[2] = '0';
        otp[3] = '0';
      }
    }
    /************************* Check OTP *****************************/

    /************************* Auto Lock *****************************/
    if (autoLock)
    {
      if ((millis() - autoLockTime) > 2000)
      {
        lock_action(false);
        updateLog("lock", "auto");
        autoLock = false;
      }
    }
    /************************* Auto Lock *****************************/

    /******************* Check Door, Temperature *********************/
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    if (isnan(temp) || isnan(hum))
    {
      // Serial.println("Failed to read from DHT sensor!");
    }
    else
    {
      if (abs(temp - old_temp) > 0.7)
      {
        if (temp > 30)
        {
          digitalWrite(FAN, HIGH);
        }
        else
        {
          digitalWrite(FAN, LOW);
        }
        Serial.printf("Set Temp... %s\n", Firebase.RTDB.setFloat(&fbdo, path + "/status/temperature", temp) ? "ok" : fbdo.errorReason().c_str());
        old_temp = temp;
      }
      if (abs(hum - old_hum) > 0.7)
      {
        Serial.printf("Set Hum... %s\n", Firebase.RTDB.setFloat(&fbdo, path + "/status/humidity", hum) ? "ok" : fbdo.errorReason().c_str());
        old_hum = hum;
      }
    }
    buttonDebounce();
    if (buttonStatus)
    {
      if ((millis() - buttonStatusTime) > 5000)
      {
        Serial.printf("Set Push Button... %s\n", Firebase.RTDB.setBool(&fbdo, path + "/device_commands/push_button", false) ? "ok" : fbdo.errorReason().c_str());
        buttonStatus = false;
      }
    }
    bool door_status;
    if (digitalRead(DOOR_DAT) == HIGH)
    {
      door_status = true;
    }
    else
    {
      door_status = false;
    }
    if (door_status != old_door_status)
    {
      Serial.printf("Set Door Status... %s\n", Firebase.RTDB.setBool(&fbdo, path + "/status/door_status", door_status) ? "ok" : fbdo.errorReason().c_str());
      old_door_status = door_status;
    }
    /******************* Check Door, Temperature *********************/

    /************************* Battery *****************************/
    if (millis() - bat_timer > 2000)
    {
      float cellVoltage = maxlipo.cellVoltage();
      int cellPercent = int(maxlipo.cellPercent());
      if (isnan(cellVoltage))
      {
        Serial.println("Failed to read cell voltage, check battery is connected!");
      }
      else
      {
        // Serial.print(F("Batt Voltage: "));
        // Serial.print(cellVoltage, 3);
        // Serial.println(" V");
        // Serial.print(F("Batt Percent: "));
        // Serial.print(maxlipo.cellPercent(), 1);
        // Serial.println(" %");
        // Serial.println();
        if (cellPercent != old_battery_percent)
        {
          Serial.printf("Set Battery Status... %s\n", Firebase.RTDB.setInt(&fbdo, path + "/status/battery", cellPercent) ? "ok" : fbdo.errorReason().c_str());
          old_battery_percent = cellPercent;
        }
      }
      bat_timer = millis();
    }
    /************************* Battery *****************************/
  }
  else
  {
    Serial.println("Firebase Issue");
  }
  /************************* KeyPad *****************************/
  currtouched = cap.touched();

  for (uint8_t i = 0; i < 12; i++)
  {
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)))
    {
      Serial.print(i);
      Serial.println(" touched");
      Serial.print(cap.filteredData(i));
      Serial.print("\t");
      Serial.print(cap.baselineData(i));
      Serial.print("\t");
      led_keypad.setPixelColor(mapLEDwithButton(i), 0, 0, 255);
      led_keypad.show();
      playSound(5);
      if (i == 11)
      {
        // lock
        lock_action(false);
        updateLog("lock", "Keypad");
      }
      else if (i == 10)
      {
        // unlock after checking OTP
        bool isPinCorrect = true;
        for (int i = 0; i < 4; i++)
        {
          if (raw_otp[i] != otp[i])
          {
            isPinCorrect = false;
            break;
          }
        }
        if (isPinCorrect && otp_setup)
        {
          ("Correct Pin");
          playSound(2);
          lock_action(true);
          otp_setup = false;
          Serial.printf("Set Pincode... %s\n", Firebase.RTDB.setString(&fbdo, path + "/status/pincode", "None") ? "ok" : fbdo.errorReason().c_str());
          delay(100);
          Serial.printf("Set Pincode... %s\n", Firebase.RTDB.setString(&fbdo, path + "/status/OTP_Start_Time", "None") ? "ok" : fbdo.errorReason().c_str());
          updateLog("unlock", "otp");
          digitalWrite(GRNLed, HIGH);
          otp[0] = '0';
          otp[1] = '0';
          otp[2] = '0';
          otp[3] = '0';
        }
        else
        {
          // play sound incorrect code
          Serial.println("Incorrect Pin");
          playSound(3);
          digitalWrite(REDLed, HIGH);
        }
        raw_otp_count = 0;
      }
      else
      {
        raw_otp[raw_otp_count] = i + '0';
        raw_otp_count++;
        if (raw_otp_count > 3)
        {
          raw_otp_count = 0;
        }
      }
      // digitalWrite(REDLed, LOW);
      // digitalWrite(GRNLed, LOW);
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)))
    {
      Serial.print(i);
      Serial.println(" released");
      led_keypad.setPixelColor(mapLEDwithButton(i), 0, 0, 0);
      led_keypad.show();
    }
  }

  lasttouched = currtouched;
  /************************* KeyPad *****************************/

  /************************* Heart Bea *****************************/
  if (millis() - heartbeat_timer >= heartbeat_timer_delay)
  {
    heartbeat_timer = millis();
    Serial.printf("Set Heartbeat... %s\n", Firebase.RTDB.setString(&fbdo, path + "/status/Heartbeat_Time", getTime()) ? "ok" : fbdo.errorReason().c_str());
  }
  delay(1);
}

void lock_action(bool state)
{
  if (state)
  {
    digitalWrite(MG_SIG, LOW);
  }
  else
  {
    digitalWrite(MG_SIG, HIGH);
  }
  lock_status = !state;
  Serial.printf("Set Lock Status... %s\n", Firebase.RTDB.setBool(&fbdo, path + "/status/lock_status", lock_status) ? "ok" : fbdo.errorReason().c_str());
  if (lock_status)
  {
    playSound(0);
  }
  else
  {
    autoLockTime = millis();
    autoLock = true;
    playSound(1);
  }
}

void buttonDebounce()
{
  int buttonPresent = digitalRead(BTN);
  if (buttonPresent == LOW && buttonPrevious == HIGH && (millis() - buttonTime) > dobounce)
  {
    Serial.println("Button Pressed");
    Serial.printf("Set Push Button... %s\n", Firebase.RTDB.setBool(&fbdo, path + "/device_commands/push_button", true) ? "ok" : fbdo.errorReason().c_str());
    buttonTime = millis();
    buttonStatusTime = millis();
    playSound(4);
    buttonStatus = true;
  }
  buttonPrevious = buttonPresent;
}

void gen_otp()
{
  // generate OTP
  String ot = "";
  for (int i = 0; i < 4; i++)
  {
    otp[i] = random(10) + '0';
    ot = ot + String(otp[i]);
    Serial.print(otp[i]);
  }
  Serial.println();
  Serial.printf("Set Pincode... %s\n", Firebase.RTDB.setString(&fbdo, path + "/status/pincode", ot) ? "ok" : fbdo.errorReason().c_str());
  Serial.printf("Set Pincode... %s\n", Firebase.RTDB.setString(&fbdo, path + "/status/OTP_Start_Time", getTime()) ? "ok" : fbdo.errorReason().c_str());
  otp_time = millis();
  otp_setup = true;
  digitalWrite(REDLed, LOW);
  digitalWrite(GRNLed, LOW);
}

void playSound(int number)
{
  String name = String(number) + ".wav";
  audio.connecttoFS(SD, name.c_str());
  while (audio.isRunning())
  {
    audio.loop();
  }
}

void streamCallback(FirebaseStream data)
{
  //  Serial.printf("sream path, %s\nevent path, %s\ndata type, %s\nevent type, %s\n\n",
  //                data.streamPath().c_str(),
  //                data.dataPath().c_str(),
  //                data.dataType().c_str(),
  //                data.eventType().c_str());
  //  printResult(data); // see addons/RTDBHelper.h

  String streamPath = String(data.dataPath());

  if (data.dataTypeEnum() == fb_esp_rtdb_data_type_boolean)
  {
    if (streamPath == "/lock")
    {
      lock = data.boolData();
    }
    else if (streamPath == "/unlock")
    {
      unlock = data.boolData();
    }
    else if (streamPath == "/otp")
    {
      otp_gen = data.boolData();
    }
  }
  else if (data.dataTypeEnum() == fb_esp_rtdb_data_type_string)
  {
    if (streamPath = "/language")
    {
      language = data.stringData();
    }
  }
  else if (data.dataTypeEnum() == fb_esp_rtdb_data_type_json)
  {
    FirebaseJson json = data.to<FirebaseJson>();

    size_t count = json.iteratorBegin();
    for (size_t i = 0; i < count; i++)
    {
      FirebaseJson::IteratorValue value = json.valueAt(i);

      if (value.type == FirebaseJson::JSON_OBJECT)
      {
        String key = value.key.c_str();
        if (key.equals("language"))
        {
          language = value.value.c_str();
        }
        else if (key.equals("lock"))
        {
          lock = value.value == "true";
        }
        else if (key.equals("otp"))
        {
          otp_gen = value.value == "true";
        }
        else if (key.equals("unlock"))
        {
          unlock = value.value == "true";
        }
      }
    }
    json.iteratorEnd(); // Free the used memory
  }
  // Serial.printf("Received stream payload size: %d (Max. %d)\n\n", data.payloadLength(), data.maxPayloadLength());
  isStreamData = true;
}

void streamTimeoutCallback(bool timeout)
{
  if (timeout)
    Serial.println("stream timeout, resuming...\n");
  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

void updateLog(String action, String method)
{
  FirebaseJson json;
  String timeStamp = getTime();
  json.add("action", action);
  json.add("method", method);
  json.add("user", "main");
  json.add("timestamp", timeStamp);
  Serial.printf("Update node... %s\n", Firebase.RTDB.updateNode(&fbdo, path + "/logs/" + String(countJson), &json) ? "ok" : fbdo.errorReason().c_str());
  countJson++;
  preferences.begin("appStorage", false);
  preferences.putLong("JSONcounter", countJson);
  preferences.end();
}

String getTime()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    // Serial.println("Failed to obtain time");
    return "0";
  }
  time(&now);
  DateTime rawTime = DateTime(now);
  char formattedTimestamp[20];
  sprintf(formattedTimestamp, "%04d-%02d-%02d %02d:%02d:%02d",
          rawTime.year(), rawTime.month(), rawTime.day(),
          rawTime.hour(), rawTime.minute(), rawTime.second());
  return String(formattedTimestamp);
}

int mapLEDwithButton(int pressButton)
{
  int ledNo = -1;
  if (pressButton == 0)
  {
    ledNo = 10;
  }
  else if (pressButton == 1)
  {
    ledNo = 2;
  }
  else if (pressButton == 2)
  {
    ledNo = 1;
  }
  else if (pressButton == 3)
  {
    ledNo = 0;
  }
  else if (pressButton == 4)
  {
    ledNo = 5;
  }
  else if (pressButton == 5)
  {
    ledNo = 4;
  }
  else if (pressButton == 6)
  {
    ledNo = 3;
  }
  else if (pressButton == 7)
  {
    ledNo = 8;
  }
  else if (pressButton == 8)
  {
    ledNo = 7;
  }
  else if (pressButton == 9)
  {
    ledNo = 6;
  }
  else if (pressButton == 10)
  {
    ledNo = 9;
  }
  else if (pressButton == 11)
  {
    ledNo = 11;
  }
  return ledNo;
}

// void saveParamsCallback()
// {
//   Serial.println("Get Params:");
//   Serial.print(EMAIL.getID());
//   Serial.print(" : ");
//   email = String(EMAIL.getValue());
//   Serial.println(email);
//   writeStringToEEPROM(1, email);
// }

int writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  EEPROM.commit();

  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
  EEPROM.commit();

  return addrOffset + 1 + len;
}

int readStringFromEEPROM(int addrOffset, String *strToRead)
{
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];

  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';

  *strToRead = String(data);
  return addrOffset + 1 + newStrLen;
}

void blePeripheralConnectHandler(BLEDevice central)
{
  // central connected event handler
  Serial.print("Connected event, central: ");
  Serial.println(central.address());
  deviceCharacteristic.writeValue("Success");
}

void blePeripheralDisconnectHandler(BLEDevice central)
{
  // central disconnected event handler
  Serial.print("Disconnected event, central: ");
  Serial.println(central.address());
}

void deviceCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic)
{
  // central wrote new value to characteristic
  Serial.print("Characteristic event, written: ");
  Serial.println(devicegetCharacteristic.value());
  String data = devicegetCharacteristic.value();
  int s1 = data.indexOf(';');
  ssid = data.substring(0, s1);
  pass = data.substring(s1 + 1);

  int s2 = pass.indexOf(';');
  userID = pass.substring(s2 + 1);
  pass = pass.substring(0, s2);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(userID);
  writeStringToEEPROM(0, ssid);
  writeStringToEEPROM(25, pass);
  writeStringToEEPROM(50, userID);
  cred_saved = true;
  EEPROM.write(48, cred_saved);
  EEPROM.commit();
  initWiFi();

  
  for (int i = 0; i < 300; i++)
  {
    BLE.poll();
    delay(10);
  }
  if (WiFi.status() == WL_CONNECTED)
    ESP.restart();
}
void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi ..");
  long x = 0;
  while (WiFi.status() != WL_CONNECTED && x < 1500)
  {
    Serial.print('.');
    BLE.poll();
    delay(10);
    x++;
  }
  if (WiFi.status() == WL_CONNECTED)
  {

    Serial.println(WiFi.localIP());
    deviceCharacteristic.writeValue("Connected");
  }
  else
  {
    deviceCharacteristic.writeValue("Not Connected");
  }
}
void initFirebase()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    config.api_key = API_KEY;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h
    Firebase.reconnectWiFi(true);
    fbdo.setResponseSize(2048);
    Firebase.begin(&config, &auth);
    // Streams
    if (!Firebase.RTDB.beginStream(&stream, path + "/app_commands"))
    {
      Serial.printf("Stream begin error, %s\n\n", stream.errorReason().c_str());
    }
    Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);

    Firebase.setDoubleDigits(2);
    /******************** Firebase Setup******************/
    // configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    configTime(0, 0, "pool.ntp.org");
    digitalWrite(GRNLed, LOW);
    if (Firebase.ready())
    {

      Serial.printf("Set BoxID... %s\n", Firebase.RTDB.setString(&fbdo, path + "/box_id", String(random(1000, 9999))) ? "ok" : fbdo.errorReason().c_str());
      delay(100);
      Serial.printf("Set Pincode... %s\n", Firebase.RTDB.setString(&fbdo, path + "/status/Firmware_Version", FirmwareVer) ? "ok" : fbdo.errorReason().c_str());
      delay(100);
      Serial.printf("Set Pincode... %s\n", Firebase.RTDB.setString(&fbdo, path + "/status/pincode", "None") ? "ok" : fbdo.errorReason().c_str());
      delay(100);
      Serial.printf("Set Pincode... %s\n", Firebase.RTDB.setString(&fbdo, path + "/status/OTP_Start_Time", "None") ? "ok" : fbdo.errorReason().c_str());
    }
  }
}
void repeatedCall()
{
  static int num = 0;
  unsigned long currentMillis = millis();
  if ((currentMillis - previousMillis) >= interval)
  {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    if (FirmwareVersionCheck())
    {
      firmwareUpdate();
    }
  }
  if ((currentMillis - previousMillis_2) >= mini_interval)
  {
    previousMillis_2 = currentMillis;
    if (WiFi.status() == WL_CONNECTED)
    {
      // if(debug)
      // Serial.println("wifi connected");
    }
    else
    {
      if (WiFi.status() != WL_CONNECTED)
      {
        // couldn't connect
        if (debug)
          Serial.println("[main] Couldn't connect to WiFi after multiple attempts");
        delay(5000);
        ESP.restart();
      }
      if (debug)
        Serial.println("Connected");
    }
  }
}

void firmwareUpdate(void)
{
  WiFiClientSecure OTA_client;
  OTA_client.setCACert(OTAcert);
  httpUpdate.setLedPin(2, LOW);
  t_httpUpdate_return ret = httpUpdate.update(OTA_client, URL_fw_Bin);

  switch (ret)
  {
  case HTTP_UPDATE_FAILED:
    if (debug)
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    if (debug)
      Serial.println("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    if (debug)
      Serial.println("HTTP_UPDATE_OK");
    break;
  }
}

int FirmwareVersionCheck(void)
{
  if (debug)
    Serial.println(FirmwareVer);
  String payload;
  int httpCode;
  String fwurl = "";
  fwurl += URL_fw_Version;
  fwurl += "?";
  fwurl += String(rand());
  if (debug)
    Serial.println(fwurl);
  WiFiClientSecure *OTA_client = new WiFiClientSecure;

  if (OTA_client)
  {
    OTA_client->setCACert(OTAcert);

    // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
    HTTPClient https;

    if (https.begin(*OTA_client, fwurl))
    { // HTTPS
      if (debug)
        Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      delay(100);
      httpCode = https.GET();
      delay(100);
      if (httpCode == HTTP_CODE_OK) // if version received
      {
        payload = https.getString(); // save received version
      }
      else
      {
        if (debug)
        {
          Serial.print("error in downloading version file:");
          Serial.println(httpCode);
        }
      }
      https.end();
    }
    delete OTA_client;
  }

  if (httpCode == HTTP_CODE_OK) // if version received
  {
    payload.trim();
    if (payload.equals(FirmwareVer))
    {
      if (debug)
        Serial.printf("\nDevice already on latest firmware version:%s\n", FirmwareVer);
      return 0;
    }
    else
    {
      if (debug)
      {
        Serial.println(payload);
        Serial.println("New firmware detected");
      }
      return 1;
    }
  }
  return 0;
}