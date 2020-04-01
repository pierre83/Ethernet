/* Copyright 2020 Pierre Casal
 * 
 * Copyright 2018 Paul Stoffregen
 *

 As getChip() is initialized once (at Ethernet.begin() when it launches W5100.init() ) it is not usefull to verify each time if the card is properly
 initialized (because the result of getChip() will never change even if the card died in the mean time)
 So, it is necessary to always control the result of Ethernet.begin(..) and remove this checking in the libraries (server and socket)
 
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

#include <Arduino.h>
#include "Ethernet.h"
#include "utility/w5100.h"

#if ARDUINO >= 156 && !defined(ARDUINO_ARCH_PIC32)
extern void yield(void);
#else
#define yield()
#endif

	// Threshold used to retrieve real values and update sockstate values
const uint16_t UPDATE_Sn_RX_RD_THRESHOLD	= 700;
	// (ms) Max time for the socket to pass from INIT state to LISTEN state 
const uint16_t SOCKET_LISTEN_TIMEOUT		= 1000;
	// (ms) Max time for the socket to pass from CLOSED to INIT/UDP/IPRAW/MACRAW state
const uint16_t SOCKET_INIT_TIMEOUT			= 100;	

// Values retrieved from RTR & RCR (Retry Time and Retry Count Registers) configuration
// Following DS, this is the ARPto (default values of RTR & RCR), TCPto = 31.8 secondes
uint16_t W5x00_RTR_RCR_TIMEOUT		= 2000;	// (ms) Retrieved during run from RTR x RCR 


uint16_t EthernetClass::outputPort[MAX_SOCK_NUM];

typedef struct {
	uint16_t RX_RSR;	// Number of bytes received
	uint16_t RX_RD;  	// Address to read
	uint16_t RX_inc; 	// how much have we advanced RX_RD
	uint16_t TX_FSR;	// Available size in the TX buffer
} socketstate_t;

static socketstate_t state[MAX_SOCK_NUM];

typedef struct {
	uint16_t state;		// Expected state failed
	uint16_t connect;  	// Connect failed
	uint16_t init; 		// Init socket failed
	//uint16_t wrong;		// Unexpected socket
} socketerror_t;

static socketerror_t error;

static uint16_t getSnTX_FSR(uint8_t s);		// Socket n TX Free Size
static uint16_t getSnRX_RSR(uint8_t s);		// Socket n RX Received Size
static void write_data(uint8_t s, uint16_t offset, const uint8_t *data, uint16_t len);
static void read_data(uint8_t s, uint16_t src, uint8_t *dst, uint16_t len);


// ==================================================================================
//						Socket management
// ==================================================================================


// **********************************************************************************
// Provide a "random" source port number
// **********************************************************************************
uint16_t EthernetClass::socketPortRand()
{
	// The "random" generator is initialized in Ethernet.cpp
	while ( 1 ) {
		uint16_t randomizedPort = random( 49152, 65535);
		if ( sourcePortCheck(randomizedPort) == true ) {
			//Serial.printf("socketPortRand: randomizedPort= %lu\n", randomizedPort);
			return randomizedPort;
		}
	}
}

// **********************************************************************************
// Source port number verification
// Verify if the number is already attributed(return false) or not(return true)
// **********************************************************************************
bool EthernetClass::sourcePortCheck(uint16_t port)
{
	uint8_t maxIndex = W5100.getMaxSocket();
	for ( uint8_t i = 0; i < maxIndex; i++) {
		//Serial.printf("Used port: %d, index: %i\n", port, i);
		if ( port == outputPort[i] ) return false;
	}
	return true;
}

// **********************************************************************************
// Opens a socket : TCP, UDP, IP_RAW or MAC_RAW mode.
// Return: number of the socket found, MAX_SOCK_NUM if none.
// **********************************************************************************
uint8_t EthernetClass::socketBegin(uint8_t protocol, uint16_t port)
{
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	uint8_t s = socketInit(protocol, port);
	if ( s >= MAX_SOCK_NUM ) return MAX_SOCK_NUM;
	W5100.execCmdSn(s, Sock_OPEN);
	switch ( protocol ) {
		case SnMR::TCP:
			s = socketCheckState(SnSR::INIT, s, SOCKET_INIT_TIMEOUT);
			break;
		case SnMR::UDP:
			s = socketCheckState(SnSR::UDP, s, SOCKET_INIT_TIMEOUT);
			break;
		case SnMR::IPRAW:
			s = socketCheckState(SnSR::IPRAW, s, SOCKET_INIT_TIMEOUT);
			break;
		case SnMR::MACRAW:
			s = socketCheckState(SnSR::MACRAW, s, SOCKET_INIT_TIMEOUT);
			break;
		default:
			s = MAX_SOCK_NUM;
			break;
	}
	SPI.endTransaction();
	return s;
}

// **********************************************************************************
// Multicast version of open a socket (UDP mode)
// Set fields before opening and select IGMP version (1=v1 or 2=v2)
// **********************************************************************************
uint8_t EthernetClass::socketBeginMulticast(IPAddress ip, uint16_t port, uint8_t igmpVersion)
{
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	uint8_t protocol = SnMR::UDP | SnMR::MULTI;
	if ( igmpVersion == 2 ) protocol |= SnMR::IGMP;
	uint8_t s = socketInit(protocol, port);
	if ( s < MAX_SOCK_NUM ) {
		// Calculate MAC address from Multicast IP Address
		byte mac[] = { 0x01, 0x00, 0x5E, 0x00, 0x00, 0x00 };
		mac[3] = ip[1] & 0x7F;
		mac[4] = ip[2];
		mac[5] = ip[3];
		W5100.writeSnDIPR(s, ip.raw_address());   //239.255.0.1
		W5100.writeSnDPORT(s, port);
		W5100.writeSnDHAR(s, mac);
		W5100.execCmdSn(s, Sock_OPEN);
		s = socketCheckState(protocol, s, SOCKET_INIT_TIMEOUT);
	}
	SPI.endTransaction();
	return s;
}

// **********************************************************************************
// Pick and configure a socket
// Return: number of the socket found, MAX_SOCK_NUM if none.
// **********************************************************************************
uint8_t EthernetClass::socketInit(uint8_t protocol, uint16_t port)
{
	uint8_t maxIndex = W5100.getMaxSocket();
	uint16_t localPort = port;
	uint8_t s;
	//Serial.printf("W5x00socket begin, protocol=%d, port=%d, maxIndex=%d\n", protocol, port, maxIndex);
	
	// look at all the hardware sockets, use any that are closed
	for ( s=0; s < maxIndex; s++) {
		if (W5100.readSnSR(s) == SnSR::CLOSED) goto makesocket;
	}
	//Serial.printf("W5x00socket step2\n");
	// as a last resort, forcibly close any already closing
	// Once the process began (called by disconnect() for ex.) 
	// the W5x00 completely manage the disconnection process.
	for ( s=0; s < maxIndex; s++) {
		uint8_t stat = W5100.readSnSR(s);
		if (stat == SnSR::CLOSED) 	 goto makesocket;
		if (stat == SnSR::TIME_WAIT) goto closemakesocket;
		if (stat == SnSR::LAST_ACK)  goto closemakesocket;
		if (stat == SnSR::FIN_WAIT)  goto closemakesocket;
		if (stat == SnSR::CLOSING)   goto closemakesocket;
	}
	error.init++;
	//Serial.println("NO socket");
	return MAX_SOCK_NUM; // all sockets are in use
	
closemakesocket:
	//Serial.printf("W5x00socket close\n");
	W5100.execCmdSn(s, Sock_CLOSE);
	
makesocket:
	//Serial.printf("W5x00socket %d\n", s);
	W5100.writeSnMR(s, protocol);
	// if the source port is not provided, give a random one.
	if (localPort == 0) {
		localPort = socketPortRand();
	}
	W5100.writeSnPORT(s, localPort);
	W5100.writeSnIR(s, 0xFF);	// Clear socket's interrupts
	EthernetClass::outputPort[s] = localPort;
	state[s].RX_RSR = 0;
	state[s].RX_RD	= 0;		//Initialized to 0 by SOCK_OPEN command
	state[s].RX_inc	= 0;
	state[s].TX_FSR	= W5100.SSIZE;		// Buffer size
	//Serial.printf("W5000socket prot=%u, port=%u\n", protocol, localPort);
	W5x00_RTR_RCR_TIMEOUT = W5100.getW5x00Timeout();
	//Serial.print("W5x00_RTR_RCR_TIMEOUT: "); Serial.println(W5x00_RTR_RCR_TIMEOUT);
	return s;
}

// **********************************************************************************
// Check that the socket is in the expected state ( except for connect() )
// **********************************************************************************
uint8_t EthernetClass::socketCheckState(uint8_t expectedState, uint8_t s, uint16_t timeout)
{
	uint32_t stopWait = millis() + timeout;
	while( millis() < stopWait ) {
		if ( W5100.readSnSR(s) == expectedState ) return s;	// Correct state achieved
		yield();
	}
	// We never reached the expected state, we cannot go further
	W5100.writeSnIR(s, 0xFF);	// Clear possible Socket's interrupts
	W5100.execCmdSn(s, Sock_CLOSE);  // See W5200 DS pg 46 & pg 54
	EthernetClass::outputPort[s] = 0;
	error.state++;
	return MAX_SOCK_NUM;
}

// **********************************************************************************
// Returns the socket's status
// **********************************************************************************
uint8_t EthernetClass::socketStatus(uint8_t s)
{
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	uint8_t status = W5100.readSnSR(s);
	SPI.endTransaction();
	return status;
}

// **********************************************************************************
// Returns the socket's interrupts
// Concerning RECV, It is set as ‘1’ whenever W5100 receives data. And it is also set 
// as ‘1’ if received data remains after execute CMD_RECV command. See W5100 DS pg29
// **********************************************************************************
uint8_t EthernetClass::socketInterrupt(uint8_t s)
{
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	uint8_t interrupts = W5100.readSnIR(s);
	SPI.endTransaction();
	return interrupts;
}

// **********************************************************************************
// Place the socket in listening (SERVER) mode
// and check the corresponding status:0= failed, 1 = success
// **********************************************************************************
uint8_t EthernetClass::socketListen(uint8_t s)
{
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	W5100.execCmdSn(s, Sock_LISTEN);
	uint8_t ret = socketCheckState(SnSR::LISTEN, s, SOCKET_LISTEN_TIMEOUT);
	SPI.endTransaction();
	if ( ret  < MAX_SOCK_NUM ) return 1;
	return 0;
}

// **********************************************************************************
// Establish a TCP connection in Active (CLIENT) mode.
// Setting the desired destination IP and port and connecting to the remote end
// and check the corresponding status: 0= failed, 1 = success
// **********************************************************************************
uint8_t EthernetClass::socketConnect(uint8_t s, uint8_t * addr, uint16_t port)
{
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	W5100.writeSnDIPR(s, addr);
	W5100.writeSnDPORT(s, port);
	W5100.execCmdSn(s, Sock_CONNECT);
	//The connect-request fails in the following three cases:
	// 1. When a ARPTO occurs(Sn_IR(3)=‘1’) because the destination
	// hardware address is not acquired through the ARP-process.
	// 2. When a SYN/ACK packet is not received and TCPTO occurs(Sn_IR(3)=‘1' )
	// 3. When a RST packet is received instead of a SYN/ACK packet.
	// In these cases, Sn_SR is changed to SOCK_CLOSED.
	uint8_t status = 0;
	uint32_t stopWait = millis() + W5x00_RTR_RCR_TIMEOUT;
	while( millis() < stopWait ) {
		status = W5100.readSnSR(s);
		if ( status == SnSR::ESTABLISHED ) {
			SPI.endTransaction();
			return 1;	// Correct state achieved
		}
		// The W5100 sometimes goes to CLOSED state between INIT and ESTABLISHED states
		if ( (status == SnSR::CLOSED) && (W5100.getChip() != 51) ) break;
		yield();
	}
	SPI.endTransaction();
#if 0
	uint32_t fail = millis() - (stopWait - W5x00_RTR_RCR_TIMEOUT);
	Serial.print("Fail: "); Serial.print(fail); Serial.println("ms");
	Serial.print("SnIR: "); Serial.println(W5100.readSnIR(s));
	Serial.print("SnSR: "); Serial.println(status);
#endif
	error.connect++;
	socketClose(s);
	return 0;
}

// **********************************************************************************
// Gracefully disconnect a TCP connection.
// Start the disconnection process: send a FIN to the other end, waiting for an ACK from remote
// Once we sent FIN to the other end, we will not be able to send anymore datas(RFC)
// **********************************************************************************
void EthernetClass::socketDisconnect(uint8_t s)
{
	// No verification for this action, socket will be closed by socketInit() if needed
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	W5100.execCmdSn(s, Sock_DISCON);
	SPI.endTransaction();
}

// **********************************************************************************
// Immediate close.  If a TCP connection is established, the remote host
// is left unaware we closed. Can takes place after disconnect
// **********************************************************************************
void EthernetClass::socketClose(uint8_t s)
{
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	W5100.writeSnIR(s, 0xFF);	// Clear the remained Socket's interrupts
	W5100.execCmdSn(s, Sock_CLOSE);
	SPI.endTransaction();
	EthernetClass::outputPort[s] = 0;
	s = MAX_SOCK_NUM;
}

// **********************************************************************************
// Return errors encoutered during run.
// Used for debugging purpose
// **********************************************************************************
uint8_t EthernetClass::getSocketError(uint8_t what)
{
	uint8_t whichOne = what % 3;
	switch (whichOne) {
		case 0:
			return error.init;
		case 1:
			return error.connect;
		case 2:
			return error.state;
	}
	return 0;
}

//===============================================================================
//				Socket Data Receive Functions
//===============================================================================

// ************************************************************************************
// Return: TOTAL number of bytes available in the receive buffer
// It is recommended that you read all 16-bits twice or more until getting the same value.
// ************************************************************************************
static uint16_t getSnRX_RSR(uint8_t s)
{
	uint8_t count = 0;
	uint16_t prev = state[s].RX_RSR;		// Socket n RX Received Size
	while ( count++ < 5 ) {		// 5: arbitrary
		uint16_t val = W5100.readSnRX_RSR(s);
		if (val == prev) {
			return prev;
		}
		prev = val;
	}
	return prev;
}

// **********************************************************************************
// Read data from W5x00 buffer
// **********************************************************************************
static void read_data(uint8_t s, uint16_t src, uint8_t *dst, uint16_t len)
{
	//Serial.printf("read_data, len=%d, at:%d\n", len, src);
	uint16_t src_mask = (uint16_t)src & W5100.SMASK;
	uint16_t src_ptr = W5100.RBASE(s) + src_mask;

	if (W5100.hasOffsetAddressMapping() || (src_mask + len) <= W5100.SSIZE) {
		W5100.read(src_ptr, dst, len);
	} else {
		uint16_t size = W5100.SSIZE - src_mask;
		W5100.read(src_ptr, dst, size);
		dst += size;
		W5100.read(W5100.RBASE(s), dst, len - size);
	}
}

// ************************************************************************************
// Receive data in corresponding socket buffer
// Return: received size, -1= for no data, or 0= remote closed/closing(TCP)
// ************************************************************************************
int EthernetClass::socketRecv(uint8_t s, uint8_t *buf, uint16_t len)
{
	int ret = len;
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	// Check how much data is available
	if ( len > state[s].RX_RSR ) {	// Not enough data in the memorized buffer size
		// Verify real available quantity
		ret = getSnRX_RSR(s) - state[s].RX_inc;		// Bytes in buffer - Bytes already read
		state[s].RX_RSR = ret;						// = Bytes waiting to be read
		//Serial.printf("Sock_RECV, RX_RSR=%d, RX_inc=%d\n", ret, state[s].RX_inc);
		if (ret > (int)len) ret = len; // more data to be read than given buffer size
	}
	if (ret == 0) {
		uint8_t status = W5100.readSnSR(s);
		SPI.endTransaction();
		if ( status == SnSR::ESTABLISHED || status == SnSR::UDP ) {
			// The connection is still up(TCP), for UDP... But there's nothing to read
			//Serial.printf("socketRecv, ret= -1\n");
			return -1;
		}
		// TCP: The remote end has closed its side of the connection, keep the application aware
		//Serial.printf("socketRecv, ret= 0\n");
		return 0;
	}

    if (buf) read_data(s, state[s].RX_RD, buf, ret);
	// Increase RX_RD with bytes read
	state[s].RX_RD += ret;
	// Decrease RX_RSR with bytes read
	state[s].RX_RSR -= ret;
	// Increase RX_inc with bytes read
	state[s].RX_inc += ret;
	// update Sn_RX_RD only if state[s].RX_inc > UPDATE_Sn_RX_RD_THRESHOLD or no more data avalaible for read
	if ( state[s].RX_inc > UPDATE_Sn_RX_RD_THRESHOLD || state[s].RX_RSR == 0) {
		state[s].RX_inc = 0;
		W5100.writeSnRX_RD(s, state[s].RX_RD);		// Socket n RX Read Pointer
		W5100.execCmdSn(s, Sock_RECV);		//  RECV command is for notifying the updated Sn_RX_RD to W5x00
		//Serial.printf("Sock_RECV cmd, RX_RD=%d, RX_RSR=%d, RX_inc=%d\n", state[s].RX_RD, state[s].RX_RSR, state[s].RX_inc);
	}
#ifndef ENABLE_INTERRUPTS
	W5100.writeSnIR(s, SnIR::RECV);
#endif
	SPI.endTransaction();
	//Serial.printf("socketRecv, ret=%d\n", ret);
	return ret;
}

// **********************************************************************************
//	Return: number of bytes available for read or 0 if none are available
// **********************************************************************************
uint16_t EthernetClass::socketRecvAvailable(uint8_t s)
{
	if ( state[s].RX_RSR > 0 ) return state[s].RX_RSR;
	
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	uint16_t ret = getSnRX_RSR(s);		// Amount in buffer
	SPI.endTransaction();
	//Serial.printf("sockRecvAvailable2 s=%d, RX_RSR=%d\n", s, ret);
	if ( ret == 0 ) return 0;
	ret -= state[s].RX_inc;		// Amount in buffer - Already read
	state[s].RX_RSR = ret;		// = Bytes to read
	//Serial.printf("sockRecvAvailable3 s=%d, RX_RSR=%d\n", s, ret);
	return ret;
}

// ************************************************************************************
// Get the first byte in the receive queue (no checking) and no update of pointers
// state[s].RX_RD and state[s].RX_RSR
// ************************************************************************************
uint8_t EthernetClass::socketPeek(uint8_t s)
{
	uint8_t b;
	uint16_t ptr = state[s].RX_RD & W5100.SMASK;
	ptr += W5100.RBASE(s);
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	W5100.read(ptr , &b, 1);
	SPI.endTransaction();
	return b;
}


// ==================================================================================
// Socket Data Transmit Functions
// ==================================================================================

// **********************************************************************************
// Get Socket n TX Free Size
// as the W5x00 is constantly sending (when available) reading more than once is better
// **********************************************************************************
static uint16_t getSnTX_FSR(uint8_t s)
{
	uint8_t count = 0;
	uint16_t prev = state[s].TX_FSR;
	while ( count++ < 5 ) {		// 5: arbitrary
		uint16_t val = W5100.readSnTX_FSR(s);
		if (val == prev) {
			break;
		}
		prev = val;
	}
	state[s].TX_FSR = prev;
	return prev;
}

// **********************************************************************************
// Write data into W5x00 buffer
// **********************************************************************************
static void write_data(uint8_t s, uint16_t data_offset, const uint8_t *data, uint16_t len)
{
	uint16_t ptr = W5100.readSnTX_WR(s);	// Socket n TX Write Pointer Register
	ptr += data_offset;
	uint16_t offset = ptr & W5100.SMASK;
	uint16_t dstAddr = offset + W5100.SBASE(s);

	if (W5100.hasOffsetAddressMapping() || offset + len <= W5100.SSIZE) {
		W5100.write(dstAddr, data, len);
	} else {
		// Wrap around circular buffer
		uint16_t size = W5100.SSIZE - offset;
		W5100.write(dstAddr, data, size);
		W5100.write(W5100.SBASE(s), data + size, len - size);
	}
	ptr += len;
	W5100.writeSnTX_WR(s, ptr);
}


// **********************************************************************************
// This function is used to send the data in TCP mode
// Return: bytes written, 0= timeout/fail, -1= W5x00 failed(read interrupts if using them)
// **********************************************************************************
int EthernetClass::socketSend(uint8_t s, const uint8_t * buf, uint16_t len)
{
	const uint16_t W5x00_FSR_TIMEOUT = 300;	// (us) Limit of time to get FSR infos
	uint32_t stopWait;
	int ret = len;
	
	// Verify and try to have available buffer size greater/equal than the requested write size
	if ( len > state[s].TX_FSR ) { // check size not to exceed available buffer size.
		if (len > W5100.SSIZE) { // check size not to exceed MAX buffer size.
			ret = W5100.SSIZE;	// Send truncated datas => use LARGE_BUFFERS ?
		}
		int freesize;
		stopWait = micros() + W5x00_FSR_TIMEOUT ;
		do {
			SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
			freesize = W5100.readSnTX_FSR(s);		// Socket TX Free Size
			uint8_t status = W5100.readSnSR(s);
			SPI.endTransaction();
			if ((status != SnSR::ESTABLISHED) && (status != SnSR::CLOSE_WAIT)) {
				return 0;		// Not connected
			}
			if ( micros() > stopWait ) {	// Also manage freesize < len
				ret = freesize;	// Send truncated datas
				break;
			} 
			yield();
		} while (freesize < ret);
		state[s].TX_FSR = freesize;
	}
	// once here, ret is <= freesize
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

	write_data(s, 0, (uint8_t *)buf, ret);	// copy data
	W5100.execCmdSn(s, Sock_SEND);			// send data

	stopWait = millis() + W5x00_RTR_RCR_TIMEOUT;	// Will Timeout before TCPto (31.8sec)
	// Wait for operation complete
	while (1) {
		uint8_t status = W5100.readSnIR(s);
		if ( status & SnIR::SEND_OK ) {		// Sent OK
			W5100.writeSnIR(s, SnIR::SEND_OK);
			// As we did'nt count +ret, we don't count -ret
			break;
		}
		// TIMEOUT Interrupts is set when TCPTO occurs,
		// The time of final timeout (TCPTO) of TCP retransmission is 31.8sec...
		// When TCPTO occurs, it is changed to SOCK_CLOSED regardless of the previous value.
		if (( W5100.readSnSR(s) == SnSR::CLOSED ) || ( status & SnIR::TIMEOUT ) ) {
			W5100.writeSnIR(s, SnIR::SEND_OK | SnIR::TIMEOUT);
			getSnTX_FSR(s);
			ret = 0;
			break;
		}
		if ( millis() > stopWait ) {	// Default ARPto of W5x00 is 1618ms (Configured with RTR-RCR)
			getSnTX_FSR(s);
			ret = -1;
			break;
		}
		SPI.endTransaction();
		yield();
		SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	}
	SPI.endTransaction();
#if 0
	uint32_t elapsed = millis() - (stopWait-W5x00_RTR_RCR_TIMEOUT);
	if ( ret == 0 )  Serial.printf("send timeout: %lums", elapsed);
	if ( ret == -1 ) Serial.printf("W5x00 frozen? %lums", elapsed);
#endif
	return ret;
}

// **********************************************************************************
// TX buffer Free space available to send data (the socket must be connected)
// Return: freesize(ESTABLISHED, CLOSE_WAIT or UDP ), 0= buffer full ( ESTABLISHED, 
// CLOSE_WAIT or UDP), -1 else. Can be used in UDP mode
// **********************************************************************************
int EthernetClass::socketSendAvailable(uint8_t s)
{
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	uint8_t status = W5100.readSnSR(s);
	if ((status == SnSR::ESTABLISHED) || (status == SnSR::CLOSE_WAIT) || (status == SnSR::UDP)) {
		uint16_t freesize = state[s].TX_FSR;
		if ( freesize < UPDATE_Sn_RX_RD_THRESHOLD ) {	// Let enough room to send something
			freesize = getSnTX_FSR(s);	// = update state[s].TX_FSR
		}
		SPI.endTransaction();
		return freesize;
	}
	SPI.endTransaction();
	return -1;
}

// ==================================================================================
//    UDP specific functions
// ==================================================================================

// **********************************************************************************
// Verify and initialize the socket with the destination IP address
// Returns true if ok, false if not
// **********************************************************************************
int EthernetClass::socketStartUDP(uint8_t s, uint8_t* addr, uint16_t port)
{
	if ( ( (addr[0] == 0x00) && (addr[1] == 0x00) && (addr[2] == 0x00) && (addr[3] == 0x00)) || port == 0x00 ) {
		return 0;
	}
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	W5100.writeSnDIPR(s, addr);
	W5100.writeSnDPORT(s, port);
	SPI.endTransaction();
	return 1;
}

// **********************************************************************************
// Sets up a UDP datagram, the data for which will be provided by one
// or more calls to socketBufferDataUDP and then finally sent with socketSendUDP.
// Return: Number of bytes successfully buffered
// **********************************************************************************
uint16_t EthernetClass::socketBufferDataUDP(uint8_t s, uint16_t offset, const uint8_t* buf, uint16_t len)
{
	//Serial.printf("  bufferData, offset=%d, len=%d\n", offset, len);
	uint16_t ret = len;
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	if ( ret > state[s].TX_FSR ) {
		uint16_t txfree = getSnTX_FSR(s);	// = update state[s].TX_FSR
		if ( ret > txfree ) {
			ret = txfree; // check size not to exceed MAX available buffer size
		}
	}
	write_data(s, offset, buf, ret);
	SPI.endTransaction();
	state[s].TX_FSR -= ret;	//Decrease free TX buffer with amount buffered
	return ret;
}

// ************************************************************************************
// Send a UDP datagram built up from a sequence of begin() followed by one
// or more calls to bufferDataUDP using write(..) or write().
// Return: 1= success, 0= timeout, -1= W5x00 failed(read interrupts if using them)
// ************************************************************************************
int EthernetClass::socketSendUDP(uint8_t s)
{
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	W5100.execCmdSn(s, Sock_SEND);

	// Wait for op complete
	int ret = -1;		// W5x00 failed
	uint32_t stopWait = millis() + W5x00_RTR_RCR_TIMEOUT;
	while (millis() < stopWait ) {			// W5x00 failed
		uint8_t status = W5100.readSnIR(s);
		if ( ( status & SnIR::SEND_OK ) == SnIR::SEND_OK ) {
			//W5100.writeSnIR(s, SnIR::SEND_OK);
			//Serial.printf("sendUDP ok\n");
			ret = 1;
			break;
		}
		// ARPTO occurs (Sn_IR(s)=‘1’) because the Destination Hardware Address is not
		// acquired through the ARP process. In case of UDP or IPRAW mode it goes back
		// to the previous status(SOCK_UDP or SOCK_IPRAW)
		if ( ( status & SnIR::TIMEOUT ) == SnIR::TIMEOUT ) { // Only on first ARP request
			//W5100.writeSnIR(s, SnIR::TIMEOUT);			// then the address is memorized
			ret = 0;									// except id FARP is set (W5500)
			break;
		}
		SPI.endTransaction();
		yield();
		SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	}
	getSnTX_FSR(s);  // => update state[s].TX_FSR
	W5100.writeSnIR(s, (SnIR::SEND_OK || SnIR::TIMEOUT));
	SPI.endTransaction();
#if 0
	uint32_t elapsed = millis() - (stopWait - W5x00_RTR_RCR_TIMEOUT);
	if ( ret == -1 ) Serial.printf("W5x00 issue: %lums", elapsed);
	if ( ret == 0 )  Serial.printf("sendUDP timeout: %lums", elapsed);
#endif
	return ret;
}


// *******************************************************
