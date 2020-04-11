/*
  Test Ethernet library performance
  Send ITERATIONS 'times' a buffer (SENDSIZE bytes) to a client requesting the M file

  The send operation can be done with 4 methods (print'", print(buffer, print(F and write(buffer

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

bool dhcp = 1;  // 0= fixed IP, 1 = DHCP

const int BUFFERSIZE = 500;
const int SENDSIZE = 400;
//------------------------------------------------------------


#if defined (ESP8266)
const uint8_t CS = 4;   // ESP8266 CS -> D2 (GPIO04)
#elif defined (ESP32)
const uint8_t CS = 5;     // ESP32   CS -> D5 (GPIO05)
#elif defined (__AVR__)
const uint8_t CS = 10;  // AVR (default)
#endif


const uint8_t HTTP_SERVER_PORT = 80;
EthernetServer http_Server( HTTP_SERVER_PORT );
EthernetClient http_client;

enum modes {
  PrintB,
  PrintF,
  PrintS,
  Write,
};
uint8_t mode = PrintB;
// Variables to measure the speed
uint32_t beginMicros, endMicros;
uint32_t byteCount = 0;

const int ITERATIONS = 10;
uint8_t count = 0;
const uint8_t MAX_COUNT = 5;
float mesure_max = 0, mesure_min = 9999, mesure_moy = 0;
char buffer[SENDSIZE + 1];

// ****************************************************************
void setup()
{
  // Open serial communications
  Serial.begin(115200);

  Ethernet.init(CS);

  Serial.println(F("Measure libraries Ethernet TCP XMT BPS\n"));
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

  // Start the HTTP server
  http_Server.begin();

  // Build the buffer
  uint16_t i = 0;
  uint8_t v = '1', w = 'A';
  while (i < SENDSIZE - 3) {
    buffer[i] = v;
    v++;
    i++;
    if ( v > '9' ) {
      v = '1';
      buffer[i] = w;
      w++;
      i++;
      if ( w > 'Z' ) w = 'A';
    }
  }
  buffer[i] = 0;
  Serial.print(F("Buffer: "));
  Serial.println(buffer);
  strcat (buffer, "\r\n\n");
  Serial.print(F("Size: "));
  Serial.println(strlen(buffer));

  Serial.print(F("Ready to accept incoming request on: http://"));
  Serial.print(Ethernet.localIP());
  Serial.println(F("/M.txt"));
}

// ****************************************************************
void loop()
{
  http_client = http_Server.available();
  if ( http_client ) {
    http_client_gestion();
  }

  if ( count >= MAX_COUNT ) {
    Serial.print(F("\n\n\tMax speed: "));   Serial.print(mesure_max);     Serial.println(F(" kbytes/second"));
    Serial.print(F("\tMin speed: "));       Serial.print(mesure_min);     Serial.println(F(" kbytes/second"));
    Serial.print(F("\tMean speed: "));      Serial.print(mesure_moy / (count));
    Serial.print(F("\tIterations: "));      Serial.println(count);
    while (true) {
      delay(1);   // stop here
    }
  }
}

// ****************************************************************
void send_buffer()
{
  count++;
  uint16_t longueur = strlen(buffer);
  Serial.println(F("Sending !"));
  beginMicros = micros();

  for ( int i = 0; i < ITERATIONS; i++) {

    //http_client.println("0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[+]^_`abcdefghijklmnopqrstuvwxy0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[+]^_`abcdefghijklmnopqrstuvwxy0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[+]^_`abc"));
    //http_client.println(F("0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[+]^_`abcdefghijklmnopqrstuvwxy0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[+]^_`abcdefghijklmnopqrstuvwxy0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[+]^_`abc"));
    //http_client.print(buffer);
    http_client.write(buffer, longueur);
  }
  endMicros = micros();
  byteCount = (uint32_t)ITERATIONS * longueur;
  calcul(endMicros - beginMicros, byteCount);
}

// ****************************************************************
void http_client_gestion()
{
  char fin[] = "HTTP/1.1 200 OK\r\nConnnection: close\r\n\r\n";
  int len = strlen(fin);
  uint8_t sortir = 0, http_buffer_in = 0;
  boolean currentLineIsBlank = true;
  const uint8_t HTTP_BUFFER_SIZE = 110;
  const uint8_t HTTP_CARACTERES_IN_MAXI = HTTP_BUFFER_SIZE - 1;
  const uint16_t HTTP_TIMEOUT = 200;
  char http_buffer[HTTP_BUFFER_SIZE];

  uint32_t stop_waiting_at = millis() + HTTP_TIMEOUT;
  while ( !sortir ) {
    if ( http_client.available() ) {
      char C10 = http_client.read();
      if ( http_buffer_in < HTTP_CARACTERES_IN_MAXI ) {
        http_buffer[http_buffer_in] = C10;
        http_buffer_in++;
      }
      if ( C10 == '\n' && currentLineIsBlank) sortir = 5;   // La dernière ligne est vide et contient seulement \n
      if ( C10 == '\n') currentLineIsBlank = true;          // every line of text received from the client ends with \r\n
      else if ( C10 != '\r') currentLineIsBlank = false;    // a text character was received from client
    }
    else if ( !http_client.connected() ) sortir = 1;
    else if ( millis() > stop_waiting_at ) sortir = 3;    // Timeout
  }
  http_buffer[http_buffer_in] = 0;
  //Serial.println(http_buffer);
  if ( sortir == 5 ) {           // Normalement tout est ok
    //http_client.println(F("HTTP/1.1 200 OK"));
    // http_client.println(F("Connnection: close"));
    //http_client.println();
    http_client.write(fin, len);
    if ( http_buffer[5] == 'M' ) send_buffer();
  }
  http_client.flush();
  delay(1);
  http_client.stop();

}

// ****************************************************************
void calcul( uint32_t tempsUs, uint32_t byteCount)
{
  float tempsMs = (float)tempsUs / 1000.0;
  float rate = (float)byteCount / tempsMs;
  Serial.print(F("Sent "));          Serial.print(byteCount);
  Serial.print(F(" bytes in "));     Serial.print(tempsMs, 3);
  Serial.print(F(" millisecondes, rate = "));  Serial.print(rate);
  Serial.println(F(" kbytes/second"));
  if (mesure_max < rate ) mesure_max = rate;
  if (mesure_min > rate ) mesure_min = rate;
  mesure_moy += rate;
}


// ****************************************************************
