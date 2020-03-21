/*
  Goal : connect to a web site and measure the time to retrieve a given file size.
  For local web site, TINY or miniweb can be used

  Operations:
  CE sketch émule un client web qui demande un fichier de x octets à un serveur face à lui
  Cette demande se fait au serveur local(192.168.0.46), Free, Google. Le fichier correspondant doit être sur le serveur.
  Cette action dure N secondes et le nombre de fichiers recus est compté et transformé en transactions/seconde
  File of filesize = to FILESIZE must be available on the server's side
  The file must be labelled as fxx.txt or fxxx.txt where xx & xxx represent the filesize
*/

//#define OLD  // Used to compare with v2.0.0 library

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

bool dhcp = 0;  // 0= fixed IP, 1 = DHCP

const int BUFFERSIZE = 300;
const float N = 5.0;    // Durée en secondes
const uint8_t FILESIZE = 50;  // => le fichier fxxx.txt doit être disponible
//------------------------------------------------------------

EthernetClient client;

enum Direct {
  Local,
  Free,
  Google,
};
const uint8_t DIRECT = Local;

const uint8_t MAX_COUNT = 5;
uint32_t quantite = 0;
uint8_t len1, len2;

char file[80];
char ending[] = "Connection: close\r\n\r\n";

// ****************************************************************
void setup()
{
  // Open serial communications
  Serial.begin(115200);

Serial.println(F("Measure libraries Ethernet TCP RCV PPS\n"));

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

  char u[4];
  strcpy(file, "GET /f");
  itoa(FILESIZE, u, 10);
  strcat(file, u);
  strcat ( file, ".txt HTTP/1.1\r\n");
  len1 = strlen(ending);
  len2 = strlen(file);

  Serial.println(F("Beginning of test"));
  Serial.print(F("Message : "));
  Serial.println(file);
  for ( uint8_t i = 0; i < MAX_COUNT; i++) {
    connect();
  }
  Serial.println(F("End of test"));
}

// ****************************************************************
void reception()
{
  int len = 0;
  uint8_t buffer[BUFFERSIZE];
  while ( len == 0) {
    len = client.available();
  }
  if ( len ) {
    quantite += len;
    while ( len ) {
      if (len > BUFFERSIZE ) client.read(buffer, BUFFERSIZE);
      else client.read(buffer, len);
      //client.read(buffer, BUFFERSIZE);
      len = client.available();
    }
    client.stop();
  }
}

// ****************************************************************
void connect()
{
  quantite = 0;
  char server[20];
  if ( DIRECT == Local ) strcpy (server, "192.168.0.46");
  else if ( DIRECT == Google ) strcpy (server, "www.google.com");
  else if ( DIRECT == Free ) strcpy (server, "pcasal.free.fr");
  Serial.print(F("\nconnecting to "));
  Serial.print(server);
  Serial.println(F("..."));
  uint16_t count = 0, failed = 0;
  uint32_t debut = millis();
  while (millis() - debut  < N * 1000 ) {
    if (client.connect(server, 80) == 1) {
      //Serial.println(F("Connected !"));
      client.write(file, len2);
      client.write(ending, len1);
      reception();
      count++;
    } else {
      failed++;
    }
  }
  calcul(count, failed);
}

// ****************************************************************
void calcul( uint16_t count, uint16_t failed)
{
  static float mesure_max = 0, mesure_min = 1000;
  static uint16_t fail = 0, count3;
  static uint8_t combien = 0;
  static float speed_max = 0;
  float count2 = count / N;
  float quantite2 = quantite / N;
  fail += failed;
  count3 += count;
  Serial.print(F("Rate : "));   Serial.print(count2, 0);     Serial.println(F(" transactions/second"));
  Serial.print(F("Speed: "));   Serial.print(quantite2, 0);  Serial.println(F(" bytes/second"));
  Serial.print(F("OK   : "));  Serial.println(count);
  Serial.print(F("Fail : "));  Serial.println(failed);
  if (mesure_max < count2 ) mesure_max = count2;
  if (mesure_min > count2 ) mesure_min = count2;
  if (speed_max < quantite2 ) speed_max = quantite2;
  combien++;
  if ( combien == MAX_COUNT) {
    Serial.print(F("Max rate: "));    Serial.print(speed_max, 0);     Serial.println(F(" Bytes/sec"));
    Serial.print(F("PPS maxi: "));    Serial.print(mesure_max, 1 );   Serial.println(F(" pps"));
    Serial.print(F("PPS mini: "));    Serial.print(mesure_min, 1 );   Serial.println(F(" pps"));
    Serial.print(F("PKT ok  : "));    Serial.println(count3);
    Serial.print(F("PKT fail: "));    Serial.println(fail);
  }
}

// ****************************************************************
void loop()
{}


// ****************************************************************
