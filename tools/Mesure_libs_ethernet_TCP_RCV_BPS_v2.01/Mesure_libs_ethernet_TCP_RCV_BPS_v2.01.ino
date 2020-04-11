/*
  Web client

  Connect to a web site and measure the time to retrieve a given file size

  Connections:
  ESP32  --   W5500 -- ESP8266
  GPIO19 ->   MISO  -> D6
  GPIO23 ->   MOSI  -> D7
  GPIO18 ->   SCK   -> D5
  GPIO5  ->   CS    -> D2

*/


#include <SPI.h>
#include <Ethernet.h>


#include "..\tools\Ethernet_W5x00\Init_W5x00\Init_W5x00.ino"


//------------------------------------------------------------
// CHANGE AS DESIRED:
uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

IPAddress ip(192, 168, 0, 56);
IPAddress myDns(192, 168, 0, 254);

bool dhcp = 1;  // 0= fixed IP, 1 = DHCP

const int BUFFERSIZE = 300;
//------------------------------------------------------------

EthernetClient client;

#if defined (ESP8266)
const uint8_t CS = 4;   // ESP8266 CS -> D2 (GPIO04)
#elif defined (ESP32)
const uint8_t CS = 5;     // ESP32   CS -> D5 (GPIO05)
#elif defined (__AVR__)
const uint8_t CS = 10;  // AVR (default)
#endif

enum DIRECTions {
  Local,
  Google,
  Free,
};
const uint8_t DIRECT = Local;

// Variables to measure the speed
unsigned long debut, fin;
unsigned long beginMicros, endMicros;
unsigned long byteCount = 0;
bool wasConnected = false;
uint8_t count = 0, failed = 0;
uint8_t MAX_COUNT = 4;
float mesure_max = 0, mesure_min = 9999, mesure_moy = 0;

// ****************************************************************
void setup()
{
  // Open serial communications
  Serial.begin(115200);

  Serial.println(F("Measure libraries Ethernet TCP RCV BPS\n"));

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
    }
  }
  Serial.print(F("\tIP address "));       Serial.println(Ethernet.localIP());
  Serial.print(F("\tIP gateway "));       Serial.println(Ethernet.gatewayIP());
  Serial.print(F("\tIP subnet mask "));   Serial.println(Ethernet.subnetMask());
  Serial.print(F("\tIP DNS "));           Serial.println(Ethernet.dnsServerIP());
#endif

  delay(1000);
  debut = millis();
  connect();
}

// ****************************************************************
void loop()
{
  int len = client.available();
  if (len > 0) {
    uint8_t  buffer[BUFFERSIZE];
    if (len > BUFFERSIZE) len = BUFFERSIZE;
    client.read(buffer, len);
    byteCount += len;
  }

  // if the server's disconnected, stop the client:
  if (!client.connected() ) {
    endMicros = micros();
    if ( wasConnected ) {
      Serial.println(F("disconnecting."));
      client.stop();
      uint32_t duree = endMicros - beginMicros;
      calcul(duree, byteCount);
    }
    wasConnected = false;
    if ( count > MAX_COUNT ) {
      fin = millis();
      Serial.print(F("\n\n\tMax speed: "));   Serial.print(mesure_max);           Serial.println(F(" Kb/second"));
      Serial.print(F("\tMin speed: "));       Serial.print(mesure_min);           Serial.println(F(" Kb/second"));
      Serial.print(F("\tMean speed: "));      Serial.print(mesure_moy / count);   Serial.println(F(" Kb/second"));
      Serial.print(F("\tIterations: "));      Serial.print(count);
      Serial.print(F("/"));                   Serial.println(failed);
      Serial.print(F("Duree: "));             Serial.print(fin - debut);          Serial.println(F(" ms"));

      Serial.println(F("\nSocket Errors:"));
      Serial.print(F("Init\t"));  Serial.println(Ethernet.getSocketError(0));
      Serial.print(F("Connect\t"));  Serial.println(Ethernet.getSocketError(1));
      Serial.print(F("State\t"));  Serial.println(Ethernet.getSocketError(2));
      while (true) {
        delay(1);      // do nothing forevermore:
      }
    } else {
      Serial.print(F("Little delay "));
      for ( uint8_t i = 0; i < 20; i++ ) {
        Serial.print(F("."));
        delay (100);
      }
      connect();
    }
  }
}

// ****************************************************************
void calcul( uint32_t tempsUs, uint32_t byteCount)
{
  float tempsMs = (float)tempsUs / 1000.0;
  float rate = (float)byteCount / tempsMs;
  Serial.print(F("Received "));            Serial.print(byteCount);
  Serial.print(F(" bytes in "));           Serial.print(tempsMs, 3);
  Serial.print(F(" millisecondes, rate = "));   Serial.print(rate);
  Serial.println(F(" kbytes/second"));
  if (mesure_max < rate ) mesure_max = rate;
  if (mesure_min > rate && rate > 0 ) mesure_min = rate;
  mesure_moy += rate;
}

// ****************************************************************
void connect()
{
  char server[20];
  Serial.print(F("\nconnecting to "));
  if ( DIRECT == Local ) strcpy (server, "192.168.0.46");
  else if ( DIRECT == Google ) strcpy (server, "www.google.com");
  else if ( DIRECT == Free ) strcpy (server, "pcasal.free.fr");
  Serial.print(server);
  Serial.println(F("..."));
  int ret = client.connect(server, 80);
  if (ret == 1) {
    wasConnected = true;
    Serial.print(F("connected to "));
    Serial.println(client.remoteIP());
    if ( DIRECT == Local ) {      // LOCAL
      //client.println("GET /N.txt HTTP/1.1");
      Serial.println(F("File size: 243427 bytes"));
      client.println("GET /mois-0.jpg HTTP/1.1");
    }
    else if ( DIRECT == Google ) {      // Google
      client.println("GET /search?q=arduino HTTP/1.1");
      client.println("Host: www.google.com");
    }
    else if ( DIRECT == Free ) {      // FREE
      client.println("GET /meteo/mois-0.jpg HTTP/1.1");
    }
    client.println("Connection: close");
    client.println();
    count++;
  } else {
    // if you didn't get a connection to the server:
    Serial.print(F("connection failed: "));
    Serial.println(ret);
    wasConnected = false;
    failed++;
  }

  byteCount = 0;
  beginMicros = micros();
}

// ****************************************************************
