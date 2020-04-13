/*
  Results:
  --------
  ESP32 240MHz, SPI = 20, W5500
  =====================================
  Longueur 50 bytes
  Duree: 138 ms
  PPS: 362.31885 
  Longueur 150 bytes
  Duree: 109 ms
  PPS: 458.7156 
  Longueur 400 bytes
  Duree: 125 ms
  PPS: 400.0 

  ESP8266 160Mhz SPI=40 + W5500
  =====================================
  50 bytes
  Duree: 298 ms
  PPS: 335.57047 
  100 bytes
  Duree: 332 ms
  PPS: 301.20483 
  400 bytes write
  Duree: 317 ms
  PPS: 315.45743 

  Mega 2560 + W5500
  =====================================
  Longueur 50 bytes
  Duree: 230 ms
  PPS: 217.39131 
  Longueur 150 bytes
  Duree: 227 ms
  PPS: 220.26431 s
  Longueur 400 bytes
  Duree: 253 ms
  PPS: 197.62846 

  Original:
  50 bytes
  Duree: 416 ms
  PPS: 240.38461 
  100 bytes:
  Duree: 431 ms
  PPS: 232.01855 
  400 bytes write
  Duree: 486 ms
  PPS: 205.76132 

  UNO + W5100
  =====================================
  Pierre
  50 bytes
  Duree: 410 ms
  PPS: 243.90244 
  100 bytes
  Duree: 456 ms
  PPS: 219.29825 
  400 bytes write
  Duree: 780 ms
  PPS: 128.20512 
  Original:
  50 bytes
  Duree: 584 ms
  PPS: 171.23288 
  100 bytes:
  Duree: 645 ms
  PPS: 155.03876 
  400 bytes write
  Duree: 976 ms
  PPS: 102.459015 

  DUE + W5500
  =====================================
  Pierre
  50 bytes
  Duree: 238 ms
  PPS: 420.16806 
  100 bytes
  Duree: 247 ms
  PPS: 404.8583 
  400 bytes write
  Duree: 240 ms
  PPS: 416.66666 
  Original:
  50 bytes
  Duree: 336 ms
  PPS: 297.61905 
  100 bytes:
  Duree: 333 ms
  PPS: 300.3003 s
  400 bytes write
  Duree: 429 ms
  PPS: 233.10023 
  ===================================


  Processing:

import processing.net.*;

Client c;
String data;
int N = 50;

void setup() {
  //println("Longueur 50 bytes");
  envoi("GET /f50.txt HTTP/1.0\n\n");
  //println("Longueur 150 bytes");
  envoi("GET /f150.txt HTTP/1.0\n\n");
  //println("Longueur 400 bytes");
  envoi("GET /f400.txt HTTP/1.0\n\n");
}

void draw()
{
}

void envoi(String file)
{
  int deb=millis();
  for (int i = 0; i< N; i++ ) {
    c = new Client(this, "192.168.0.56", 80);  // Connect to server on port 80
    c.write(file);  // Use the HTTP "GET" command to ask for a webpage
    while ( c.available() == 0 ) {
    }
    while (c.available() > 0) {    // If there's incoming data from the client...
      data = c.readString();   // ...then grab it
    }
    
  }
  delay(2);
  c.stop();
  int fin=millis();
  print("Longueur ");
  print(data.length());
  println(" bytes"); 
  print("Duree: ");
  print(fin-deb);
  println(" ms");
  print("PPS: ");
  print(float(N*1000)/(fin-deb));
  println(" packets/sec");
}

*/
