/*
  Test Ethernet library performance
  Works as web server delivering a packet (of xx bytes, to choose) each time it is asked
  The Processing3 client ask 100 times for the same file, time is measured and
  a packet per second speed is given.

  The send operation can be done with 4 methods: print("..."), print(buffer), print(F("...")) and write(buffer,len)

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
const int SENDSIZE = 50;
//------------------------------------------------------------

const uint8_t HTTP_SERVER_PORT = 80;
EthernetServer http_server( HTTP_SERVER_PORT );
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
float mesure_max = 0, mesure_min = 1000, mesure_moy = 0;
char buffer[SENDSIZE + 1];
const uint16_t longueur = SENDSIZE;

// ****************************************************************
void setup()
{
  // Open serial communications
  Serial.begin(115200);

  Serial.println(F("Mesure Ethernet library TCP XMT packet per second(PPS)"));

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
  http_server.begin();

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
  char u[5];
  Serial.print(F("Buffer: "));
  Serial.println(buffer);
  strcat (buffer, "\r\n\n");
  Serial.print(F("Size: "));
  Serial.println(strlen(buffer));

  Serial.print(F("Ready to accept incoming request on: http://"));
  Serial.print(Ethernet.localIP());
  Serial.print(F("/f"));
  itoa (SENDSIZE, u, 10);
  Serial.print(u);
  Serial.println(F(".txt"));
}

// ****************************************************************
void loop()
{
  http_client = http_server.available();
  if ( http_client ) {
    http_client_gestion();
  }
}

// ****************************************************************
void send_buffer()
{
  count++;
  //uint16_t longueur = strlen(buffer);
  //http_client.println("0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[+]^_`abcdefghijklmnopqrstuvwxy0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[+]^_`abcdefghijklmnopqrstuvwxy0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[+]^_`abc"));
  //http_client.println(F("0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[+]^_`abcdefghijklmnopqrstuvwxy0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[+]^_`abcdefghijklmnopqrstuvwxy0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[+]^_`abc"));
  //http_client.print(buffer);
  http_client.write(buffer, longueur);
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
    http_client.write(fin, len);
    if ( http_buffer[5] == 'f' ) send_buffer();
  }
  http_client.flush();
  delay(1);
  http_client.stop();

}

// ****************************************************************
