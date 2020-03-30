
  // You can use Ethernet.init(pin) to configure the CS pin
  //Ethernet.init(10);  // Most Arduino shields
  //Ethernet.init(5);   // MKR ETH shield
  //Ethernet.init(0);   // Teensy 2.0
  //Ethernet.init(20);  // Teensy++ 2.0
  //Ethernet.init(15);  // ESP8266 with Adafruit Featherwing Ethernet
  //Ethernet.init(33);  // ESP32 with Adafruit Featherwing Ethernet


void Ethernet_init(bool dhcp, uint8_t mac[], IPAddress ip, IPAddress myDns)
{
  // Check and start the Ethernet connection:
  int connecte = 0;  // 1= W5x00 initialized, 0 = not initialized, -1= no Ethernet card
  uint32_t deb, fin;
  if ( !dhcp ) {
    Serial.println(F("Configure using IP address instead of DHCP"));
    connecte = Ethernet.begin(mac, ip, myDns, myDns);	//hope myDns = myGateway
  } else {
    Serial.println(F("Initialize Ethernet with DHCP"));
	deb = millis();
    connecte = Ethernet.begin(mac);
	fin = millis();
	Serial.print(F("DHCP duration "));  Serial.print(fin - deb);  Serial.println("ms");
  }
  if (connecte == -1 ) {
    Serial.println(F("Failed to initialize W5x00 (no hardware"));
    while (true) {
      delay(1); // Stop here
    }
  }
  else if (connecte == 0 ) {
    Serial.println(F("DHCP not found, initialize Ethernet with provided IPs"));
    connecte = Ethernet.begin(mac, ip, myDns, myDns);  //hope myDns = myGateway
  }
  
  Serial.println(F("IP parameters:"));
  Serial.print(F("\tIP address  "));	Serial.println(Ethernet.localIP());
  Serial.print(F("\tIP gateway  "));	Serial.println(Ethernet.gatewayIP());
  Serial.print(F("\tSubnet mask "));	Serial.println(Ethernet.subnetMask());
  Serial.print(F("\tIP DNS      "));	Serial.println(Ethernet.dnsServerIP());

  // Check for Ethernet hardware
  Serial.println(F("Ethernet parameters:"));
  uint8_t hardwareType = Ethernet.hardwareStatus();
  Serial.print(F("\tBoard "));
  if ( hardwareType == EthernetNoHardware) {
    Serial.println(F("\tNo hardware"));
    while (true) {
      delay(1); // Stop here
    }
  }
  else if ( hardwareType == EthernetW5500) {
    Serial.println(F("W5500"));
  }
  else if ( hardwareType == EthernetW5100) {
    Serial.println(F("W5100"));
  }
  else if ( hardwareType == EthernetW5200) {
    Serial.println(F("W5200"));
  }

  uint8_t LinkStatus = Ethernet.linkStatus();
  Serial.print(F("\tCable "));
  if (LinkStatus == LinkOFF) {
    Serial.println(F("NOT connected"));
    while (true) {
      delay(1); // Stop here
    }
  }
  else if (LinkStatus == LinkON) {
    Serial.println(F("connected"));
  }
  else if (LinkStatus == Unknown) {
    if ( (hardwareType == EthernetW5100 || hardwareType == EthernetW5200) && connecte == 1 && dhcp == true ) {
      Serial.println(F("connected"));
    } else {
      Serial.println(F("unknown"));
    }
  }

  uint8_t LinkSpeed = Ethernet.linkSpeed();
  Serial.print(F("\tSpeed "));
  if (LinkSpeed == Mbs10) {
    Serial.println(F("10Mb/s"));
  }
  else if (LinkSpeed == Mbs100) {
    Serial.println(F("100Mb/s"));
  }
  else {
    Serial.println(F("Unknown"));
  }

  uint8_t LinkDuplex = Ethernet.linkDuplex();
  Serial.print(F("\tDuplex "));
  if (LinkDuplex == HalfDuplex) {
    Serial.println(F("Half"));
  }
  else if (LinkDuplex == FullDuplex) {
    Serial.println(F("Full"));
  }
  else {
    Serial.println(F("Unknown"));
  }
}
