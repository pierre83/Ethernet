/*
  Results:
  --------
  ESP32 240MHz, SPI = 20, W5500
  =====================================
  50 bytes
  Duree: 233 ms
  PPS: 429.18454 ms
  100 bytes
  Duree: 235 ms
  PPS: 425.53192 ms
  400 bytes write
  Duree: 232 ms
  PPS: 431.0345 ms

  ESP8266 160Mhz SPI=40 + W5500
  =====================================
  50 bytes
  Duree: 298 ms
  PPS: 335.57047 ms
  100 bytes
  Duree: 332 ms
  PPS: 301.20483 ms
  400 bytes write
  Duree: 317 ms
  PPS: 315.45743 ms

  Mega 2560 + W5500
  =====================================
  50 bytes
  Duree: 396 ms
  PPS: 252.52525 ms
  100 bytes
  Duree: 399 ms
  PPS: 250.62657 ms
  400 bytes write
  Duree: 455 ms
  PPS: 219.78021 ms

  Original:
  50 bytes
  Duree: 416 ms
  PPS: 240.38461 ms
  100 bytes:
  Duree: 431 ms
  PPS: 232.01855 ms
  400 bytes write
  Duree: 486 ms
  PPS: 205.76132 ms
  
  UNO + W5100
  =====================================
  Pierre
  50 bytes
  Duree: 410 ms
  PPS: 243.90244 ms
  100 bytes
  Duree: 456 ms
  PPS: 219.29825 ms
  400 bytes write
  Duree: 780 ms
  PPS: 128.20512 ms
  Original:
  50 bytes
  Duree: 584 ms
  PPS: 171.23288 ms
  100 bytes:
  Duree: 645 ms
  PPS: 155.03876 ms
  400 bytes write
  Duree: 976 ms
  PPS: 102.459015 ms

  DUE + W5500
  =====================================
  Pierre
  50 bytes
  Duree: 238 ms
  PPS: 420.16806 ms
  100 bytes
  Duree: 247 ms
  PPS: 404.8583 ms
  400 bytes write
  Duree: 240 ms
  PPS: 416.66666 ms
  Original:
  50 bytes
  Duree: 336 ms
  PPS: 297.61905 ms
  100 bytes:
  Duree: 333 ms
  PPS: 300.3003 ms
  400 bytes write
  Duree: 429 ms
  PPS: 233.10023 ms
  ===================================


  Processing:

  import processing.net.*;

  Client c;
  String data;
  int N = 100;

  void setup() {
  int deb=millis();
  for (int i = 0; i< N; i++ ) {
    c = new Client(this, "192.168.0.56", 80);  // Connect to server on port 80

    c.write("GET /fxx.txt HTTP/1.0\n\n");  // Use the HTTP "GET" command to ask for a webpage
    while ( c.available() == 0 ) {
    }
    while (c.available() > 0) {    // If there's incoming data from the client...
      data = c.readString();   // ...then grab it
    }
  }
  delay(2);
  c.stop();
  int fin=millis();
  print("Duree: ");
  print(fin-deb);
  println(" ms");
  print("PPS: ");
  print(float(N*1000)/(fin-deb));
  println(" ms");
  }

  void draw()
  {
  }
*/
