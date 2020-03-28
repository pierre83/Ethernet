/* Copyright 2020 Pierre Casal
 *
 * Copyright 2018 Paul Stoffregen
 *
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

#include <Arduino.h>
#include "Ethernet.h"
#include "Dns.h"
#include "utility/w5100.h"


// ****************************************************************************
// Connect to a host name, use DNS to request IP address
// Returns 0= fail , 1= success, -x= DNS error code(-1 to -8)
// ****************************************************************************
int EthernetClient::connect(const char * host, uint16_t port)
{
    DNSClient dns;
    IPAddress remote_addr;
    int ret = dns.begin(Ethernet.dnsServerIP());
    if (ret != 1) return ret;		// Wrong DNS
    ret = dns.getHostByName(host, remote_addr, DNS_TIMEOUT);
    if (ret != 1) return ret;		// DNS resolution did not work
    return connect(remote_addr, port);
}

// ****************************************************************************
// Connect to a XXX.XXX.XXX.XXX IP address
// Returns 0= fail , 1= success, 2= already connected, -11= bad @IP, -12= no socket
// ****************************************************************************
int EthernetClient::connect(IPAddress ip, uint16_t port)
{
    // Check for 'valid' IP address
    if (ip == IPAddress((uint32_t)0) || ip == IPAddress(0xFFFFFFFFul)) return -11;
    // This socket should be closed by the W5x00, or the sketch called it twice...
    if (sockindex < MAX_SOCK_NUM) {
        // The sketch must take the appropriate action in this case
        if (Ethernet.socketStatus(sockindex) == SnSR::ESTABLISHED ) return 2;
        // Any other status is not "normal"
        Ethernet.socketClose(sockindex);
    }
    // Ask for a new socket
    sockindex = Ethernet.socketBegin(SnMR::TCP, 0);
    if (sockindex >= MAX_SOCK_NUM) return -12;
    // Connect to the remote end
    return Ethernet.socketConnect(sockindex, rawIPAddress(ip), port);
}

// ****************************************************************************
// Check for empty space in TX buffer
// Returns: freesize, -1= not ESTABLISHED and not CLOSE_WAIT, 0= buffer full
// ****************************************************************************
int EthernetClient::availableForWrite(void)
{
    if (sockindex >= MAX_SOCK_NUM) return 0;
    return Ethernet.socketSendAvailable(sockindex);
}

// ****************************************************************************
// Write a byte
// Returns 1= byte sent, 0= timeout/fail, 65535= W5x00 failed
// ****************************************************************************
size_t EthernetClient::write(uint8_t b)
{
    return write(&b, 1);
}

// ****************************************************************************
// Write the provided buffer in TX buffer
// Returns	bytes written, 0 = timeout/fail, 65535 = W5x00 failed
// ****************************************************************************
size_t EthernetClient::write(const uint8_t *buf, size_t size)
{
    if (sockindex >= MAX_SOCK_NUM) return 0;
    int ret = Ethernet.socketSend(sockindex, buf, size);
    if ( ret > 0 ) return ret;
    setWriteError();
    return ret;
}

// ****************************************************************************
//	Returns the number of bytes available for read or 0 if none are available
// ****************************************************************************
int EthernetClient::available()
{
    if (sockindex >= MAX_SOCK_NUM) return 0;
    return Ethernet.socketRecvAvailable(sockindex);
    //--------------------------------------------------------------------------
    // ACK packet are not re-sent as there is no way to know it was not received.
    // If the remote does'nt receive our ACK packet it normaly should transmit
    // again the packet and in the next recv we must look at what we received..
    // and send another ACK and perhaps disgard the packet...
}

// ****************************************************************************
// Read "size" bytes in the provided buffer
// Returns number of bytes read, -1= no data/invalid socket, 0= connection closed
// ****************************************************************************
int EthernetClient::read(uint8_t *buf, size_t size)
{
    if (sockindex >= MAX_SOCK_NUM) return 0;
    return Ethernet.socketRecv(sockindex, buf, size);
}

// ****************************************************************************
// Read a byte and returns a data (0-255)
// No verification
// ****************************************************************************
int EthernetClient::peek()
{
    if (!available()) return -1;
    return Ethernet.socketPeek(sockindex);
}

// ****************************************************************************
// Read a byte and returns a data (0-255) or -1 for no data/problem
// Return MUST BE CHECKED because this function can return -1
// ****************************************************************************
int EthernetClient::read()
{
    uint8_t b;
    if (sockindex >= MAX_SOCK_NUM) return -1;
    if (Ethernet.socketRecv(sockindex, &b, 1) > 0) return b;
    return -1;
}

// ****************************************************************************
// Wait until the TX buffer is emptied(1) or the connection is closed(2)
// ****************************************************************************
void EthernetClient::flush()
{
    const uint16_t WAIT_FOR_FLUSH = 300;
    if (sockindex >= MAX_SOCK_NUM) return;
    uint32_t stop_wait = millis() + WAIT_FOR_FLUSH;
    while ( millis() < stop_wait ) {
        // Return: freesize, -1= not connected,	0= buffer full
        int freeSendBuffer = Ethernet.socketSendAvailable(sockindex);
        // not ESTABLISHED and not CLOSE_WAIT
        if ( freeSendBuffer < 0 ) return; // (2)
        // Connected and buffer empty
        if ( freeSendBuffer >= (int)W5100.SSIZE) return; // (1)
        yield();
    }
}

// ****************************************************************************
// Attempt to close the connection gracefully (send a FIN to other side)
// The rest of the closing process is managed by the W5x00
// Once the FIN is sent we cannot use the pipe anymore to send datas
// ****************************************************************************
void EthernetClient::stop()
{
    if (sockindex >= MAX_SOCK_NUM) return;
    Ethernet.socketDisconnect(sockindex);
    sockindex = MAX_SOCK_NUM;
}

// ****************************************************************************
// Test if client is connected
// Returns: 1= connected, 0= not connected
// ****************************************************************************
uint8_t EthernetClient::connected()
{
    if (sockindex >= MAX_SOCK_NUM) return 0;
    switch ( Ethernet.socketStatus(sockindex) ) {
    case SnSR::ESTABLISHED:
        return 1;
        break;
    case SnSR::CLOSE_WAIT:
        if ( Ethernet.socketRecvAvailable(sockindex) ) return 1;
        break;
    }
    return 0;
}

// ****************************************************************************
// Returns the socket status
// ****************************************************************************
uint8_t EthernetClient::status()
{
    if (sockindex >= MAX_SOCK_NUM) return SnSR::CLOSED;
    return Ethernet.socketStatus(sockindex);
}

// ****************************************************************************
// This function allows us to use the client returned by
// EthernetServer::available() as the condition in an if-statement.
// ****************************************************************************
bool EthernetClient::operator==(const EthernetClient& rhs)
{
    if (sockindex != rhs.sockindex) return false;
    if (sockindex >= MAX_SOCK_NUM) return false;
    return true;
}

// https://github.com/per1234/EthernetMod
// from: https://github.com/ntruchsess/Arduino-1/commit/937bce1a0bb2567f6d03b15df79525569377dabd
uint16_t EthernetClient::localPort()
{
    if (sockindex >= MAX_SOCK_NUM) return 0;
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    uint16_t port = W5100.readSnPORT(sockindex);
    SPI.endTransaction();
    return port;
}

// https://github.com/per1234/EthernetMod
// returns the remote IP address: http://forum.arduino.cc/index.php?topic=82416.0
IPAddress EthernetClient::remoteIP()
{
    if (sockindex >= MAX_SOCK_NUM) return IPAddress((uint32_t)0);
    uint8_t remoteIParray[4];
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.readSnDIPR(sockindex, remoteIParray);
    SPI.endTransaction();
    return IPAddress(remoteIParray);
}

// https://github.com/per1234/EthernetMod
// from: https://github.com/ntruchsess/Arduino-1/commit/ca37de4ba4ecbdb941f14ac1fe7dd40f3008af75
uint16_t EthernetClient::remotePort()
{
    if (sockindex >= MAX_SOCK_NUM) return 0;
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    uint16_t port = W5100.readSnDPORT(sockindex);
    SPI.endTransaction();
    return port;
}
