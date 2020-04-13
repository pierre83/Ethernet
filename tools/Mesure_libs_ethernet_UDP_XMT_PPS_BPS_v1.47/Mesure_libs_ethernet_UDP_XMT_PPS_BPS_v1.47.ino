//
// This sketch send packet of variable length to another host running UDP
// it it used with a Processing sketch on the other side.

//#define OLD  // Used to compare with v2.0.0 library

/*
   Connections:
  ESP32  --   W5500 -- ESP8266
  GPIO19 ->   MISO  -> D6
  GPIO23 ->   MOSI  -> D7
  GPIO18 ->   SCK   -> D5
  GPIO5  ->   CS    -> D2
*/

#include <SPI.h>
#include <Ethernet.h>

#ifndef OLD
#include "..\tools\Ethernet_W5x00\Init_W5x00\Init_W5x00.ino"
#endif

//------------------------------------------------------------
// CHANGE AS DESIRED:
uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

IPAddress ip(192, 168, 0, 56);
IPAddress myDns(192, 168, 0, 254);

bool dhcp = 1;  // 0= fixed IP, 1 = DHCP

// Parameters must be IDENTICAL on the Processing side:
IPAddress receiver(192, 168, 0, 46);
const uint16_t UdpPort = 6000;        // local port to listen on
const uint16_t ITERATIONS = 500;      // Number of loops of test
//------------------------------------------------------------

#if defined (ESP8266)
const uint8_t CS = 4;   // ESP8266 CS -> D2 (GPIO04)
#elif defined (ESP32)
const uint8_t CS = 5;     // ESP32   CS -> D5 (GPIO05)
#elif defined (__AVR__)
const uint8_t CS = 10;  // AVR (default)
#endif

const uint8_t lng[] = { 10, 50, 100, 250};    // Length of the bufs

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

// ****************************************************************
void setup()
{
  // Open serial communications
  Serial.begin(115200);

  Ethernet.init(CS);

  // Verify & Start the Ethernet connection:
#ifndef OLD
  Ethernet_init( dhcp, mac, ip, myDns);
#else
  if ( Ethernet.begin(mac) ) {
    Serial.print(F("\tIP address "));       Serial.println(Ethernet.localIP());
    Serial.print(F("\tIP gateway "));       Serial.println(Ethernet.gatewayIP());
    Serial.print(F("\tIP subnet mask "));   Serial.println(Ethernet.subnetMask());
    Serial.print(F("\tIP DNS "));           Serial.println(Ethernet.dnsServerIP());
  }
#endif


  for (uint8_t i = 0; i < 4; i++) {
    uint8_t j = lng[i];
    char buf[j + 1];
    // Build the buf
    uint8_t k = 0, v = '1', w = 'A';
    while (k < j ) {
      buf[k] = v;
      v++;
      if ( v > '9' ) {
        k++;
        v = '1';
        buf[k] = w;
        w++;
        if ( w > 'Z' ) w = 'A';
      }
      k++;
    }
    buf[j] = 0;
    Serial.println(F("\nBeginning of test"));
    Serial.print(F("Buffer= "));    Serial.println(buf);
    Serial.print(F("Long= "));    Serial.print(j);    Serial.println(F(" bytes"));
    // Start EthernetUDP socket, listening at local port udpPort
    if ( Udp.begin(UdpPort) != 1 ) {
      Serial.println(F("\nPb UDP start !"));
    }
    udp_send(buf, j);
  }
  Serial.println(F("\nEnd of test"));
}

// ****************************************************************
void loop()
{}

// ****************************************************************
void udp_send(const char * buf, const uint8_t len)
{
  uint16_t count = 0;
  uint32_t totalBytesSent = 0, badPackets = 0;
  uint32_t deb = micros();
  for ( uint16_t i = 0; i < ITERATIONS; i++ ) {
    if ( Udp.beginPacket(receiver, UdpPort) == 1 ) {
      uint16_t bytesSent = Udp.write(buf, len);
      if ( Udp.endPacket() == 1 ) {
        totalBytesSent += bytesSent;
        count++;
      } else {
        badPackets++;
      }
    }
  }
  uint32_t fin = micros();
  Udp.stop();
  float tempsMs = (fin - deb) / 1000.0;
  float transfertSpeed = (float)totalBytesSent / tempsMs;
  float pps = count / (tempsMs / 1000.0);
  float goodpackets = (count * 100.0) / ITERATIONS;
  float bytespackets = (totalBytesSent * 100.0) / ((uint32_t)ITERATIONS * len);
  Serial.print(F("Temps: "));      Serial.print(tempsMs, 2);          Serial.println(F(" ms"));
  Serial.print(F("Speed: "));      Serial.print(transfertSpeed, 2);   Serial.println(F(" Kb/s"));
  Serial.print(F("Rate : "));      Serial.print(pps, 2);              Serial.println(F(" packets/sec"));
  Serial.print(F("Good packets: "));    Serial.print(goodpackets);         Serial.println(F(" %"));
  Serial.print(F("Bad packets : "));    Serial.println(badPackets);
  //Serial.print(F(" / "));          Serial.print(ITERATIONS);
  Serial.print(F("Bytes : "));      Serial.print(bytespackets);         Serial.println(F(" %"));
  //Serial.print(F(" / "));          Serial.print((uint32_t)ITERATIONS * len);   Serial.println(F(" bytes"));
}

// ****************************************************************
