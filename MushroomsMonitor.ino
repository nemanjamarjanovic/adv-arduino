#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <avr/wdt.h>

#define DEBUG true

#define SERIAL_NUMBER 1000DEAD
#define DEFAULT_MAC { 0xDE, 0xAD, 0x10, 0x00, 0x04, 0x56 };
#define DEFAULT_PORT 8888
#define NETWORK_PERIOD 1000


EthernetClient client;
byte _MAC_ADDRESS[] = DEFAULT_MAC;

const IPAddress _IP_ADDRESS(192, 168, 1, 106);
const IPAddress _SERVER_IP_ADDRESS(192, 168, 1, 2);
const IPAddress _DNS(8, 8, 8, 8);
const IPAddress _SUBNET(255, 255, 255, 0);

const unsigned int _PORT = DEFAULT_PORT;

//195
char networkResponse[10] = "ID:553303";


unsigned long networkTime;

void setup() {

#ifdef DEBUG
  Serial.begin(115200);
#endif

#ifdef DEBUG
    Serial.println("hi!");
#endif

  //Ethernet.begin(_MAC_ADDRESS, _IP_ADDRESS, _DNS, _SERVER_IP_ADDRESS, _SUBNET);
  //Ethernet.begin(_MAC_ADDRESS, _IP_ADDRESS);
  //Udp.begin(DEFAULT_PORT);

  // start the Ethernet connection:
  if (Ethernet.begin(_MAC_ADDRESS) == 0) {
#ifdef DEBUG
    Serial.println("Failed to configure Ethernet using DHCP");
#endif
  }
#ifdef DEBUG
  else {
    printIPAddress();
  }
#endif

  wdt_enable(WDTO_8S);
  //wdt_enable(15);

  networkTime = millis();

}

void loop() {

  wdt_reset();

  if ( millis() - networkTime > NETWORK_PERIOD) {
    // odgovori na mrezni upit
    networkAction();
    networkTime = millis();
  }

}


void networkAction() {

 wdt_disable();

#ifdef DEBUG
  Serial.println("connecting...");
#endif

  int i = client.connect("192.168.0.11", 8080);
  if (i) {

#ifdef DEBUG
    Serial.println("connected");
#endif
    // Make a HTTP request:
    client.println("POST /iotserver/input/ HTTP/1.1");
    client.println("Host: 192.168.0.11");
    client.println("Connection: close");
    client.println("Content-Type: text/plain");
    client.print("Content-Length: ");
    client.println(sizeof(networkResponse));
    client.println();
    client.println(networkResponse);

#ifdef DEBUG
    Serial.println(networkResponse);
#endif
  }

 //if (client.connected()) {
    client.stop();
  //}
  wdt_enable(WDTO_8S);
}

#ifdef DEBUG
void printIPAddress()
{
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  }

  Serial.println();
}
#endif


