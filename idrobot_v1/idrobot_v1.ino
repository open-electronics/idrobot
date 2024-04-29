/*    
 *     IdroBot v1 - Luca Bellan - 2024
 */

#include <ESP32Time.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "esp_bt_main.h"
#include "esp_bt.h"
#include "esp_wifi.h"

//  BEGIN CONFIGURATION

//#define USE_TOUCH_PIN_INSTEAD_BTN
#define DEFAULT_SLEEP_SECONDS   30
#define TOUCH_THRESHOLD         40
#define DISCONNECT_AFTER_SEC    300
#define CAPACITOR_CHARGE_MS     1500
#define SOLENOID_IMPULSE_MS     100
#define RTC_COMPENSATION_SEC    540

//  END CONFIGURATION

#define PIN_ADC                 35
#define PIN_BTN                 GPIO_NUM_33
#define PIN_CONVERTER_ENABLE    26
#define PIN_HBRIDGE_POWER       18
#define PIN_HBRIDGE_EN_1_2      27
#define PIN_HBRIDGE_1A          22
#define PIN_HBRIDGE_2A          21
#define PIN_HBRIDGE_EN_3_4      25
#define PIN_HBRIDGE_3A          17
#define PIN_HBRIDGE_4A          16

#define uS_TO_S_FACTOR          1000000ULL

#define UUID_WATER_SERVICE_1    "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define UUID_WATER_DAYS_1       "19b10000-e8f2-537e-4f6c-d104768a1214"
#define UUID_WATER_TIMES_1      "19b10001-e8f2-537e-4f6c-d104768a1214"
#define UUID_WATER_DURATIONS_1  "19b10002-e8f2-537e-4f6c-d104768a1214"
#define UUID_WATER_CONTROL_1    "19b10003-e8f2-537e-4f6c-d104768a1214"

#define UUID_WATER_SERVICE_2    "4fafc202-1fb5-459e-8fcc-c5c9c331914b"
#define UUID_WATER_DAYS_2       "19b10004-e8f2-537e-4f6c-d104768a1214"
#define UUID_WATER_TIMES_2      "19b10005-e8f2-537e-4f6c-d104768a1214"
#define UUID_WATER_DURATIONS_2  "19b10006-e8f2-537e-4f6c-d104768a1214"
#define UUID_WATER_CONTROL_2    "19b10007-e8f2-537e-4f6c-d104768a1214"

#define UUID_TIME_SERVICE       "4fafc203-1fb5-459e-8fcc-c5c9c331914b"
#define UUID_TIME               "19b10008-e8f2-537e-4f6c-d104768a1214"

#define UUID_BATTERY_SERVICE    "4fafc204-1fb5-459e-8fcc-c5c9c331914b"
#define UUID_BATTERY            "19b10009-e8f2-537e-4f6c-d104768a1214"



ESP32Time rtc;

struct WaterTime {
  byte hour;
  byte minute;
};

RTC_DATA_ATTR byte waterDays1[7];
RTC_DATA_ATTR WaterTime waterTimes1[7];
RTC_DATA_ATTR byte waterDurations1[7];
RTC_DATA_ATTR byte waterDays2[7];
RTC_DATA_ATTR WaterTime waterTimes2[7];
RTC_DATA_ATTR byte waterDurations2[7];
RTC_DATA_ATTR bool irrigating1;
RTC_DATA_ATTR bool irrigating2;
RTC_DATA_ATTR struct tm endIrrigationTime1;
RTC_DATA_ATTR struct tm endIrrigationTime2;
RTC_DATA_ATTR bool rtc_need_correction;

bool SomeoneConnected = false;

bool AllSettingsWritten[7] = {0, 0, 0, 0, 0, 0, 0};

void StartIrrigation(byte line);
void StopIrrigation(byte line);
void SettingsWritten(byte index);



class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      SomeoneConnected = true;
      Serial.println("BLE client connected.");
    }

    void onDisconnect(BLEServer* pServer) {
      SomeoneConnected = false;
      Serial.println("BLE client disconnected.");
    }
};


class CharacteristicCallback : public BLECharacteristicCallbacks { 
    void onWrite(BLECharacteristic* pCharacteristic) {

      size_t size = pCharacteristic->getLength();

      Serial.print("Size: ");
      Serial.println(size);

      if(pCharacteristic->getUUID().toString() == UUID_WATER_DAYS_1 && size == 7) {

        memcpy(waterDays1, pCharacteristic->getData(), sizeof(waterDays1));
        SettingsWritten(0);
        Serial.println("Water days 1:");for (int i=0; i<7; i++) {Serial.print(waterDays1[i]);Serial.print(" ");}Serial.println("");

      } else if(pCharacteristic->getUUID().toString() == UUID_WATER_TIMES_1 && size == 14) {

        memcpy(waterTimes1, pCharacteristic->getData(), sizeof(waterTimes1));
        SettingsWritten(1);
        Serial.println("Water times 1:");for (int i=0; i<7; i++) {Serial.print(waterTimes1[i].hour);Serial.print(":");Serial.print(waterTimes1[i].minute);Serial.print(" ");}Serial.println("");

      } else if(pCharacteristic->getUUID().toString() == UUID_WATER_DURATIONS_1 && size == 7) {

        memcpy(waterDurations1, pCharacteristic->getData(), sizeof(waterDurations1));
        SettingsWritten(2);
        Serial.println("Water durations 1:");for (int i=0; i<7; i++) {Serial.print(waterDurations1[i]);Serial.print(" ");}Serial.println("");

      } else if(pCharacteristic->getUUID().toString() == UUID_WATER_CONTROL_1 && size == 1) {

        byte Value[1];
        memcpy(Value, pCharacteristic->getData(), sizeof(Value));
        irrigating1 = Value[0];
        Serial.println("Water control 1:");Serial.println(Value[0]);
        if(Value[0] == 1) {
          StartIrrigation(1);
        } else {
          StopIrrigation(1);
        }

      } else if(pCharacteristic->getUUID().toString() == UUID_WATER_DAYS_2 && size == 7) {

        memcpy(waterDays2, pCharacteristic->getData(), sizeof(waterDays2));
        SettingsWritten(3);
        Serial.println("Water days 2:");for (int i=0; i<7; i++) {Serial.print(waterDays2[i]);Serial.print(" ");}Serial.println("");

      } else if(pCharacteristic->getUUID().toString() == UUID_WATER_TIMES_2 && size == 14) {

        memcpy(waterTimes2, pCharacteristic->getData(), sizeof(waterTimes2));
        SettingsWritten(4);
        Serial.println("Water durations 2:");for (int i=0; i<7; i++) {Serial.print(waterTimes2[i].hour);Serial.print(":");Serial.print(waterTimes2[i].minute);Serial.print(" ");}Serial.println("");

      } else if(pCharacteristic->getUUID().toString() == UUID_WATER_DURATIONS_2 && size == 7) {

        memcpy(waterDurations2, pCharacteristic->getData(), sizeof(waterDurations2));
        SettingsWritten(5);
        Serial.println("Water durations 2:");for (int i=0; i<7; i++) {Serial.print(waterDurations2[i]);Serial.print(" ");}Serial.println("");

      } else if(pCharacteristic->getUUID().toString() == UUID_WATER_CONTROL_2 && size == 1) {

        byte Value[1];
        memcpy(Value, pCharacteristic->getData(), sizeof(Value));
        irrigating2 = Value[0];
        Serial.println("Water control 2:");Serial.println(Value[0]);
        if(Value[0] == 1) {
          StartIrrigation(2);
        } else {
          StopIrrigation(2);
        }

      } else if(pCharacteristic->getUUID().toString() == UUID_TIME && size == 6) {

        byte datetime[6];
        memcpy(datetime, pCharacteristic->getData(), sizeof(datetime));
        rtc.setTime(datetime[0], datetime[1], datetime[2], datetime[3], datetime[4], 2000 + datetime[5]);  // ss mm hh DD MM YYYY
        rtc_need_correction = true;
        SettingsWritten(6);
        Serial.println("Set time:");for (int i=0; i<6; i++) {Serial.print(datetime[i]);Serial.print(" ");}Serial.println("");

      }

    }
};



void SettingsWritten(byte index) {

  AllSettingsWritten[index] = 1;

  //  Check if all settings are written and I can go to sleep (disconnect BLE client)
  for(byte i=0; i<7; i++) {
    if(AllSettingsWritten[i] == 0) {
      return;
    }
  }

  SomeoneConnected = false;

}



void setup() {

  //  Init
  Serial.begin(115200);

  pinMode(PIN_BTN, INPUT_PULLDOWN);
  pinMode(PIN_CONVERTER_ENABLE, OUTPUT);
  pinMode(PIN_HBRIDGE_POWER, OUTPUT);
  pinMode(PIN_HBRIDGE_EN_1_2, OUTPUT);
  pinMode(PIN_HBRIDGE_1A, OUTPUT);
  pinMode(PIN_HBRIDGE_2A, OUTPUT);
  pinMode(PIN_HBRIDGE_EN_3_4, OUTPUT);
  pinMode(PIN_HBRIDGE_3A, OUTPUT);
  pinMode(PIN_HBRIDGE_4A, OUTPUT);

  digitalWrite(PIN_CONVERTER_ENABLE, LOW);
  digitalWrite(PIN_HBRIDGE_POWER, LOW);
  digitalWrite(PIN_HBRIDGE_EN_1_2, LOW);
  digitalWrite(PIN_HBRIDGE_1A, LOW);
  digitalWrite(PIN_HBRIDGE_2A, LOW);
  digitalWrite(PIN_HBRIDGE_EN_3_4, LOW);
  digitalWrite(PIN_HBRIDGE_3A, LOW);
  digitalWrite(PIN_HBRIDGE_4A, LOW);


  //  Run only if params are set
  if(rtc.getYear() != 1970) {
    CompensateRTCDrift();
    RunIrrigation();
  }


  //  If wakeup caused by user: start BLE and wait for connection, else Run irrigation
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  #ifdef USE_TOUCH_PIN_INSTEAD_BTN
    if(wakeup_reason == ESP_SLEEP_WAKEUP_TOUCHPAD ) {
  #else
    if(wakeup_reason == ESP_SLEEP_WAKEUP_EXT0  ) {
  #endif

    StartBLE("IdroBot");

    delay(5000);

    //  Until we have a BLE connection or we are insice DISCONNECT_AFTER_SEC time: we sta awake and run irrigation checks
    unsigned long tmrDisconnect = millis();
    while(SomeoneConnected && millis() - tmrDisconnect < DISCONNECT_AFTER_SEC * 1000) {
      RunIrrigation();
      delay(5000);
    }

  }

  GoToSleep();

}



void loop() {

    //  Nothing to do here

}



void StartBLE(std::string device_name) {

  // Create the BLE Device
  BLEDevice::init(device_name);

  // Create BLE server
  BLEServer* bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  // Create water 1 service and characteristics
  BLEService* serWater1 = bleServer->createService(UUID_WATER_SERVICE_1);

  BLECharacteristic* carWaterDays1 = serWater1->createCharacteristic(
                               UUID_WATER_DAYS_1,
                               BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                             );
  carWaterDays1->setValue(waterDays1, sizeof(waterDays1));
  carWaterDays1->setCallbacks(new CharacteristicCallback());


  BLECharacteristic* carWaterTimes1 = serWater1->createCharacteristic(
                               UUID_WATER_TIMES_1,
                               BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                             );
  byte waterTimesByteArray1[sizeof(waterTimes1) * 7];
  WaterTimesToByteArray(waterTimes1, waterTimesByteArray1, 7);
  carWaterTimes1->setValue(waterTimesByteArray1, sizeof(waterTimesByteArray1));
  carWaterTimes1->setCallbacks(new CharacteristicCallback());

  BLECharacteristic* carWaterDurations1 = serWater1->createCharacteristic(
                                   UUID_WATER_DURATIONS_1,
                                   BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                                 );
  carWaterDurations1->setValue(waterDurations1, sizeof(waterDurations1));
  carWaterDurations1->setCallbacks(new CharacteristicCallback());

  BLECharacteristic* carWaterControl1 = serWater1->createCharacteristic(
                                   UUID_WATER_CONTROL_1,
                                   BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                                 );
  byte Irrig1[1];
  Irrig1[0] = irrigating1;
  carWaterControl1->setValue(Irrig1, sizeof(Irrig1));
  carWaterControl1->setCallbacks(new CharacteristicCallback());

  // Create water 2 service and characteristics
  BLEService* serWater2 = bleServer->createService(UUID_WATER_SERVICE_2);

  BLECharacteristic* carWaterDays2 = serWater2->createCharacteristic(
                               UUID_WATER_DAYS_2,
                               BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                             );
  carWaterDays2->setValue(waterDays2, sizeof(waterDays2));
  carWaterDays2->setCallbacks(new CharacteristicCallback());


  BLECharacteristic* carWaterTimes2 = serWater2->createCharacteristic(
                               UUID_WATER_TIMES_2,
                               BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                             );
  byte waterTimesByteArray2[sizeof(waterTimes2) * 7];
  WaterTimesToByteArray(waterTimes2, waterTimesByteArray2, 7);
  carWaterTimes2->setValue(waterTimesByteArray2, sizeof(waterTimesByteArray2));
  carWaterTimes2->setCallbacks(new CharacteristicCallback());

  BLECharacteristic* carWaterDurations2 = serWater2->createCharacteristic(
                                   UUID_WATER_DURATIONS_2,
                                   BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                                 );
  carWaterDurations2->setValue(waterDurations2, sizeof(waterDurations2));
  carWaterDurations2->setCallbacks(new CharacteristicCallback());

  BLECharacteristic* carWaterControl2 = serWater2->createCharacteristic(
                                   UUID_WATER_CONTROL_2,
                                   BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                                 );
  byte Irrig2[1];
  Irrig2[0] = irrigating2;
  carWaterControl2->setValue(Irrig2, sizeof(Irrig2));
  carWaterControl2->setCallbacks(new CharacteristicCallback());

  // Create time service and characteristic
  BLEService* serTime = bleServer->createService(UUID_TIME_SERVICE);

  BLECharacteristic* carTime = serTime->createCharacteristic(
                               UUID_TIME,
                               BLECharacteristic::PROPERTY_WRITE
                             );
  carTime->setCallbacks(new CharacteristicCallback());

  // Create battery service and characteristic
  BLEService* serBattery = bleServer->createService(UUID_BATTERY_SERVICE);

  BLECharacteristic* carBattery = serBattery->createCharacteristic(
                               UUID_BATTERY,
                               BLECharacteristic::PROPERTY_READ
                             );
  byte BatteryVolt[1];
  BatteryVolt[0] = GetBatteryCharge();
  carBattery->setValue(BatteryVolt, sizeof(BatteryVolt));

  // Start services
  serWater1->start();
  serWater2->start();
  serTime->start();
  serBattery->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  Serial.println("BLE advertising...");

}



void CompensateRTCDrift() {

  //  Retrieve current time
  time_t Now;
  time(&Now);
  struct tm TimeNow;
  localtime_r(&Now, &TimeNow);

  Serial.println("Current time: ");
  PrintDateTime(TimeNow);

  //  If it's midnight and rtc_need_correction: remove compensation from current time
  if (rtc_need_correction == true && TimeNow.tm_hour == 0) {
    Serial.println("Need time correction.");
    Now -= RTC_COMPENSATION_SEC;
    struct tm TimeCorrected;
    localtime_r(&Now, &TimeCorrected);
    //  Check if time hasn't go back before midnight
    if(TimeCorrected.tm_hour == 0) {
      rtc_need_correction = false;
      rtc.setTimeStruct(TimeCorrected);
      Serial.println("Time corrected: ");
      PrintDateTime(TimeCorrected);
    } else {
      Serial.println("Time correction discarded.");
    }
  }

  //  If it's one in the night: set rtc_need_correction = true
  if(rtc_need_correction == false && TimeNow.tm_hour == 1) {
    rtc_need_correction = true;
  }
  

}



void RunIrrigation() {

  //  Retrieve current time
  time_t Now;
  time(&Now);
  struct tm TimeNow;
  localtime_r(&Now, &TimeNow);
  byte WeekDay = (TimeNow.tm_wday + 6) % 7;

  //  Check if it's time to start irrigating (only if battery voltage is sufficient to stop it)
  byte BatteryVolt = GetBatteryCharge();
  Serial.print("Battery %: ");
  Serial.println(BatteryVolt);
  if(BatteryVolt >= 5) {
    Serial.print("Irrigating1: ");
    Serial.print(irrigating1);
    Serial.print("\tWeekDay: ");
    Serial.print(WeekDay);
    Serial.print("\twaterDays1[WeekDay]: ");
    Serial.print(waterDays1[WeekDay]);
    Serial.print("\twaterTimes1[WeekDay] ");
    Serial.print(waterTimes1[WeekDay].hour);
    Serial.print(":");
    Serial.print(waterTimes1[WeekDay].minute);
    Serial.print("\twaterDurations1[WeekDay]: ");
    Serial.println(waterDurations1[WeekDay]);

    Serial.println("- - - - - - - - - - - - - - - - - - - - -");

    Serial.print("Irrigating2: ");
    Serial.print(irrigating2);
    Serial.print("\tWeekDay: ");
    Serial.print(WeekDay);
    Serial.print("\twaterDays2[WeekDay]: ");
    Serial.print(waterDays2[WeekDay]);
    Serial.print("\twaterTimes2[WeekDay] ");
    Serial.print(waterTimes2[WeekDay].hour);
    Serial.print(":");
    Serial.print(waterTimes2[WeekDay].minute);
    Serial.print("\twaterDurations2[WeekDay]: ");
    Serial.println(waterDurations2[WeekDay]);

    Serial.println("");
    Serial.println("");

    if(!irrigating1 && waterDays1[WeekDay] == 1 && waterTimes1[WeekDay].hour == TimeNow.tm_hour && waterTimes1[WeekDay].minute == TimeNow.tm_min) {
      irrigating1 = true;
      struct tm EndTime1 = TimeNow;
      EndTime1.tm_min += waterDurations1[WeekDay];
      endIrrigationTime1 = EndTime1;
      StartIrrigation(1);
    }
    if(!irrigating2 && waterDays2[WeekDay] == 1 && waterTimes2[WeekDay].hour == TimeNow.tm_hour && waterTimes2[WeekDay].minute == TimeNow.tm_min) {
      irrigating2 = true;
      struct tm EndTime2 = TimeNow;
      EndTime2.tm_min += waterDurations2[WeekDay];
      endIrrigationTime2 = EndTime2;
      StartIrrigation(2);
    }
  } else {
    Serial.println("Battery voltage too low.");
  }

  //  If irrigation is in action: check if it's time to stop it
  if(irrigating1 && !DateTimeIsNULL(&endIrrigationTime1) && Now >= mktime(&endIrrigationTime1)) {
    irrigating1 = false;
    memset(&endIrrigationTime1, 0, sizeof(struct tm));
    StopIrrigation(1);
  }
  if(irrigating2 && !DateTimeIsNULL(&endIrrigationTime2) && Now >= mktime(&endIrrigationTime2)) {
    irrigating2 = false;
    memset(&endIrrigationTime2, 0, sizeof(struct tm));
    StopIrrigation(2);
  }

}



void GoToSleep() {

  //  Disable all peripherals and go to sleep
  Serial.println("Sleep now.");
  Serial.flush();

  esp_sleep_enable_timer_wakeup(DEFAULT_SLEEP_SECONDS * uS_TO_S_FACTOR);

  #ifdef USE_TOUCH_PIN_INSTEAD_BTN
    touchSleepWakeUpEnable(T5, TOUCH_THRESHOLD);
  #else
    esp_sleep_enable_ext0_wakeup(PIN_BTN, 1);
  #endif
  
  esp_bluedroid_disable();
  esp_bt_controller_disable();
  esp_wifi_stop(); 

  esp_deep_sleep_start();

}



void StartIrrigation(byte line) {

  Serial.print("Start irrigation ");
  Serial.println(line);
  //  Switch ON DC-DC converter and H-Bridge and charge capacitor
  digitalWrite(PIN_CONVERTER_ENABLE, HIGH);
  digitalWrite(PIN_HBRIDGE_POWER, HIGH);
  delay(CAPACITOR_CHARGE_MS);
  //  Solenoid open impulse
  if(line == 1) {
    digitalWrite(PIN_HBRIDGE_1A, HIGH);
    digitalWrite(PIN_HBRIDGE_2A, LOW);
    digitalWrite(PIN_HBRIDGE_EN_1_2, HIGH);
  } else {
    digitalWrite(PIN_HBRIDGE_3A, HIGH);
    digitalWrite(PIN_HBRIDGE_4A, LOW);
    digitalWrite(PIN_HBRIDGE_EN_3_4, HIGH);
  }
  delay(SOLENOID_IMPULSE_MS);
  if(line == 1) {
    digitalWrite(PIN_HBRIDGE_EN_1_2, LOW);
  } else {
    digitalWrite(PIN_HBRIDGE_EN_3_4, LOW);
  }
  //  Switch OFF DC-DC converter and H-Bridge
  digitalWrite(PIN_HBRIDGE_POWER, LOW);
  digitalWrite(PIN_CONVERTER_ENABLE, LOW);

}



void StopIrrigation(byte line) {

  Serial.print("Stop irrigation ");
  Serial.println(line);
  //  Switch ON DC-DC converter and H-Bridge and charge capacitor
  digitalWrite(PIN_CONVERTER_ENABLE, HIGH);
  digitalWrite(PIN_HBRIDGE_POWER, HIGH);
  delay(CAPACITOR_CHARGE_MS);
  //  Solenoid close impulse
  if(line == 1) {
    digitalWrite(PIN_HBRIDGE_1A, LOW);
    digitalWrite(PIN_HBRIDGE_2A, HIGH);
    digitalWrite(PIN_HBRIDGE_EN_1_2, HIGH);
  } else {
    digitalWrite(PIN_HBRIDGE_3A, LOW);
    digitalWrite(PIN_HBRIDGE_4A, HIGH);
    digitalWrite(PIN_HBRIDGE_EN_3_4, HIGH);
  }
  delay(SOLENOID_IMPULSE_MS);
  if(line == 1) {
    digitalWrite(PIN_HBRIDGE_EN_1_2, LOW);
  } else {
    digitalWrite(PIN_HBRIDGE_EN_3_4, LOW);
  }
  //  Switch OFF DC-DC converter and H-Bridge
  digitalWrite(PIN_HBRIDGE_POWER, LOW);
  digitalWrite(PIN_CONVERTER_ENABLE, LOW);

}



byte GetBatteryCharge() {

  //  Formula Vout = (Vbat*R2)/(R1+R2)
  
  int Voltage = map(analogRead(PIN_ADC), 1500, 1970, 0, 100);
  if (Voltage <0) {Voltage = 0;}
  if (Voltage >100) {Voltage = 100;}
  return (byte)Voltage;

}



void WaterTimesToByteArray(const WaterTime* waterTimes, byte* byteArray, size_t size) {

  for (size_t i = 0; i < size; ++i) {
    byteArray[2 * i] = waterTimes[i].hour;
    byteArray[2 * i + 1] = waterTimes[i].minute;
  }

}



bool DateTimeIsNULL(const struct tm *timePtr) {
    return (
        timePtr->tm_sec == 0 &&
        timePtr->tm_min == 0 &&
        timePtr->tm_hour == 0 &&
        timePtr->tm_mday == 0 &&
        timePtr->tm_mon == 0 &&
        timePtr->tm_year == 0 &&
        timePtr->tm_wday == 0 &&
        timePtr->tm_yday == 0 &&
        timePtr->tm_isdst == 0
    );
}



void PrintDateTime(struct tm timeinfo) {

  Serial.print(timeinfo.tm_year + 1900);
  Serial.print('/');
  Serial.print(timeinfo.tm_mon + 1);
  Serial.print('/');
  Serial.print(timeinfo.tm_mday);
  Serial.print(" ");
  Serial.print(timeinfo.tm_hour);
  Serial.print(':');
  Serial.print(timeinfo.tm_min);
  Serial.print(':');
  Serial.print(timeinfo.tm_sec);
  Serial.println();

}