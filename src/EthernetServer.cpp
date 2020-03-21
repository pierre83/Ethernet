/* Copyright 2020 Pierre Casal
 * 
 * Copyright 2018 Paul Stoffregen
 *
 *
 09/02/20	Add socket state verification to avoid starting another server
 
 
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

// ********************************************************************
// Start or restart the web server
// ********************************************************************
void EthernetServer::begin()
{
	// Effective start was not checked in the previous versions. It is always the case but each
	// command is checked separately: it is verified that INIT state is effective after
	// socketBegin() then LISTEN state is effective after the socketListen() command.
	// Sometimes there were 2 servers running, this was caused by the time the W5x00 take to change 
	// state as going from INIT state to LISTEN state was sometimes very slow or never happened in
	// the worst case.
	uint8_t sockindex = Ethernet.socketBegin(SnMR::TCP, _port);
	if (sockindex >= MAX_SOCK_NUM) return;
	Ethernet.socketListen(sockindex);
}

// ********************************************************************************
// available() has several functions:
// Return the socket receiving the datas
// Start the close process in disconnecting the socket if a FIN has been received
// Restart the server if none is running
// Release EthernetClass::outputPort[x]
// Must run continuously (if socket or interrupts are not managed directly)
// ********************************************************************************
EthernetClient EthernetServer::available()
{
	//https://benohead.com/blog/2013/07/21/tcp-about-fin_wait_2-time_wait-and-close_wait/
	bool listening = false;
	uint8_t sockindex = MAX_SOCK_NUM;

	uint8_t maxIndex = W5100.getMaxSocket();
	for (uint8_t i=0; i < maxIndex; i++) {
		if (EthernetClass::outputPort[i] == _port) {
			switch ( Ethernet.socketStatus(i) ) {
				// As the socket status is verified (up to LISTEN) we should normaly never be (here) in INIT state
				case SnSR::LISTEN:		// Most of the time
					// A web server is running
					listening = true;
					break;
				case SnSR::ESTABLISHED:
					// SnIR::RECV is set as ‘1’ whenever W5100 receives data. And it is also set as
					// ‘1’ if received data remains after execute CMD_RECV command. See W5100 DS pg29
					//if ( Ethernet.socketRecvAvailable(i) > 0 ) {
					if ( ( Ethernet.socketInterrupt(i) & SnIR::RECV ) == SnIR::RECV ) {
						sockindex = i; 
					}
					break;
				case SnSR::CLOSE_WAIT:
					// We received a FIN from the remote host wanting to close its connection and 
					// telling us that no more datas will be sent through this pipe
					// The W5x00, when in this state, has already sent an ACK to acknoweledge the FIN, then it passed to CLOSE_WAIT state
					// If we have some datas in the buffer received before the FIN: read them
					//if ( Ethernet.socketRecvAvailable(i) > 0 ) {
					if ( ( Ethernet.socketInterrupt(i) & SnIR::RECV ) == SnIR::RECV ) {
						// There is something in the RX buffer!
						sockindex = i;
					} else {		// Nothing in the buffer:
						// it's time to send our FIN to the other side
						Ethernet.socketDisconnect(i);
						// The status becomes LAST_ACK for short time
						// The W5x00 is now waiting from an ACK from the other end to close this socket
					}
					break;
				case SnSR::CLOSED:
					// Sn_SR changes to SOCK_CLOSED during a client connection failure(SYN/ACK packet failed to transfer)
					EthernetClass::outputPort[i] = 0;
					break;
			}
		}
	}
	// Once answer is done, if the server's socket is closed
	// we need to open another socket to listen to incoming messages
	if (!listening) begin();
	return EthernetClient(sockindex);
}

// ********************************************************************
// accept(), like available() has several functions:
// return the socket receiving the datas : only return the client once
// restart the server if none is running
// release EthernetClass::outputPort[x]
// Must run continuously
// ********************************************************************
EthernetClient EthernetServer::accept()
{
	bool listening = false;
	uint8_t sockindex = MAX_SOCK_NUM;
	uint8_t maxIndex = W5100.getMaxSocket();

	for (uint8_t i=0; i < maxIndex; i++) {
		if (EthernetClass::outputPort[i] == _port) {
			uint8_t stat = Ethernet.socketStatus(i);
			if (sockindex == MAX_SOCK_NUM && (stat == SnSR::ESTABLISHED || stat == SnSR::CLOSE_WAIT)) {
				// Return the connected client even if no data received.
				// Some protocols like FTP expect the server to send the first data.
				sockindex = i;
				EthernetClass::outputPort[i] = 0; // only return the client once
			} else if (stat == SnSR::LISTEN || stat == SnSR::INIT) {
				listening = true;
			} else if (stat == SnSR::CLOSED) {
				EthernetClass::outputPort[i] = 0;
			}
		}
	}
	if (!listening ) begin();
	return EthernetClient(sockindex);
}

// ********************************************************************
// Use as: if ( server_name ) then....
// ********************************************************************
EthernetServer::operator bool()
{
	uint8_t maxIndex = W5100.getMaxSocket();

	for (uint8_t i=0; i < maxIndex; i++) {
		if (EthernetClass::outputPort[i] == _port) {
			if (Ethernet.socketStatus(i) == SnSR::LISTEN) {
				return true; // server is listening for incoming clients
			}
		}
	}
	return false;
}

// ********************************************************************
// Returns	1= ok, 0 = timeout/fail, 65535 = frozen
// ********************************************************************
size_t EthernetServer::write(uint8_t b)
{
	return write(&b, 1);
}

// ********************************************************************
// As one socket by client, this function writes the same datas 
// to each client connected to this server's _port. (used by 'chat')
// Returns last amount of bytes written 
// ********************************************************************
size_t EthernetServer::write(const uint8_t *buffer, size_t size)
{
	uint8_t maxIndex = W5100.getMaxSocket();
	available();
	for (uint8_t i=0; i < maxIndex; i++) {
		if (EthernetClass::outputPort[i] == _port) {
			if (Ethernet.socketStatus(i) == SnSR::ESTABLISHED) {
				Ethernet.socketSend(i, buffer, size);
			}
		}
	}
	return size;
}

#if 0
void EthernetServer::statusreport()
{
	Serial.printf("EthernetServer, port=%d\n", _port);
	for (uint8_t i=0; i < MAX_SOCK_NUM; i++) {
		uint16_t port = EthernetClass::outputPort[i];
		uint8_t stat = Ethernet.socketStatus(i);
		const char *name;
		switch (stat) {
			case 0x00: name = "CLOSED"; break;
			case 0x13: name = "INIT"; break;
			case 0x14: name = "LISTEN"; break;
			case 0x15: name = "SYNSENT"; break;
			case 0x16: name = "SYNRECV"; break;
			case 0x17: name = "ESTABLISHED"; break;
			case 0x18: name = "FIN_WAIT"; break;
			case 0x1A: name = "CLOSING"; break;
			case 0x1B: name = "TIME_WAIT"; break;
			case 0x1C: name = "CLOSE_WAIT"; break;
			case 0x1D: name = "LAST_ACK"; break;
			case 0x22: name = "UDP"; break;
			case 0x32: name = "IPRAW"; break;
			case 0x42: name = "MACRAW"; break;
			case 0x5F: name = "PPPOE"; break;
			default: name = "???";
		}
		int avail = Ethernet.socketRecvAvailable(i);
		Serial.printf("  %d: port=%d, status=%s (0x%02X), avail=%d\n",i, port, name, stat, avail);
	}
}
#endif
