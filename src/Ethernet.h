/* Copyright 2020 Pierre Casal 
 *
 * Copyright 2018 Paul Stoffregen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ethernet_h_
#define ethernet_h_

// All symbols exposed to Arduino sketches are contained in this header file
//
// Older versions had much of this stuff in EthernetClient.h, EthernetServer.h,
// and socket.h.  Including headers in different order could cause trouble, so
// these "friend" classes are now defined in the same header file.  socket.h
// was removed to avoid possible conflict with the C library header files.


// Configure the maximum number of sockets to support.  W5100 chips can have
// up to 4 sockets.  W5200 & W5500 can have up to 8 sockets.  Several bytes
// of RAM are used for each socket.  Reducing the maximum can save RAM, but
// you are limited to fewer simultaneous connections.
/*#if defined(RAMEND) && defined(RAMSTART) && ((RAMEND - RAMSTART) <= 2048)
#define MAX_SOCK_NUM 4
#else
#define MAX_SOCK_NUM 8
#endif*/

#define MAX_SOCK_NUM 8

// By default, each socket uses 2K buffers inside the Wiznet chip.  If
// MAX_SOCK_NUM is set to fewer than the chip's maximum, uncommenting
// this will use larger buffers within the Wiznet chip.  Large buffers
// can really help with UDP protocols like Artnet.  In theory larger
// buffers should allow faster TCP over high-latency links, but this
// does not always seem to work in practice (maybe Wiznet bugs?)
//#define ETHERNET_LARGE_BUFFERS

// If it is desired to manage the card using interrupts:
// Connect the card 'int' to any digital pin interrupt compatible(or not)
// then manage interrupts reported and restart web server as necessary.
// do not use server.available() or server.accept()
//#define ENABLE_INTERRUPTS


#include <Arduino.h>
#include "Client.h"
#include "Server.h"
#include "Udp.h"

enum EthernetLinkStatus {
	Unknown,
	LinkON,
	LinkOFF,
	Mbs10,
	Mbs100,
	FullDuplex,
	HalfDuplex
};

enum EthernetHardwareStatus {
	EthernetNoHardware,
	EthernetW5100,
	EthernetW5200,
	EthernetW5500
};

class EthernetUDP;
class EthernetClient;
class EthernetServer;
class DhcpClass;

class EthernetClass {
private:
	static IPAddress _dnsServerAddress;
	static DhcpClass* _dhcp;
	static uint16_t outputPort[MAX_SOCK_NUM];
	
public:
	// Initialise the Ethernet shield to use the provided MAC address and
	// gain the rest of the configuration through DHCP.
	// Returns 0 if the DHCP configuration failed, -1 if W5x00 did not initialize 
	// properly and 1 if it succeeded
	static int begin(uint8_t *mac, unsigned long timeout = 3000);//, unsigned long responseTimeout = 5000);
	static int maintain();
	
	static EthernetLinkStatus linkStatus();
	static EthernetLinkStatus linkSpeed();
	static EthernetLinkStatus linkDuplex();
	static EthernetHardwareStatus hardwareStatus();

	// Manual configuration
	// All 'begin' return -1 if W5x00 did not initialize properly and 1 if it succeeded
	static int begin(uint8_t *mac, IPAddress ip);
	static int begin(uint8_t *mac, IPAddress ip, IPAddress dns);
	static int begin(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway);
	static int begin(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet);
	static void init(uint8_t sspin = 10);

	static void MACAddress(uint8_t *mac_address);
	static IPAddress localIP();
	static IPAddress subnetMask();
	static IPAddress gatewayIP();
	static IPAddress dnsServerIP() { return _dnsServerAddress; }

	static void setMACAddress(const uint8_t *mac_address);
	void setLocalIP(const IPAddress local_ip);
	void setSubnetMask(const IPAddress subnet);
	void setGatewayIP(const IPAddress gateway);
	void setDnsServerIP(const IPAddress dns_server) { _dnsServerAddress = dns_server; }

	void setRetransmissionTimeout(uint16_t milliseconds);
	void setRetransmissionCount(uint8_t num);

	friend class EthernetClient;
	friend class EthernetServer;
	friend class EthernetUDP;

private:

	// Opens a socket(TCP or UDP or IP_RAW mode)
	static uint8_t socketBegin(uint8_t protocol, uint16_t port=0);
	// Multicast version of open a socket (UDP mode)
	static uint8_t socketBeginMulticast(IPAddress ip, uint16_t port=0, uint8_t igmpVersion=1);
	// Pick a socket and configure it, called by socketBegin or socketBeginMulticast
	static uint8_t socketInit(uint8_t protocol, uint16_t port);
	// Check that the socket status is in the expected mode
	static uint8_t socketCheckState(uint8_t state, uint8_t s, uint16_t timeout);
	// Returns the socket's status
	static uint8_t socketStatus(uint8_t s);
	// Returns the socket's interrupts
	static uint8_t socketInterrupt(uint8_t s);
	// Close socket
	static void socketClose(uint8_t s);
	// Establish TCP connection (Active connection: client)
	static uint8_t socketConnect(uint8_t s, uint8_t * addr, uint16_t port);
	// disconnect the connection
	static void socketDisconnect(uint8_t s);
	// Establish TCP connection (Passive connection: server)
	static uint8_t socketListen(uint8_t s);
	// Send data (TCP)
	static int socketSend(uint8_t s, const uint8_t * buf, uint16_t len);
	// Return: freesize ou 0 si buffer full (connected or close_wait state)
	// or:	-1 = socket not connected
	static int socketSendAvailable(uint8_t s);
	// Receive data (TCP), Returns size, or -1 for no data, or 0 if not connected
	static int socketRecv(uint8_t s, uint8_t * buf, uint16_t len);
	// Returns the number of characters available, or 0 if none are available
	static uint16_t socketRecvAvailable(uint8_t s);
	// Get the first byte in the receive queue
	static uint8_t socketPeek(uint8_t s);
	// sets up a UDP datagram, the data for which will be provided by one
	// or more calls to bufferData and then finally sent with sendUDP.
	// return 1 if the datagram was successfully set up, or 0 if there was an error
	static int socketStartUDP(uint8_t s, uint8_t* addr, uint16_t port);
	// copy up to len bytes of data from buf into a UDP datagram to be
	// sent later by sendUDP.  Allows datagrams to be built up from a series of bufferData calls.
	// return Number of bytes successfully buffered
	static uint16_t socketBufferDataUDP(uint8_t s, uint16_t offset, const uint8_t* buf, uint16_t len);
	// Send a UDP datagram built up from a sequence of startUDP followed by one or more
	// calls to bufferData.
	// return 1 if the datagram was successfully sent, 0 if there was a timeout, -1 if W5x00 is frozen
	static int socketSendUDP(uint8_t s);
	// Initialize the "random" source port number
	static uint16_t socketPortRand();
	// Verify if the socket has not been already attributed
	static bool sourcePortCheck(uint16_t port);

	// Writes the network parameters into the W5x00
	static void setNetworkParameters(IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet);
	// Retrieve DHCP parameters and pass them to setNetworkParameters
	static void setDHCPParameters();
};

extern EthernetClass Ethernet;



class EthernetUDP : public UDP {
private:
	uint16_t _port; // local port to listen on
	IPAddress _remoteIP; // remote IP address for the incoming packet whilst it's being processed
	uint16_t _remotePort; // remote port for the incoming packet whilst it's being processed
	uint16_t _offset; // offset into the packet being sent

protected:
	uint8_t sockindex;
	uint16_t _bytesInBuffer; // remaining bytes of incoming packet yet to be processed

public:
	EthernetUDP() : sockindex(MAX_SOCK_NUM) {}  // Constructor
	// initialize, start listening on specified port. Returns 1 if successful, 0 if there are no sockets available to use
	// if port is not provided, socket.cpp will provide a "random" one
	virtual uint8_t begin(uint16_t port = 0);
	// initialize, start listening on specified port. Returns 1 if successful, 0 if there are no sockets available to use
	virtual uint8_t beginMulticast(IPAddress, uint16_t, uint8_t igmp = 2);
	// Finish with the UDP socket
	virtual void stop();

	// Sending UDP packets
	// Start building up a packet to send to the remote host specific in ip and port
	// Returns 1 if successful, 0 if there was a problem with the supplied IP address or port
	virtual int beginPacket(IPAddress ip, uint16_t port);
	// Start building up a packet to send to the remote host specific in host and port
	// Returns 1 if successful, 0 if there was a problem resolving the hostname or port
	virtual int beginPacket(const char *host, uint16_t port);
	// Finish off this packet and send it
	// Returns 1 if the packet was sent successfully, 0 if there was a timeout or -1 if W5x00 failed
	virtual int endPacket();
	// Returns: freesize, -1= not UDP mode, 0= buffer full
	virtual int availableForWrite();
	// Write a single byte into the packet
	virtual size_t write(uint8_t);
	// Write size bytes from buffer into the packet
	virtual size_t write(const uint8_t *buffer, size_t size);

	using Print::write;

	// Start processing the next available incoming packet
	// Returns the size of the packet in bytes, or 0 if no packets are available
	virtual int parsePacket();
	// Number of bytes remaining in the current packet
	virtual int available();
	// Read a single byte from the current packet
	// Returns the byte read, or -1 if none are available
	virtual int read();
	// Read up to len bytes from the current packet and place them into buffer
	// Returns number of bytes read, or -1 for no data
	virtual int read(unsigned char* buffer, size_t len);
	// Read up to len characters from the current packet and place them into buffer
	// Returns the number of characters read, or -1 if none are available
	virtual int read(char* buffer, size_t len) { return read((unsigned char*)buffer, len); };
	// Return the next byte from the current packet without moving on to the next byte
	// get the first byte in the receive queue (no checking)
	virtual int peek();
	// Finish reading the current packet
	virtual void flush();
	
	// Return the IP address of the host who sent the current incoming packet
	virtual IPAddress remoteIP() { return _remoteIP; };
	// Return the port of the host who sent the current incoming packet
	virtual uint16_t remotePort() { return _remotePort; };
	virtual uint16_t localPort() { return _port; }
};



class EthernetClient : public Client {
public:
	EthernetClient() : sockindex(MAX_SOCK_NUM), _timeout(1000) { }
	EthernetClient(uint8_t s) : sockindex(s), _timeout(1000) { }

	uint8_t status();
	virtual int connect(IPAddress ip, uint16_t port);
	virtual int connect(const char *host, uint16_t port);
	virtual int availableForWrite(void);
	virtual size_t write(uint8_t);
	virtual size_t write(const uint8_t *buf, size_t size);
	virtual int available();
	virtual int read();
	virtual int read(uint8_t *buf, size_t size);
	virtual int peek();
	virtual void flush();
	virtual void stop();
	virtual uint8_t connected();
	virtual operator bool() { return sockindex < MAX_SOCK_NUM; }
	virtual bool operator==(const bool value) { return bool() == value; }
	virtual bool operator!=(const bool value) { return bool() != value; }
	virtual bool operator==(const EthernetClient&);
	virtual bool operator!=(const EthernetClient& rhs) { return !this->operator==(rhs); }
	uint8_t getSocketNumber() const { return sockindex; }
	virtual uint16_t localPort();
	virtual IPAddress remoteIP();
	virtual uint16_t remotePort();
	virtual void setConnectionTimeout(uint16_t timeout) { _timeout = timeout; }

	friend class EthernetServer;

	using Print::write;

private:
	uint8_t sockindex; // MAX_SOCK_NUM means client not in use
	uint16_t _timeout;
};



class EthernetServer : public Server {
private:
	uint16_t _port;
public:
	EthernetServer(uint16_t port) : _port(port) { }
	EthernetClient available();
	EthernetClient accept();
	virtual void begin();
	//virtual bool maintain();
	virtual size_t write(uint8_t);
	virtual size_t write(const uint8_t *buf, size_t size);
	virtual operator bool();
	using Print::write;
	//void statusreport();
};



class DhcpClass {
private:
	uint32_t _dhcpInitialTransactionId;
	uint32_t _dhcpTransactionId;
	uint8_t  _dhcpMacAddr[6];
#ifdef __arm__
	uint8_t  _dhcpLocalIp[4] __attribute__((aligned(4)));
	uint8_t  _dhcpSubnetMask[4] __attribute__((aligned(4)));
	uint8_t  _dhcpGatewayIp[4] __attribute__((aligned(4)));
	uint8_t  _dhcpDhcpServerIp[4] __attribute__((aligned(4)));
	uint8_t  _dhcpDnsServerIp[4] __attribute__((aligned(4)));
#else
	uint8_t  _dhcpLocalIp[4];
	uint8_t  _dhcpSubnetMask[4];
	uint8_t  _dhcpGatewayIp[4];
	uint8_t  _dhcpDhcpServerIp[4];
	uint8_t  _dhcpDnsServerIp[4];
#endif
	uint32_t _dhcpLeaseTime;
	uint32_t _dhcpT1, _dhcpT2;
	uint32_t _renewInSec;
	uint32_t _rebindInSec;
	unsigned long _timeout;
	unsigned long _responseTimeout;
	unsigned long _lastCheckLeaseMillis;
	uint8_t _dhcp_state;
	
	EthernetUDP _dhcpUdpSocket;

	int request_DHCP_lease();
	void reset_DHCP_lease();
	void printByte(char *, uint8_t);

	// new library
	bool send_DHCP_MESSAGE(uint8_t, uint32_t);
	uint8_t parseDHCPResponse( uint32_t& transactionId);
	// old library
	//uint8_t parseDHCPResponse(unsigned long responseTimeout, uint32_t& transactionId);
	//void presend_DHCP();
	//void send_DHCP_MESSAGE(uint8_t messageType, uint16_t secondsElapsed);
	
public:
	IPAddress getLocalIp();
	IPAddress getSubnetMask();
	IPAddress getGatewayIp();
	IPAddress getDhcpServerIp();
	IPAddress getDnsServerIp();

	int beginWithDHCP(uint8_t *, unsigned long timeout = 3000);//, unsigned long responseTimeout = 3000);
	int checkLease();
};





#endif
