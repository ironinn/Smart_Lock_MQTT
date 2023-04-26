/*
İLK AÇILIŞTA KART TANIMLAMASI
--İlk Açılışta Kart Hafıza Silme
  1-ESP 'te Güç Verilir.
  2-Aynı Anda Butona  15 'sn basılı tutulur.
  3- Silme moduna girildiğinde KIRMIZI Işık yanık kalır.
  4- İşlem bittiğinde 2 defa kırmızı ısık yanıp söner.
  5- MAVI Işık Yanıp Söner.
  6- Yeni MASTER KART  Okutulur.
  7- Tanımlama tamamlandığında yeşil-mavi-kırmızı ışıklar sırası ile yanar.
*/
/*
 - Programlama modunda kart tanımlama, mevcut kartın yetkisini kaldırma
   Master kart okutulur programala moduna giriş yapılır.
   Mevcut kartı silmek için , mevcut kart okutulur ve silme işlemi gerçekleşir.
   Yeni kart okutur, yeni kart tanımlaması yapılır.
   İşlemler bittikten sonra Master Kart tekrar okutulur ve program modundan çıkılır.
 */

#include <Arduino.h>
#include <WiFi.h>
// #include <WebServer.h>
//#include <AutoConnect.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h> // We are going to read and write PICC's UIDs from/to EEPROM

#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <ArduinoHA.h>
// #define COMMON_ANODE

#define wipeB 3 // Button pin for Wipeode
//!!!!!!!!!!!!! Bu butona basıldığında Master kart silme işlemleri gerçekleştiriliyor.
#define SS_PIN 5   // ESP32 pin GIOP5
#define RST_PIN 27 // ESP32 pin GIOP27
#define SCAN_INTERVAL 5000
#define EEPROM_SIZE 512
MFRC522 mfrc522(SS_PIN, RST_PIN);

#define redLed 22 // led_pinleri
#define greenLed 25
#define blueLed 26

#define esp32_alarm_on_off_pin 32
#define SERVO_PIN 33
// #define COMMON_ANODE

#ifdef COMMON_ANODE
#define ON_LED LOW
#define OFF_LED HIGH
#else
#define ON_LED HIGH
#define OFF_LED LOW
#endif
unsigned long lastMsg = 0;  // millis kontrol tanımı
unsigned long lastMsg2 = 0; // millis kontrol tanımı
unsigned long lastMsg3 = 0; // millis kontrol tanımı

unsigned long lastTagScannedAt = 0;

WebServer Server;
// AutoConnect Portal(Server);
AutoConnectConfig Config; // Enable autoReconnect supported on v0.9.4

const char *ssid = "Zyxel_2AC1";
const char *password = "7v5WGEdz2.";

void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  /*while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }*/
  Serial.println(WiFi.localIP());
}

void rootPage()
{
  char content[] = "Merhaba Dunya";
  Server.send(200, "text/plain", content);
}

// AUTOCONNECT SETUP END

// WiFi ve PubSubClient Ataması
// const char* mqtt_server = "69683d8507ae4b5fa28675479aaf4fcf.s1.eu.hivemq.cloud"; // replace with your broker url
//#define BROKER_ADDR IPAddress(192, 168, 1, 200)
const char *mqtt_server = "192.168.1.200"; //"mqtt://10.211.55.8";
const char *mqtt_username = "mqttuser";
const char *mqtt_password = "12345";
const int mqtt_port = 1883;
byte mac[] = {0x00, 0x10, 0xFA, 0x6E, 0x38, 0x4E};
// const char* mqtt_sunucu = "127.0.0.1";
WiFiClient client;
HADevice device; //(mac, sizeof(mac));
HAMqtt mqtt(client, device);
HATagScanner scanner("myscanner");
HALock lock("myLock");

const char *alarm_durum_topic = "alarmo/state"; //
const char *alarmed_home = "armed_home";
const char *alarmed_away = "armed_away";
const char *alarm_disabled = "disarmed";

const char *alarm_command = "alarmo/command";
const char *alarm_home = "arm_home";
const char *alarm_away = "arm_away";
const char *alarm_disable = "disarm";

const char *servo_derece_topic = "aha/deedba6efeeb/servo_derece/stat_t"; //
const char *kapi_durum_topic = "aha/deedba6efeeb/kapi_sensor/stat_t";    // Mega- kapı üzerindeki sensordeki değeri alarak kapı acık mı kapalımı öğreniyoruz.
const char *readed_card_topic = "sensor/readed_card";
const char *modem_on_off_topic = "aha/deedba6efeeb/modem_on_off/stat_t";

bool alarm_durum = false;
bool kapi_durum = false;
bool kilit_durum = false;
bool modem_on_off_durum = false;

Servo myservo;
int pos = 0;
char posString[8];

String message = "";
char *c_message;

// manuel mod tanım kodları
//-----------------------------------
boolean match = false;       // initialize card match to false
boolean programMode = false; // initialize programming mode to false
boolean replaceMaster = false;

boolean successfulRead; // Variable integer to keep if we have Successful Read from Reader

byte storedCard[4]; // Stores an ID read from EEPROM
byte readedCard[4]; // Stores scanned ID read from RFID Module
byte masterCard[4]; // Stores master card's ID read from EEPROM

//////////////////////////////////  Alarm Işıklandırma   ///////////////////////////////////
void cycleLeds()
{
  digitalWrite(redLed, OFF_LED);  // Make sure red LED is off
  digitalWrite(greenLed, ON_LED); // Make sure green LED is on
  digitalWrite(blueLed, OFF_LED); // Make sure blue LED is off
  delay(200);
  digitalWrite(redLed, OFF_LED);   // Make sure red LED is off
  digitalWrite(greenLed, OFF_LED); // Make sure green LED is off
  digitalWrite(blueLed, ON_LED);   // Make sure blue LED is on
  delay(200);
  digitalWrite(redLed, ON_LED);    // Make sure red LED is on
  digitalWrite(greenLed, OFF_LED); // Make sure green LED is off
  digitalWrite(blueLed, OFF_LED);  // Make sure blue LED is off
  delay(200);
}
//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn()
{
  digitalWrite(blueLed, ON_LED);   // Blue LED ON and ready to read card
  digitalWrite(redLed, OFF_LED);   // Make sure Red LED is off
  digitalWrite(greenLed, OFF_LED); // Make sure Green LED is off
  // digitalWrite(relay, HIGH); 		// Make sure Door is Locked
}
//////////////////////////////////////// Kapı Kilitleme Fonksiyonu  ///////////////////////////////////
void kilitle()
{
  Serial.println("Güle Güle Kapı Kilitleniyor...");
  myservo.attach(SERVO_PIN);
  // Serial.println("0");     // You can display on the serial the signal value
  myservo.write(90 - pos); // Turn clockwise at high speed
  delay(2000);
  myservo.detach(); // Stop. You can use deatch function or use write(x), as x is the middle of 0-180 which is 90, but some lack of precision may change this value;
  kilit_durum = true;
}
//////////////////////////////////////// Kapı Kilitini Açma Fonksiyonu  ///////////////////////////////////
void kilit_ac()
{

  myservo.attach(SERVO_PIN); // Always use attach function after detach to re-connect your servo with the board
  // Serial.println("0");//Turn left high speed
  Serial.println(" Hoşgeldin:) Kapı Kiliti Açılıyor...");
  myservo.write(90 + pos);
  delay(2000);
  myservo.detach(); // Stop
  kilit_durum = false;
}
//////////////////////////////////////// Kapı Açma Fonksiyonu  ///////////////////////////////////
void kapi_ac()
{
  myservo.attach(SERVO_PIN); // Always use attach function after detach to re-connect your servo with the board
  // Serial.println("0");//Turn left high speed
  myservo.write(90 + pos);
  delay(500);
  myservo.detach(); // Stop
  kilit_durum = false;
}
/////////////////////////////////////////  Yetki Verildi.    ///////////////////////////////////
void granted(int setDelay)
{
  digitalWrite(blueLed, OFF_LED); // Turn off blue LED
  digitalWrite(redLed, OFF_LED);  // Turn off red LED
  digitalWrite(greenLed, ON_LED); // Turn on green LED
  // digitalWrite(relay, LOW); 		// Unlock door!
  // delay(setDelay); 					// Hold door lock open for given seconds
  // digitalWrite(relay, HIGH); 		// Relock door
  delay(1000); // Hold green LED on for a second
}
///////////////////////////////////////// Yetkisiz Giriş  ///////////////////////////////////
void denied()
{
  digitalWrite(greenLed, OFF_LED); // Make sure green LED is off
  digitalWrite(blueLed, OFF_LED);  // Make sure blue LED is off
  digitalWrite(redLed, ON_LED);    // Turn on red LED
  delay(1000);
}
// Flashes the green LED 3 times to indicate a successful write to EEPROM
void successWrite()
{
  digitalWrite(blueLed, OFF_LED);  // Make sure blue LED is off
  digitalWrite(redLed, OFF_LED);   // Make sure red LED is off
  digitalWrite(greenLed, OFF_LED); // Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, ON_LED); // Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, OFF_LED); // Make sure green LED is off
  delay(200);
  digitalWrite(greenLed, ON_LED); // Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, OFF_LED); // Make sure green LED is off
  delay(200);
  digitalWrite(greenLed, ON_LED); // Make sure green LED is on
  delay(200);
}
///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failedWrite()
{
  digitalWrite(blueLed, OFF_LED);  // Make sure blue LED is off
  digitalWrite(redLed, OFF_LED);   // Make sure red LED is off
  digitalWrite(greenLed, OFF_LED); // Make sure green LED is off
  delay(200);
  digitalWrite(redLed, ON_LED); // Make sure red LED is on
  delay(200);
  digitalWrite(redLed, OFF_LED); // Make sure red LED is off
  delay(200);
  digitalWrite(redLed, ON_LED); // Make sure red LED is on
  delay(200);
  digitalWrite(redLed, OFF_LED); // Make sure red LED is off
  delay(200);
  digitalWrite(redLed, ON_LED); // Make sure red LED is on
  delay(200);
}
///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the blue LED 3 times to indicate a success delete to EEPROM
void successDelete()
{
  digitalWrite(blueLed, OFF_LED);  // Make sure blue LED is off
  digitalWrite(redLed, OFF_LED);   // Make sure red LED is off
  digitalWrite(greenLed, OFF_LED); // Make sure green LED is off
  delay(200);
  digitalWrite(blueLed, ON_LED); // Make sure blue LED is on
  delay(200);
  digitalWrite(blueLed, OFF_LED); // Make sure blue LED is off
  delay(200);
  digitalWrite(blueLed, ON_LED); // Make sure blue LED is on
  delay(200);
  digitalWrite(blueLed, OFF_LED); // Make sure blue LED is off
  delay(200);
  digitalWrite(blueLed, ON_LED); // Make sure blue LED is on
  delay(200);
}

///////////////////////////////////////// NFC Tag Okuma  ///////////////////////////////////
bool getID()
{
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
  {

    return false;
  }

  if (millis() - lastTagScannedAt < SCAN_INTERVAL)
  {
    return false; // prevents spamming the HA. You can implement your own logic for debouncing the scans.
  }
  Serial.print("Size:");
  Serial.println(mfrc522.uid.size);
  Serial.println(F("Scanned PICC's UID:"));
  for (int i = 0; i < mfrc522.uid.size; i++)
  { //
    readedCard[i] = mfrc522.uid.uidByte[i];
    // UID[i] = readedCard[i];
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }

  // the tag UID needs to be converted into string
  char tag[mfrc522.uid.size * 2 + 1] = {0};
  HAUtils::byteArrayToStr(tag, mfrc522.uid.uidByte, mfrc522.uid.size);

  Serial.print("  =>>Card scanned: ");
  Serial.println(tag);

  if (modem_on_off_durum)
  {
    scanner.tagScanned(tag); // let the HA know about the scanned tag
  }
  // mfrc522.PICC_HaltA();    // Stop reading
  lastTagScannedAt = millis();
  return true;
}
/////////////////////////////////////  Hafıdan kayıtlı Kartı Okuma   /////////////////////////
void readID(int number)
{
  int start = (number * 4) + 2; // Figure out starting position
  for (int i = 0; i < 4; i++)
  {                                         // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i); // Assign values read from EEPROM to array
  }
}
///////////////////////////////////// NFC TAG Karşılaştırma  ///////////////////////////////
boolean checkTwo(byte a[], byte b[])
{
  if (a[0] != NULL) // Make sure there is something in the array first
    match = true;   // Assume they match at first
  for (int k = 0; k < 4; k++)
  {                   // Loop 4 times
    if (a[k] != b[k]) // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if (match)
  {              // Check to see if if match is still true
    return true; // Return true
  }
  else
  {
    return false; // Return false
  }
}

////////////////////////// Kayıtlı NFC Tag ın hafızadaki yerini bulur   /////////////////////
int findIDSLOT(byte find[])
{
  int count = EEPROM.read(0); // Read the first Byte of EEPROM that
  for (int i = 1; i <= count; i++)
  {            // Loop once for each EEPROM entry
    readID(i); // Read an ID from EEPROM, it is stored in storedCard[4]
    if (checkTwo(find, storedCard))
    { // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i; // The slot number of the card
      break;    // Stop looking we found it
    }
  }
  return 0;
}

///////////////////////////////// Hafızadan Kayıtlı NFC Tag'ı Bul  //////////////////////////
boolean findID(byte find[])
{
  int count = EEPROM.read(0); // Read the first Byte of EEPROM that
  Serial.print("EEPROM daki Kayıtlı Kart Adeti: ");
  Serial.println(count);
  for (int i = 1; i <= count; i++)
  {            // Loop once for each EEPROM entry
    readID(i); // Read an ID from EEPROM, it is stored in storedCard[4]
    if (checkTwo(find, storedCard))
    { // Check to see if the storedCard read from EEPROM
      // Serial.println("Okutulan kart ile hafıdaki kart eşleşti.");
      return true;
      break; // Bakmayı kesiyoruz.
    }
    else
    { //
      // Serial.println("Hafızadaki kartla eşleşmedi.");
    }
  }
  return false;
}
////////////////////// Check readedCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
boolean isMaster(byte test[])
{
  if (checkTwo(test, masterCard))
    return true;
  else
    return false;
}
void writeID(byte a[])
{
  if (!findID(a))
  {                            // Before we write to the EEPROM, check to see if we have seen this card before!
    int num = EEPROM.read(0);  // Get the numer of used spaces, position 0 stores the number of ID cards
    int start = (num * 4) + 6; // Figure out where the next slot starts
    num++;                     // Increment the counter by one
    // EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(0, num); // Write the new count to the counter
    EEPROM.commit();
    for (int j = 0; j < 4; j++)
    {                                // Loop 4 times
      EEPROM.write(start + j, a[j]); // Write the array values to EEPROM in the right position
      EEPROM.commit();
    }
    successWrite();
    Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else
  {
    failedWrite();
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID(byte a[])
{
  if (!findID(a))
  {                // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite(); // If not
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else
  {
    int num = EEPROM.read(0); // Get the numer of used spaces, position 0 stores the number of ID cards
    int slot;                 // Figure out the slot number of the card
    int start;                // = ( num * 4 ) + 6; // Figure out where the next slot starts
    int looping;              // The number of times the loop repeats
    int j;
    int count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT(a);       // Figure out the slot number of the card to delete
    if (!slot == 0)
    {
      start = (slot * 4) + 2;
      looping = ((num - slot) * 4);
      num--; // Decrement the counter by one
      // EEPROM.begin(EEPROM_SIZE);
      EEPROM.write(0, num); // Write the new count to the counter
      for (j = 0; j < looping; j++)
      {                                                      // Loop the card shift times
        EEPROM.write(start + j, EEPROM.read(start + 4 + j)); // Shift the array values to 4 places earlier in the EEPROM
        EEPROM.commit();
      }
      for (int k = 0; k < 4; k++)
      { // Shifting loop
        EEPROM.write(start + j + k, 0);
        EEPROM.commit();
      }
      successDelete();
      Serial.println(F("Succesfully removed ID record from EEPROM"));
    }
    else
    {
      failedWrite();
      Serial.println("SlotID bulunamadı. Silme işlemi gerçekleşemedi.");
    }
  }
}

void onMessage(const char *topic, const uint8_t *payload, uint16_t length)
{
  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }
  c_message = new char[message.length() + 1]; // Burada message uzunluğunda char karakter nesnesi oluşturuluyor.
  strcpy(c_message, message.c_str());         // char mesajı const char a çeviriliyor.
  // message = messageTemp;
  //  İşlemler
  Serial.print("Topic: ");
  Serial.print(topic);
  Serial.print(" =>Message: ");
  Serial.println(message);
  //---------------

  if (strcmp(topic, alarm_durum_topic) == 0)
  {
    if (message != alarm_disabled)
    {
      Serial.print("Alarm Kuruldu.=> Alarm Durum: ");
      alarm_durum = 1;
      Serial.println(alarm_durum);
    }
    else if (message == alarm_disabled)
    {
      Serial.println("Alarm Kapatıldı.");
      alarm_durum = 0;
    }
  }
  if (strcmp(topic, kapi_durum_topic) == 0)
  {
    if (message == "ON")
    {
      kapi_durum = true;
    }
    else
    {
      kapi_durum = false;
    }
    kapi_durum = payload;
  }
  if (strcmp(topic, modem_on_off_topic) == 0)
  {
    if (message == "ON")
    {
      modem_on_off_durum = true;
    }
    else
    {
      modem_on_off_durum = false;
    }
  }

  if (strcmp(topic, servo_derece_topic) == 0)
  {
    // pos = atoi(c_message);
    pos = message.toInt();
  }
  message = ""; // mesaj değişkeni sıfırlanıyor
}
void onConnected()
{
  mqtt.subscribe(kapi_durum_topic);
  mqtt.subscribe(alarm_durum_topic);
  mqtt.subscribe(servo_derece_topic);
  mqtt.subscribe(modem_on_off_topic);
}

void onLockCommand(HALock::LockCommand cmd, HALock *sender)
{
  if (cmd == HALock::CommandLock)
  {
    Serial.println("Command: Lock");
    kilitle();
    sender->setState(HALock::StateLocked); // report state back to the HA
  }
  else if (cmd == HALock::CommandUnlock)
  {
    Serial.println("Command: Unlock");
    kilit_ac();
    sender->setState(HALock::StateUnlocked); // report state back to the HA
  }
  else if (cmd == HALock::CommandOpen)
  {
    if (sender->getCurrentState() != HALock::StateUnlocked)
    {
      // kilit_ac();
      return; // optionally you can verify if the lock is unlocked before opening
    }
    // kapi_ac();
    Serial.println("Command: Open");
  }
}

void alarm_on_off(const char *alarm_tip, int alarm_delay = 0)
{
  StaticJsonDocument<64> doc;
  doc["command"] = alarm_tip;
  doc["delay"] = alarm_delay;
  doc["code"] = "22222";

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  mqtt.publish(alarm_command, buffer, n);
}

void setup()
{
  Serial.begin(9600);
  initWiFi();
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());
  Serial.println();
  EEPROM.begin(EEPROM_SIZE);

  // Unique ID must be set!
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));
  device.setName("ESP Kilit");
  device.setManufacturer("Serkan Demirhan");
  device.setSoftwareVersion("1.0.1");
  // device.setModel("ESP32 Door Lock");
  device.enableSharedAvailability();
  device.enableLastWill();

  lock.onCommand(onLockCommand);
  lock.setName("Kapı Kilit");           // optional
  lock.setIcon("mdi:door-closed-lock"); // optional

  // scanner.setName("Tag Kart Okuyucu");
  scanner.setName("tag_scanner");

  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init();
  mfrc522.PCD_DumpVersionToSerial();

  // Enable saved past credential by autoReconnect option,
  // even once it is disconnected.
  Config.autoReconnect = true;
  Config.hostName = "ESP32-Kilit";

  // Portal.config(Config);

  // Behavior a root path of ESP8266WebServer.
  Server.on("/", rootPage);
  Server.begin(); /// Portal.begin()
  Serial.println("WiFi connected: " + WiFi.localIP().toString());

  //=======================PİN AYARLARI====================
  pinMode(esp32_alarm_on_off_pin, OUTPUT);
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(wipeB, INPUT_PULLUP);
  digitalWrite(esp32_alarm_on_off_pin, OFF_LED);
  digitalWrite(redLed, OFF_LED);   // Make sure led is off
  digitalWrite(greenLed, OFF_LED); // Make sure led is off
  digitalWrite(blueLed, OFF_LED);  // Make sure led is off //

  myservo.attach(SERVO_PIN);

  //====================   MQTT AYARLARI   ======================================
  mqtt.onMessage(onMessage);
  mqtt.onConnected(onConnected);
  mqtt.begin(mqtt_server, mqtt_username, mqtt_password);

  //================   İLK KURULUMDA RFID KART TANIMLAMA KODLARI  ===============================
  if (digitalRead(wipeB) == LOW)
  {                               // when button pressed pin should get low, button connected to ground
    digitalWrite(redLed, ON_LED); // Sıfırlama işlemine kadar Kırmızı Led yanık kalacak.
    Serial.println(F("Wipe Button Pressed"));
    Serial.println(F("You have 15 seconds to Cancel"));
    Serial.println(F("This will be remove all records and cannot be undone"));
    delay(5000);                   // Give user enough time to cancel operation
    if (digitalRead(wipeB) == LOW) // Eğer buton hala basılı ise silme işlemi gerçekleştirilir.
    {
      Serial.println(F("Starting Wiping EEPROM"));
      EEPROM.begin(EEPROM_SIZE);
      for (int x = 0; x < EEPROM.length(); x = x + 1)
      { // Loop end of EEPROM address
        if (EEPROM.read(x) == 0)
        { // If EEPROM address 0
          // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
        }
        else
        {
          EEPROM.write(x, 0); // if not write 0 to clear, it takes 3.3mS
          EEPROM.commit();
        }
      }
      Serial.println(F("EEPROM Successfully Wiped"));
    }
    else
    {
      Serial.println(F("Wiping Cancelled"));
      digitalWrite(redLed, OFF_LED);
    }
  }

  if (EEPROM.read(1) != 143)
  {
    Serial.println(F("Master Kart Tanımlı Değil...!!!!"));
    Serial.println(F("Master Kart yapmak istediğiniz NFC Kartı Okutunuz..."));
    do
    {
      successfulRead = getID();      // sets successfulRead to 1 when we get read from reader otherwise 0
      digitalWrite(blueLed, ON_LED); // Visualize Master Card need to be defined
      delay(200);
      digitalWrite(blueLed, OFF_LED);
      delay(200);
    } while (!successfulRead); // Program will not go further while you not get a successful read
    EEPROM.begin(EEPROM_SIZE);
    for (int j = 0; j < 4; j++)
    {                                     // Loop 4 times
      EEPROM.write(2 + j, readedCard[j]); // Write scanned PICC's UID to EEPROM, start from address 3
      EEPROM.commit();
    }
    EEPROM.write(1, 143); // Write to EEPROM we defined Master Card.
    EEPROM.commit();
    Serial.println(F("Master Kart Tanımlandı."));
  }

  Serial.println(F("-------------------"));
  Serial.print(F("Master Kart UID: "));
  for (int i = 0; i < 4; i++)
  {                                     // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(2 + i); // Write it to masterCard
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Herşey Hazır..."));
  Serial.println(F("Waiting PICCs to be scanned"));
  cycleLeds(); // Herşeyin hazır olduğuna dair ışıklı geri bildirim veriyoruz.
  //  RFID KURULUM KODLARI SON
}

void loop()
{
  unsigned long now = millis();
  successfulRead = getID();
  // Do nothing. Everything is done in another task by the web server
  Server.handleClient();
  if (message != "")
  {
    Serial.print("Message: ");
    Serial.println(message);
  }
  if (now - lastMsg3 > 1000)
  {
    Serial.print("Modem On_OFF: ");
    Serial.print(modem_on_off_durum);
    Serial.print(" => Alarm Durumu: ");
    Serial.print(alarm_durum);
    Serial.print(" => Kilit Durum: ");
    Serial.println(kilit_durum);
    lastMsg3 = now;
  }
  {
    /* code */
  }

  if (successfulRead)
  {
    if (digitalRead(wipeB) == LOW)
    {
      Serial.println(F("Wipe Button Pressed"));
      Serial.println(F("Master Card will be Erased! in 10 seconds"));
      delay(10000);
      if (digitalRead(wipeB) == LOW)
      {
        EEPROM.write(1, 0); // Reset Magic Number.
        Serial.println(F("Restart device to re-program Master Card"));
        while (1)
          ;
      }
    }
    if (programMode)
    {
      if (isMaster(readedCard))
      { // Eğer Master Kart Okutulur ise programlama modundan çıkılıyor.
        Serial.println(F("Master Card Scanned"));
        Serial.println(F("Exiting Program Mode"));
        Serial.println(F("-----------------------------"));
        programMode = false;
        return;
      }
      else
      {
        if (findID(readedCard)) // Programlama modunda master kart dışında bir kart okutulursa o kart silinir.
        {
          Serial.println(F("I know this PICC, removing..."));
          deleteID(readedCard);
          Serial.println("-----------------------------");
          Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
        }
        else // Eğer tanımsız bir kart eklenirse tanımlama işlemi gerçekleştirilir.
        {    //
          Serial.println(F("I do not know this PICC, adding..."));
          writeID(readedCard);
          // add_card_yetki(readedCard);
          Serial.println(F("-----------------------------"));
          Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
        }
      }
    }
    else // program modunda değilse içeri girer ve kart tipine bakılır.
    {
      if (isMaster(readedCard)) // Master Kart okutulursa , Program mod aktif edilir.
      {
        programMode = true;
        Serial.println(F("Hello Master - Entered Program Mode"));

        int count = EEPROM.read(0); // Read the first Byte of EEPROM that
        Serial.print(F("I have ")); // stores the number of ID's in EEPROM
        Serial.print(count);
        Serial.print(F(" record(s) on EEPROM"));
        Serial.println("");
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
        Serial.println(F("Scan Master Card again to Exit Program Mode"));
        Serial.println(F("-----------------------------"));
      }
      else // Master Kart Değilse Tanımlı ,Tanımsız mı kontrol edilerek Giriş yada Red yapılır.
      {
        if (findID(readedCard)) // Tanımlı ise Kilit Açılır.
        {
          granted(300); // Open the door lock for 300 ms
          if (kilit_durum)
          {
            if (modem_on_off_durum == false && alarm_durum) // Modem Kapalı ve alarm açık ise ,Alarm kapat gönderiliyor.
            {
              Serial.println("Modem Kapalı Kablo üzerin MEGA'ya Alarm İptal Gönderildi.");
              digitalWrite(esp32_alarm_on_off_pin, OFF_LED);
              alarm_durum = false;
            }
            kilit_ac();
            if (alarm_durum != 0)
            {
              // alarm_on_off(alarm_disable); // Alarm panelinde alarmı aktif ediyor.
            }
          }
          else
          {
            kilitle();
            if (modem_on_off_durum == false && alarm_durum == false)
            {
              Serial.println("Modem Kapalı Kablo üzerin MEGA'ya Alarm AÇ Gönderildi.");
              digitalWrite(esp32_alarm_on_off_pin, ON_LED); //
              alarm_durum = true;
            }
            if (alarm_durum == 0)
            {
              // alarm_on_off(alarm_home);
            }
          }
        }
        else // Tanımlı Değil ise reddedilir
        {
          Serial.println(F("Geçersiz Kart..."));
          denied();
        }
      }
    }
  }
  if (programMode)
  {
    cycleLeds(); // Program Mode cycles through RGB waiting to read a new card
  }
  else
  {
    normalModeOn(); // Normal mode, blue Power LED is on, all others are off
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    if (kilit_durum)
    {
      lock.setState(HALock::StateLocked);
    }
    else
    {
      lock.setState(HALock::StateUnlocked);
    }
  }

  if ((WiFi.status() != WL_CONNECTED) && (now - lastMsg2 > 60000) && modem_on_off_durum)
  { // Burası test edilemedi. Özellikle internet bağlantısı koptığında ne yapacak bakılması gerekiyor.
    // Bu kod çalışmaz ise MEGA olduğu gibi reset pine high gönderilir ve yeniden başlatılarak intenete baglanır.
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    lastMsg2 = now;
  }

  if ((now - lastMsg > 50))
  {
    mqtt.loop();
    lastMsg = now;
  }
}
