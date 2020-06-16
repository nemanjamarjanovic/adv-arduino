#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <avr/wdt.h>

#define DS3231_I2C_ADDRESS 0x68
#define DUGME1_PIN 7
#define DUGME2_PIN 6
#define DUGME3_PIN 5
#define KLIMA_PIN 8
#define DHT21_PIN 14
#define DHTTYPE DHT21

#define BUTTON_PERIOD 1000
#define SENSOR_PERIOD 2000
#define DISPLAY_PERIOD 1000
#define CONTROL_PERIOD 60000
#define NETWORK_PERIOD 30000
#define TIME_PERIOD 1000

struct FAZA {
  unsigned int min_temperatura;
  unsigned int max_temperatura;
  boolean ventilator;
  boolean klima;
  char naziv;
};

DHT dht(DHT21_PIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
EthernetClient client;

char tempDisplayOutput[21]  =  "Temp: 00,0C Klima:NE";
char vlazDisplayOutput[21]  =  "Vlaz: 00,0%  Vent:NE";
char co2DisplayOutput[21]   =  "Mreza:        MOD: 0";
char timeDisplayOutput[21]  =  "OK 00.00.2000 00:00 ";
const char beginDisplayOutput[21] =  "Izaberi mod rada!   ";
const char errorDisplayOutput[21] =  "Nedozvoljeno stanje!";
const char networkDisplayOutput[21] =  "Slanje podataka.....";
const char networkOKDisplayOutput[21] =  "Slanje podataka...OK";
const char networkFailDisplayOutput[21] =  "Slanje nije uspjelo!";

// SERIAL_NUMBER-TEMPERATURE-HUMIDITY-CO2-AC-VENT-MOD-DATE-MONTH-YEAR-HOUR-MINUTE-SECOND
char networkResponse[46] = "AUN1BK01-00.0-00.00000-NE-NE-0-00002000000000";

FAZA pocetnaFaza      = {0, 0,  false, false, 0};
FAZA fazaPrva         = {200, 220,  true, true, 1};
FAZA fazaDruga        = {140, 180,  true, true, 2};
FAZA fazaTreca        = {160, 180, true, true, 3};
FAZA nedozvoljenaFaza = {0, 0,  false, false, 4};
FAZA* trenutnaFaza;

unsigned long buttonTime, sensorTime, controlTime, displayTime, networkTime, timeTime;

void setup() {

  pinMode(DUGME1_PIN, INPUT);
  pinMode(DUGME2_PIN, INPUT);
  pinMode(DUGME3_PIN, INPUT);
  pinMode(KLIMA_PIN, OUTPUT);

  lcd.begin(20, 4);
  lcd.backlight();

  byte _MAC_ADDRESS[] =  { 0xDE, 0xAD, 0x10, 0x00, 0x04, 0x56 };
  const IPAddress _IP_ADDRESS(192, 168, 1, 106);
  const IPAddress _GATEWAY(192, 168, 1, 1);
  const IPAddress _DNS(8, 8, 8, 8);
  const IPAddress _SUBNET(255, 255, 255, 0);
  Ethernet.begin(_MAC_ADDRESS, _IP_ADDRESS, _DNS, _GATEWAY, _SUBNET);

  dht.begin();
  Wire.begin();
  wdt_enable(WDTO_8S);

  trenutnaFaza = &pocetnaFaza;

  buttonTime = millis();
  sensorTime = millis();
  controlTime = millis();
  displayTime = millis();
  networkTime = millis();
  timeTime = millis();

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

  unsigned int trenutnaTemperatura = dht.readTemperature() * 10 - 13;
  unsigned int trenutnaVlaznost = dht.readHumidity() * 10;

  tempDisplayOutput[6] = networkResponse[9] = trenutnaTemperatura / 100 + 48;
  tempDisplayOutput[7] = networkResponse[10] = (trenutnaTemperatura % 100) / 10 + 48;
  tempDisplayOutput[9] = networkResponse[12] = trenutnaTemperatura % 10 + 48;

  vlazDisplayOutput[6] = networkResponse[14] = trenutnaVlaznost / 100 + 48;
  vlazDisplayOutput[7] = networkResponse[15] = (trenutnaVlaznost % 100) / 10 + 48;
  vlazDisplayOutput[9] = networkResponse[17] = trenutnaVlaznost % 10 + 48;

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

  wdt_disable();

  lcd.setCursor(0, 3);
  lcd.print(networkDisplayOutput);

  int i = client.connect("adv-nemanjamarjan.rhcloud.com", 80);

  if (i) {
    client.println("POST /rest/measurements/ HTTP/1.1");
    client.println("Host: adv-nemanjamarjan.rhcloud.com");
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(47);
    client.println();
    client.println(networkResponse);

    lcd.setCursor(0, 3);
    lcd.print(networkOKDisplayOutput);
    co2DisplayOutput[7] = 'D';
    co2DisplayOutput[8] = 'A';
  }
  else {
    lcd.setCursor(0, 3);
    lcd.print(networkFailDisplayOutput);
    co2DisplayOutput[7] = 'N';
    co2DisplayOutput[8] = 'E';
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
  // char date2 = bcdToDec(Wire.read());
  char date = bcdToDec(Wire.read());
  char month = bcdToDec(Wire.read());
  char year = bcdToDec(Wire.read()) ;

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



