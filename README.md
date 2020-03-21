# Ethernet
= Improvment fork of the Ethernet Library for Arduino =
=======================================================

This fork was created to fix some W5x00 issues I encountered in my daily use of the library and bring some improvment starting from Paul Stoffregen EThernet libraries for WizNet W5x00

In particular some lags in EthernetServer answers or messages which never arrive,

Secondly, the clientConnect was not enough reliable, now it's not perfect but better,

Thirdly, UDP was sometimes 'stuck' unable to send or receive anything,

Then faster as much as possible all procedures and calls,

Sidely, this version is more reliable by reporting the majority of events up to the user.



Errors in files:
-----------------
RX_inc must be an uint16_t instead of uint8_t (as both are compared)

TX_FSR : not initialized correctly and never used in the library despite the fact it is updated in getSnTX_FSR()

Error in LARGE BUFFER #endif content (w5100.cpp : w5200 init)

Errors in W5500 addresses:(already seen on github)

  GP_REGISTER8 (SIR,    0x0017);    // Sockets Interrupt		// Which socket raised an interrupt
  
  GP_REGISTER8 (SIMR,   0x0018);    // Sockets Interrupt Mask	// Which socket is allowed to raise an interrupt
  
  GP_REGISTER16(RTR,    0x0019);    // Timeout address


Changes and corrections:
------------------------
1.EthernetUdp : Add a timeout out in UDP send and report to the user/sketch in case of negative event.

2.EthernetClient : if client.connect() asked twice reports -1, close the socket in any other case,

3 EthernetUdp : udp.read(...) and read() _remaining adjusted to 0 if no data,

4.client.connect() (1c) return 1 if the socket status is ESTABLISHED after connecting,

5.all files: No more loops() without timeout,

6.EthernetUdp : udp.parsePacket() doesn't flush() anymore, this is the sketch responsability,

7.EthernetServer: (1a-1b) avoid to start another server which was sometimes the case. This avoid also the loss of messages and sometime inexplicable response delays

  

bugs created:
-------------
1.Arduino Due : link on/off & duplex not correct


Performances:
-------------
1.Between 10 and 30% increase in TX speed, all protocols,

2 Increase up to 90% in transactionnal mode,

3 Increase up to 25% in server mode,

4 Small increase in RX modes

5.10% decrease with DUE in RX bytes continuous mode,


Improvements:
-------------
1a.Socket.cpp: now verifies that the status is INIT after a socketBegin()

1b.Socket.cpp: now verifies that the status is ESTABLISHED after a socketConnect() command

1c.Socket.cpp: now verifies that the status is LISTEN after a socketListen() command

1d.Socket.cpp: now verifies that the status is UDP after a socketStartUDP() command

2.EthernetUdp.cpp: Fixed udp.flush(),

3.EthernetClient.cpp: Verification if client connect is called twice by the sketch,

4.Socket.cpp: If no local port is provided, socket provide a 'random' one,

5.Socket.cpp: Manage provided port output list in outputPort[] and verify unity,

6.dhcp.cpp: Remove random DHCP port thanks to (4),

7.dhcp.cpp: DHCP now reports errors,

8.Ethernet.cpp: all 'Ethernet.begin' always report the result 1 or -1,

9.Report up to the application if the socket is closed, as much as possible,

10.Ethernet.cpp: Report link duplex and speed (W5500),

11.w5100.cpp: Implement SPI buffer transfert in W5100.write (w5200 & w5500),

12.w5100.cpp: Add interrupt availables,

13.EthernetClient.cpp: Client connect() more reliable,

14.Socket.cpp: Let the sketch aware in case of W5x00 needs attention (return -1),

15.EthernetServer.cpp: Far more reliable EthernetServer thanks to 1a-1b, uses interrupts to track incoming datas

16.The W5x00 can be configured for IGMP v1 or v2 version.

Interrupt management:
---------------------
1.Enable global interrupts, count bits represent interrupts from 7...0, (1 enabling it)

  static void enableSocketsInterrupt(uint8_t count) { writeSIMR(count); }
  
2.Clear the general socket interrupt register

  static void clearSocketsInterrupt(uint8_t value) { writeSIR(value); }
  
3.Read the general socket interrupt register

  static uint8_t getSocketsInterrupt() { return readSIR(); }
  
4.Clear the socket interrupt register

  static void clearSocketInterrupt(uint8_t s, uint8_t value) { writeSnIR(s, value); }
  
5.Return the socket's interrupt register

  static uint8_t getSocketInterrupt(uint8_t s) { return readSnIR(s); }
  
  
Tools:
------
Some small sketches to test speed/throughput and pps transactionnal


Errors:
-------
Surely, there may still have some errors/improvements to find/make


 
