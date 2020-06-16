#include <SPI.h>
#include <Ethernet.h>
//#include <EthernetUdp.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
//#include <SD.h>
#include <avr/wdt.h>

#define DS3231_I2C_ADDRESS 0x68
#define SERIAL_NUMBER 1000DEAD
#define DEFAULT_MAC { 0xDE, 0xAD, 0x10, 0x00, 0x04, 0x56 };
#define DEFAULT_PORT 8888
#define DUGME1_PIN 7
#define DUGME2_PIN 6
#define DUGME3_PIN 5
#define KLIMA_PIN 8
#define VENTILATOR_PIN 9
//#define SD_KARTICA_PIN 10
#define DHT21_PIN 14
#define DHTTYPE DHT21

#define BUTTON_PERIOD 1000
#define SENSOR_PERIOD 2000
#define DISPLAY_PERIOD 1000
#define CONTROL_PERIOD 60000
#define NETWORK_PERIOD 30000
#define TIME_PERIOD 1000
//#define FILE_PERIOD 10000

typedef struct FAZA {
  unsigned int min_temperatura;
  unsigned int max_temperatura;
  //char min_vlaznost;
  // char max_vlaznost;
  int min_co2;
  int max_co2;
  boolean ventilator;
  boolean klima;
  char naziv;
};

DHT dht(DHT21_PIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
//EthernetUDP Udp;
EthernetClient client;
byte _MAC_ADDRESS[] = DEFAULT_MAC;

const IPAddress _IP_ADDRESS(192, 168, 1, 106);
const IPAddress _SERVER_IP_ADDRESS(192, 168, 1, 2);
const IPAddress _DNS(8, 8, 8, 8);
const IPAddress _SUBNET(255, 255, 255, 0);

const unsigned int _PORT = DEFAULT_PORT;
//char packetBuffer[UDP_TX_PACKET_MAX_SIZE];

char fileName[8]  =  "LOG.TXT";

char tempDisplayOutput[21]  =  "Temp: 00,0C Klima:NE";
char vlazDisplayOutput[21]  =  "Vlaz: 00,0%  Vent:NE";
char co2DisplayOutput[21]   =  " CO2: 0000    MOD: 0";
char timeDisplayOutput[21]  =  "OK 00.00.2000 00:00 ";
char beginDisplayOutput[21] =  "Izaberi mod rada!   ";
char errorDisplayOutput[21] =  "Nedozvoljeno stanje!";
char networkDisplayOutput[21] =  "Slanje podataka.....";
char networkOKDisplayOutput[21] =  "Slanje podataka...OK";
char networkFailDisplayOutput[21] =  "Slanje nije uspjelo!";

// SERIAL_NUMBER-TEMPERATURE-HUMIDITY-CO2-AC-VENT-MOD-DATE-MONTH-YEAR-HOUR-MINUTE-SECOND
char networkResponse[46] = "1000DEA-00.0-00.0-0000-NE-NE-0-00002000000000";

FAZA pocetnaFaza      = {0, 0, 0, 0, false, false, 0};
FAZA fazaPrva         = {200, 220, 4000, 6000, true, true, 1};
FAZA fazaDruga        = {140, 180, 4000, 6000, true, true, 2};
FAZA fazaTreca        = {160, 180, 1100, 1500, true, true, 3};
FAZA nedozvoljenaFaza = {0, 0, 0, 0, false, false, 4};
FAZA* trenutnaFaza;

unsigned long buttonTime, sensorTime, controlTime, displayTime, networkTime, timeTime; //,fileTime;

void setup() {

  pinMode(DUGME1_PIN, INPUT);
  pinMode(DUGME2_PIN, INPUT);
  pinMode(DUGME3_PIN, INPUT);
  pinMode(KLIMA_PIN, OUTPUT);
  pinMode(VENTILATOR_PIN, OUTPUT);
  //pinMode(SD_KARTICA_PIN, OUTPUT);

  lcd.begin(20, 4);
  lcd.backlight();
  Ethernet.begin(_MAC_ADDRESS, _IP_ADDRESS, _DNS, _SERVER_IP_ADDRESS, _SUBNET);
  //Ethernet.begin(_MAC_ADDRESS, _IP_ADDRESS);
  //Udp.begin(DEFAULT_PORT);
  dht.begin();
  Wire.begin();
  //SD.begin(4);
  //setDS3231time(50,26,19,3,24,06,15);
  // Serial.begin(9600);
 wdt_enable(WDTO_8S);
   //wdt_enable(15);

  trenutnaFaza = &pocetnaFaza;

  buttonTime = millis();
  sensorTime = millis();
  controlTime = millis();
  displayTime = millis();
  networkTime = millis();
  timeTime = millis();
  // fileTime = millis();

}

void loop() {

  wdt_reset();

  if (millis() - timeTime  > TIME_PERIOD) {
    // provjeri stanje dugmadi
    timeAction();
    timeTime = millis();
  }

  wdt_reset();

  if ( millis() - buttonTime  > BUTTON_PERIOD) {
    // provjeri stanje dugmadi
    buttonAction();
    buttonTime = millis();
  }

  wdt_reset();

  if ( millis() - sensorTime > SENSOR_PERIOD) {
    // procitaj vrjednosti senzora
    sensorAction();
    sensorTime = millis();
  }

  wdt_reset();

  if (millis() - controlTime > CONTROL_PERIOD) {
    // preduzmi akciju paljenja uredjaja
    controlAction();
    controlTime = millis();
  }

  wdt_reset();

  if (  millis() - displayTime > DISPLAY_PERIOD) {
    // ispisi podatke na lcd
    displayAction();
    displayTime = millis();
  }

  wdt_reset();

  if ( millis() - networkTime > NETWORK_PERIOD) {
    // odgovori na mrezni upit
    networkAction();
    networkTime = millis();
  }

  //
  //  wdt_reset();
  //
  //  if ( millis() - fileTime > FILE_PERIOD) {
  //    // upisi na sd karticu
  //    fileAction();
  //    fileTime = millis();
  //  }

}

void buttonAction() {

  // ako je prvo pritisnuto onda je trenutnaFaza = fazaPrva itd...
  boolean valDugme1 = digitalRead(DUGME1_PIN) == HIGH;
  boolean valDugme2 = digitalRead(DUGME2_PIN) == HIGH;
  boolean valDugme3 = digitalRead(DUGME3_PIN) == HIGH;

  if      ( valDugme1 && !valDugme2 && !valDugme3 ) {
    trenutnaFaza = &fazaPrva;
  }
  else if ( !valDugme1 && valDugme2 && !valDugme3 ) {
    trenutnaFaza = &fazaDruga;
  }
  else if ( !valDugme1 && !valDugme2 && valDugme3 ) {
    trenutnaFaza = &fazaTreca;
  }
  else if ( !valDugme1 && !valDugme2 && !valDugme3 ) {
    trenutnaFaza = &pocetnaFaza;
  }
  else {
    trenutnaFaza = &nedozvoljenaFaza;
  }

  co2DisplayOutput[19] = trenutnaFaza->naziv + 48;
  networkResponse[29] = trenutnaFaza->naziv + 48;

}

void sensorAction() {

  unsigned int trenutnaTemperatura = dht.readTemperature() * 10 + 13;
  unsigned int trenutnaVlaznost = dht.readHumidity() * 10;
  unsigned int trenutniCO2 = random(4500, 5000) ;

  tempDisplayOutput[6] = networkResponse[8] = trenutnaTemperatura / 100 + 48;
  tempDisplayOutput[7] = networkResponse[9] = (trenutnaTemperatura % 100) / 10 + 48;
  tempDisplayOutput[9] = networkResponse[11] = trenutnaTemperatura % 10 + 48;

  vlazDisplayOutput[6] = networkResponse[13] = trenutnaVlaznost / 100 + 48;
  vlazDisplayOutput[7] = networkResponse[14] = (trenutnaVlaznost % 100) / 10 + 48;
  vlazDisplayOutput[9] = networkResponse[16] = trenutnaVlaznost % 10 + 48;

  co2DisplayOutput[6] = networkResponse[18] = trenutniCO2 / 1000 + 48;
  co2DisplayOutput[7] = networkResponse[19] = (trenutniCO2 % 1000) / 100 + 48;
  co2DisplayOutput[8] = networkResponse[20] = (trenutniCO2 % 100) / 10 + 48;
  co2DisplayOutput[9] = networkResponse[21] = trenutniCO2 % 10 + 48;

}

void controlAction() {

  unsigned int trenutnaTemperatura = dht.readTemperature() * 10;

  if (trenutnaFaza->klima && (trenutnaTemperatura > trenutnaFaza->max_temperatura)) {

    //postavi bit za paljenje klime
    // klimaUkljucena = true;
    digitalWrite(KLIMA_PIN, HIGH);
    tempDisplayOutput[18] = 'D';
    tempDisplayOutput[19] = 'A';
    networkResponse[23] = 'D';
    networkResponse[24] = 'A';
  } else {

    //ugasi klimu
    // klimaUkljucena = false;
    digitalWrite(KLIMA_PIN, LOW);
    tempDisplayOutput[18] = 'N';
    tempDisplayOutput[19] = 'E';
    networkResponse[23] = 'N';
    networkResponse[24] = 'E';
  }

  if (trenutnaFaza->ventilator && (random(4500, 5000) > trenutnaFaza->max_co2)) {

    //postavi bit za paljenje ventilatora
    // ventilatorUkljucen = true;
    digitalWrite(VENTILATOR_PIN, HIGH);
    vlazDisplayOutput[18] = 'D';
    vlazDisplayOutput[19] = 'A';
    networkResponse[26] = 'D';
    networkResponse[27] = 'A';
  } else {

    //ugasi ventilator
    // ventilatorUkljucen = false;
    digitalWrite(VENTILATOR_PIN, LOW);
    vlazDisplayOutput[18] = 'N';
    vlazDisplayOutput[19] = 'E';
    networkResponse[26] = 'N';
    networkResponse[27] = 'E';
  }

}

void displayAction() {

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(tempDisplayOutput);

  lcd.setCursor(0, 1);
  lcd.print(vlazDisplayOutput);

  lcd.setCursor(0, 2);
  lcd.print(co2DisplayOutput);

  lcd.setCursor(0, 3);
  if (trenutnaFaza->naziv == 0) {
    lcd.print(beginDisplayOutput);
  }
  else if (trenutnaFaza->naziv == 4) {
    lcd.print(errorDisplayOutput);
  }
  else {
    lcd.print(timeDisplayOutput);
  }

}

void networkAction() {

  // send data over network
  // if there's data available, read a packet
  //  Serial.println(networkResponse);
  //  if (Udp.parsePacket())
  //  {
  //    // Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
  //    // Serial.println("REQUEST");
  //    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  //    // char  replyBuffer[reply.length() + 1];
  //    // reply.toCharArray(replyBuffer, reply.length() + 1);
  //    Udp.write(networkResponse);
  //    Udp.endPacket();
  //    Serial.println(networkResponse);
  //  }
wdt_disable();
  lcd.setCursor(0, 3);
  lcd.print(networkDisplayOutput);
  
  networkResponse[17] = 48;

 int i = client.connect("adv-nemanjamarjan.rhcloud.com", 80);
 //int i = client.connect("192.168.1.2", 8080);
  
  
  if (i) {
    //Serial.println("connected");
    // Make a HTTP request:
    client.println("POST /rest/measurements/ HTTP/1.1");
    
    
  client.println("Host: adv-nemanjamarjan.rhcloud.com"); 
    //client.println("Host: 192.168.1.2");
    
    
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(47);
    client.println();
    client.println(networkResponse);

    lcd.setCursor(0, 3);
    lcd.print(networkOKDisplayOutput);
  }
  else {
    lcd.setCursor(0, 3);
    lcd.print(networkFailDisplayOutput);
 }
  if (client.connected()) {
    client.stop();
  }
wdt_enable(WDTO_8S);
}

void timeAction()
{

  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);

  char second = bcdToDec(Wire.read() & 0x3f);
  char minute = bcdToDec(Wire.read()) ;
  char hour = bcdToDec(Wire.read() & 0x7f) ;
  char date2 = bcdToDec(Wire.read());
  char date = bcdToDec(Wire.read());
  char month = bcdToDec(Wire.read());
  char year = bcdToDec(Wire.read()) ;

  //char networkResponse[50] = "1000456-00.0-00.0-0000-NE-NE-0-00002000000000";
  //char timeDisplayOutput[21]  =  "OK 00.00.2000 00:00 ";

  timeDisplayOutput[3] = networkResponse[31] = date / 10 + 48;
  timeDisplayOutput[4] = networkResponse[32] = date % 10 + 48;
  timeDisplayOutput[6] = networkResponse[33] = month / 10 + 48;
  timeDisplayOutput[7] = networkResponse[34] = month % 10 + 48;
  timeDisplayOutput[11] = networkResponse[37] = year / 10 + 48;
  timeDisplayOutput[12] = networkResponse[38] = year % 10 + 48;
  timeDisplayOutput[14] = networkResponse[39] = hour / 10 + 48;
  timeDisplayOutput[15] = networkResponse[40] = hour % 10 + 48;
  timeDisplayOutput[17] = networkResponse[41] = minute / 10 + 48;
  timeDisplayOutput[18] = networkResponse[42] = minute % 10 + 48;
  networkResponse[43] = second / 10 + 48;
  networkResponse[44] = second % 10 + 48;

}

//
//void fileAction() {
////  String fileName = "LOG.TXT";
////  Serial.println("FILE");
////  car  fileNameBuffer[fileName.length() + 1];
////  fileName.toCharArray(fileNameBuffer, fileName.length() + 1);
//
//  File dataFile = SD.open(fileName, FILE_WRITE);
//  if (dataFile) {
//    dataFile.println(networkResponse);
//    Serial.println("FILE");
//    Serial.println(networkResponse);
//    dataFile.close();
//  }
//}


// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
  return ( (val / 10 * 16) + (val % 10) );
}
// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
  return ( (val / 16 * 10) + (val % 16) );
}
void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte
                   dayOfMonth, byte month, byte year)
{
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(second)); // set seconds
  Wire.write(decToBcd(minute)); // set minutes
  Wire.write(decToBcd(hour)); // set hours
  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month)); // set month
  Wire.write(decToBcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}



