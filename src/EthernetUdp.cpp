/* Copyright 2020 Pierre Casal
 *
 *
 *  Udp.cpp: Library to send/receive UDP packets with the Arduino ethernet shield.
 *  This version only offers minimal wrapping of socket.cpp
 *  Drop Udp.h/.cpp into the Ethernet library directory at hardware/libraries/Ethernet/
 *
 * MIT License:
 * Copyright (c) 2008 Bjoern Hartmann
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * bjoern@cs.stanford.edu 12/30/2008
 */

#include <Arduino.h>
#include "Ethernet.h"
#include "Dns.h"
#include "utility/w5100.h"


// ****************************************************************************
//		Start EthernetUDP socket, listening at local port PORT
//		Return: 0= no socket, 1= ok
// ****************************************************************************
uint8_t EthernetUDP::begin(uint16_t port)
{
    // Normally the W5x00 should close it or the sketch called it twice ?
    if (sockindex < MAX_SOCK_NUM) Ethernet.socketClose(sockindex);
    sockindex = Ethernet.socketBegin(SnMR::UDP, port);
    if (sockindex >= MAX_SOCK_NUM) return 0;
    _port = port;
    _bytesInBuffer = 0;
    return 1;
}

// ****************************************************************************
//		Start building up a packet to send to the remote host specified in host and port
//		Return: 1 = success, -xx = DNS error code
// ****************************************************************************
int EthernetUDP::beginPacket(const char *host, uint16_t port)
{
    DNSClient dns;
    IPAddress remote_addr;

    int ret = dns.begin(Ethernet.dnsServerIP());	// Good DNS ?
    if (ret != 1) return ret;
    ret = dns.getHostByName(host, remote_addr, DNS_TIMEOUT);	// Get the IP ?
    if (ret != 1) return ret;
    return beginPacket(remote_addr, port);
}

// ****************************************************************************
//		Start building up a packet to send to the remote IP and port
//		Return: 1= success, 0= bad IP, no socket
// ****************************************************************************
int EthernetUDP::beginPacket(IPAddress ip, uint16_t port)
{
    _offset = 0;
    //Serial.printf("UDP beginPacket\n");
    if (sockindex >= MAX_SOCK_NUM) return 0;
    return Ethernet.socketStartUDP(sockindex, rawIPAddress(ip), port);
}

// ****************************************************************************
//		Command to send the content of the TX buffer
//		Returns 1: send OK, 0: timeout/bad socket, -1: Send failed
// ****************************************************************************
int EthernetUDP::endPacket()
{
    if (sockindex >= MAX_SOCK_NUM) return 0;
    return Ethernet.socketSendUDP(sockindex);
}

// ****************************************************************************
//		Release any resources being used by this UDP instance
// ****************************************************************************
void EthernetUDP::stop()
{
    if (sockindex >= MAX_SOCK_NUM) return;
    Ethernet.socketClose(sockindex);
    sockindex = MAX_SOCK_NUM;
}

// ****************************************************************************
//		Check for empty space in TX buffer
//		Returns: freesize, -1= not UDP mode, 0= buffer full
// ****************************************************************************
int EthernetUDP::availableForWrite()
{
    if (sockindex >= MAX_SOCK_NUM) return 0;
    return Ethernet.socketSendAvailable(sockindex);
}

// ****************************************************************************
//		Write 1 byte into the packet buffer, will be sent
//		later using socketSendUDP(..). Return 0 or 1
// ****************************************************************************
size_t EthernetUDP::write(uint8_t byte)
{
    return write(&byte, 1);
}

// ****************************************************************************
//		Write size bytes from 'buffer' into the packet buffer, will be sent
//		later using socketSendUDP(..). Returns the number of bytes written
// ****************************************************************************
size_t EthernetUDP::write(const uint8_t *buffer, size_t size)
{
    if (sockindex >= MAX_SOCK_NUM) return 0;
    uint16_t bytes_written = Ethernet.socketBufferDataUDP(sockindex, _offset, buffer, size);
    _offset += bytes_written;
    //Serial.printf("UDP write %u\n", bytes_written);
    return bytes_written;
}

// ****************************************************************************
//		Start processing the next available incoming packet
//		Returns the size of the packet in bytes, 0= no data
//		The packet can be uncompletely in the buffer as all datas 
//		are'nt have not been received at that time
// ****************************************************************************
int EthernetUDP::parsePacket()
{
    const uint8_t HEADER_LENGTH = 8;
    if (sockindex >= MAX_SOCK_NUM) return 0;
    if ( ( Ethernet.socketInterrupt(sockindex) & SnIR::RECV ) == SnIR::RECV ) {
        //if ( Ethernet.socketRecvAvailable(sockindex) >= HEADER_LENGTH ) {
        //HACK - hand-parse the UDP packet using TCP recv method
        uint8_t headerBuffer[HEADER_LENGTH];
        //read 8 header bytes and get IP, port and packet length from it
        if (Ethernet.socketRecv(sockindex, headerBuffer, HEADER_LENGTH) == HEADER_LENGTH) {
            _remoteIP   = headerBuffer;
            _remotePort = headerBuffer[4];
            _remotePort = (_remotePort << 8) + headerBuffer[5];
            _bytesInBuffer  = headerBuffer[6];
            _bytesInBuffer  = (_bytesInBuffer << 8) + headerBuffer[7];
            // Return amount of available data
            return _bytesInBuffer;
        }
        // No data or truncated packet
        flush();		// The socket pointer is somewhere
    }
    _bytesInBuffer = 0;
    return 0;
}

// ****************************************************************************
//		Return number of bytes available in the RCV buffer
//		Return 0 if parsePacket hasn't been called yet or bad socket
// ****************************************************************************
int EthernetUDP::available()
{
    if (sockindex >= MAX_SOCK_NUM) return 0;
    return _bytesInBuffer;
}

// ****************************************************************************
//		Read a single byte from the current packet
//		Returns the byte read or -1 if no data available
// ****************************************************************************
int EthernetUDP::read()
{
    uint8_t byte;
	if ( sockindex >= MAX_SOCK_NUM || _bytesInBuffer == 0 ) return -1;
    //Returns the byte read or -1 for no data
    if (Ethernet.socketRecv(sockindex, &byte, 1) > 0 ) {
        _bytesInBuffer--;
        return byte;
    }
    // Data not yet in  buffer
    return -1;

}

// ****************************************************************************
//		Read up to len bytes from the current packet and place them into 'buffer'
//		Returns number of bytes read or -1 no data available
// ****************************************************************************
int EthernetUDP::read(unsigned char *buffer, size_t len)
{
    if ( sockindex >= MAX_SOCK_NUM || _bytesInBuffer == 0 ) return -1;
    int got;
    if (_bytesInBuffer <= len) {
        // data should fit in the buffer
        got = Ethernet.socketRecv(sockindex, buffer, _bytesInBuffer);	//Returns size, or -1 for no data
    } else {
        // too much data for the buffer, grab as much as will fit
        got = Ethernet.socketRecv(sockindex, buffer, len);	//Returns size, or -1 for no data
    }
    //Serial.printf("UDP read %i\n", got);
    if ( got > 0 ) {		//We read something
        _bytesInBuffer -= got;
        return got;
    }
 	// Data not yet in  buffer
    return -1;
}

// ****************************************************************************
//		Return the next byte from the current packet without moving on to the next byte
//		Return MUST be checked as the return value can be -1
// 		Be carreful using this function, can be nowhere...
// ****************************************************************************
int EthernetUDP::peek()
{
    // Unlike recv, peek doesn't check to see if there's any data available.
    // Note that none of the pointers are updated. Read can be anywhere...
    // If the user hasn't called parsePacket yet then return nothing otherwise they
    // may get the UDP header
    if ( sockindex >= MAX_SOCK_NUM || _bytesInBuffer == 0 ) return -1;
    return Ethernet.socketPeek(sockindex);
}

// ****************************************************************************
//		Flush RX buffer
// ****************************************************************************
void EthernetUDP::flush()
{
    const uint16_t WAIT_FOR_FLUSH = 200;		// millis
    if (sockindex >= MAX_SOCK_NUM) return;
    uint32_t stopWait = millis() + WAIT_FOR_FLUSH;
    while ( millis() < stopWait ) {
        int ret = Ethernet.socketRecvAvailable(sockindex);
        if ( Ethernet.socketRecv(sockindex, (uint8_t *)NULL, ret) <= 0 ) break;
    }

    _bytesInBuffer = 0;
}

// ****************************************************************************
//		Start EthernetUDP socket in multicast mode
// ****************************************************************************
uint8_t EthernetUDP::beginMulticast(IPAddress ip, uint16_t port, uint8_t igmpVersion)
{
    if (sockindex < MAX_SOCK_NUM) Ethernet.socketClose(sockindex);
    sockindex = Ethernet.socketBeginMulticast(ip, port, igmpVersion);
    if (sockindex >= MAX_SOCK_NUM) return 0;
    _port = port;
    _bytesInBuffer = 0;
    return 1;
}

