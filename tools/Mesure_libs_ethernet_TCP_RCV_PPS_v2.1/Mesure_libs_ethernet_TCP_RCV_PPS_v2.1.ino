/*
  Goal : connect to a web site and measure the time to retrieve a given file size.
  For local web site, TINY or miniweb can be used
  le fichier fxxx.txt doit être disponible

  W5500 -> ESP8266
  MISO  -> D6
  MOSI  -> D7
  SCK   -> D5
  CS    -> D2

  ESP32  ->   W5500
  GPIO23 <--> MOSI
  GPIO19 <--> MISO
  GPIO18 <--> SCLK
  GPIO5  <--> SCS

  Operations:
  CE sketch émule un client web qui demande un fichier de x octets à un serveur face à lui
  Cette demande se fait au serveur local(192.168.0.46), Free, Google. Le fichier correspondant doit être sur le serveur.
  Cette action dure N secondes et le nombre de fichiers recus est compté et transformé en transactions/seconde
  File of filesize = to FILESIZE must be available on the server's side
  The file must be labelled as fxx.txt or fxxx.txt where xx & xxx represent the filesize
*/


#include <SPI.h>
#include <Ethernet.h>

#include "..\tools\Ethernet_W5x00\Init_W5x00\Init_W5x00.ino"

//------------------------------------------------------------
// CHANGE AS DESIRED:
uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

IPAddress ip(192, 168, 0, 56);
IPAddress myDns(192, 168, 0, 254);

bool dhcp = 0;  // 0= fixed IP, 1 = DHCP

const int BUFFERSIZE = 300;
const float duree = 5.0;    // Durée en secondes

//------------------------------------------------------------

EthernetClient client;


#if defined (ESP8266)
const uint8_t CS = 4;   // ESP8266 CS -> D2 (GPIO04)
#elif defined (ESP32)
const uint8_t CS = 5;     // ESP32   CS -> D5 (GPIO05)
#elif defined (__AVR__) || defined (__SAM3X8E__)
const uint8_t CS = 10;  // AVR (default)
#endif

enum Direct {
  Local,
  Free,
  Google,
};
const uint8_t DIRECT = Local;

const uint8_t MAX_COUNT = 5;

char file[40], server[20];

// ****************************************************************
void setup()
{
  // Open serial communications
  Serial.begin(115200);

  Serial.println(F("Measure libraries Ethernet TCP RCV PPS\n"));

  Ethernet.init(CS);

  // Verify & Start the Ethernet connection:
#ifndef OLD
  Ethernet_init( dhcp, mac, ip, myDns);
#else
  if ( !dhcp ) {
    Ethernet.begin(mac, ip, myDns);
  } else {
    if ( !Ethernet.begin(mac) ) {
      Serial.println(F("Ethernet failed to start"));
      while (1);
    } else {
      Serial.println(F("Configuring Ethernet with DHCP"));
    }
  }
  Serial.print(F("\tIP address "));       Serial.println(Ethernet.localIP());
  Serial.print(F("\tIP gateway "));       Serial.println(Ethernet.gatewayIP());
  Serial.print(F("\tIP subnet mask "));   Serial.println(Ethernet.subnetMask());
  Serial.print(F("\tIP DNS "));           Serial.println(Ethernet.dnsServerIP());
#endif


  if ( DIRECT == Local ) strcpy (server, "192.168.0.46");
  else if ( DIRECT == Google ) strcpy (server, "www.google.com");
  else if ( DIRECT == Free ) strcpy (server, "pcasal.free.fr");

  Serial.println(F("Beginning of test"));
  int j = 50;
  char u[5];
  while ( j <= 250 ) {
    strcpy(file, "GET /f");
    itoa(j, u, 10);
    strcat(file, u);
    strcat ( file, ".txt HTTP/1.1\r\n");
    Serial.print(F("Requesting: f"));
    Serial.print(u);
    Serial.println(F(".txt\n"));
    for ( uint8_t i = 0; i < MAX_COUNT; i++) {
      connection(j);
    }
    j += 100;
  }
  Serial.println(F("End of test"));
}

// ****************************************************************
uint32_t reception()
{
  uint32_t quantite = 0;
  int len = 0;
  uint8_t buffer[BUFFERSIZE];
  while ( len == 0) {
    len = client.available();
  }
  while ( len > 0 ) {
    if (len > BUFFERSIZE ) len = BUFFERSIZE;
    client.read(buffer, len);
    quantite += len;
    len = client.available();
  }
  return quantite;
}

// ****************************************************************
void connection(int j)
{
  Serial.print(F("connecting to "));  Serial.println(server);
  uint32_t count = 0, failed = 0, total = 0;
  uint32_t fin = millis() + duree * 1000;
  while ( millis() < fin ) {
    if (client.connect(server, 80) == 1) {
      client.print(file);
      client.println("Connection: close\r\n\r\n");
      total += reception();
      client.stop();
      count++;
    } else {
      failed++;
    }
  }
  calcul(count, failed, total, j);
}

// ****************************************************************
void calcul( uint16_t count, uint16_t failed, uint32_t total, int j)
{
  static float pps_max = 0, pps_min = 9999, speed_max = 0;
  static uint16_t fail = 0, packetCount = 0;
  static uint8_t combien = 0;
  float pps = count / duree;
  float speed = total / duree;
  fail += failed;
  packetCount += count;
  Serial.print(F("Rate\t"));    Serial.print(pps, 0);    Serial.println(F(" transactions/second"));
  Serial.print(F("Speed\t"));   Serial.print(speed, 0);  Serial.println(F(" bytes/second"));
  Serial.print(F("OK\t"));      Serial.println(count);
  Serial.print(F("Fail\t"));    Serial.println(failed);
  if (pps_max < pps ) pps_max = pps;
  if (pps_min > pps ) pps_min = pps;
  if (speed_max < speed ) speed_max = speed;
  combien++;
  if ( combien == MAX_COUNT) {
    Serial.print(F("\nSize: "));      Serial.print(j);              Serial.println(F(" Bytes"));
    Serial.print(F("Max rate: "));    Serial.print(speed_max, 0);   Serial.println(F(" Bytes/sec"));
    Serial.print(F("PPS max : "));    Serial.print(pps_max, 1 );    Serial.println(F(" pps"));
    Serial.print(F("PPS min : "));    Serial.print(pps_min, 1 );    Serial.println(F(" pps"));
    Serial.print(F("PKT ok  : "));    Serial.println(packetCount);
    Serial.print(F("PKT fail: "));    Serial.println(fail);
    Serial.println();
    pps_max = 0, pps_min = 9999, speed_max = 0;
    fail = 0, packetCount = 0, combien = 0;
  }
}

// ****************************************************************
void loop()
{}


// ****************************************************************
