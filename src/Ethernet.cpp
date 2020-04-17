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
#include "utility/w5100.h"
#include "Dhcp.h"

IPAddress EthernetClass::_dnsServerAddress;
DhcpClass* EthernetClass::_dhcp = NULL;

// ****************************************************************************
// Initialize the W5x00, get IP parameters through DHCP then configure
// the W5x00 with the returned IP parameters
// Return 1: DHCP & config successfull, 0: DHCP fail, -1: W5x00 failed
// ****************************************************************************
int EthernetClass::begin(uint8_t *mac, unsigned long timeout)//, unsigned long responseTimeout)
{
    static DhcpClass s_dhcp;
    _dhcp = &s_dhcp;

    // Initialise the basic network infos
	if (W5100.init() == 0) {
		return -1;		// W5x00 Init: fail, not found, etc..
	}
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.setMACAddress(mac);
    W5100.setIPAddress(IPAddress(0,0,0,0).raw_address());
    SPI.endTransaction();

    // Now try to get our config info from a DHCP server
    int ret = _dhcp->beginWithDHCP(mac, timeout);//, responseTimeout);
    if (ret == 1) {
        // We've successfully found a DHCP server and got our configuration
        // infos, so set things accordingly
        setDHCPParameters();
    }
    return ret;
}

// ****************************************************************************
// Start configuring IP parameters
// Return 1: success, -1: W5x00 failed
// ****************************************************************************
int EthernetClass::begin(uint8_t *mac, IPAddress ip)
{
    // Assume the DNS server will be the machine on the same network as the local IP
    // but with last octet being '1'
    IPAddress dns = ip;
    dns[3] = 1;
    return begin(mac, ip, dns);
}

// ****************************************************************************
// Continue/start to configure IP parameters
// Return 1: success, -1: W5x00 failed
// ****************************************************************************
int EthernetClass::begin(uint8_t *mac, IPAddress ip, IPAddress dns)
{
    // Assume the gateway will be the machine on the same network as the local IP
    // but with last octet being '1'
    IPAddress gateway = ip;
    gateway[3] = 1;
    return begin(mac, ip, dns, gateway);
}

// ****************************************************************************
// Continue/start to configure IP parameters
// Return 1: success, -1: W5x00 failed
// ****************************************************************************
int EthernetClass::begin(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway)
{
    // Assume the network is a class C network
    IPAddress subnet(255, 255, 255, 0);
    return begin(mac, ip, dns, gateway, subnet);
}

// ****************************************************************************
// Continue/start to configure IP parameters
// Return 1: success, -1: W5x00 failed
// ****************************************************************************
int EthernetClass::begin(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet)
{
    if (W5100.init() == 0) {
		return -1;		// W5x00 Init
	}
    setMACAddress(mac);
    setNetworkParameters( ip, dns, gateway, subnet);
    return 1;
}

// ****************************************************************************
// Get DHCP IP parameters then ask to configure the W5x00
// ****************************************************************************
void EthernetClass::setDHCPParameters()
{
    IPAddress ip = _dhcp->getLocalIp().raw_address();
    IPAddress gateway = _dhcp->getGatewayIp().raw_address();
    IPAddress subnet = _dhcp->getSubnetMask().raw_address();
    IPAddress dns = _dhcp->getDnsServerIp().raw_address();
    setNetworkParameters( ip, dns, gateway, subnet);
}

// ****************************************************************************
// Configure the card with given IP parameters (DHCP or manual)
// ****************************************************************************
void EthernetClass::setNetworkParameters(IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet)
{
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	W5100.setIPAddress(ip.raw_address());
	W5100.setGatewayIp(gateway.raw_address());
	W5100.setSubnetMask(subnet.raw_address());
    SPI.endTransaction();
    _dnsServerAddress = dns;
    // Initialize the "random" generator which will provide Ethernet ports number
    randomSeed(micros());
}

void EthernetClass::init(uint8_t sspin)
{
    W5100.setSS(sspin);
}

EthernetLinkStatus EthernetClass::linkStatus()
{
    switch (W5100.getLinkStatus(LINK)) {
    case LINK_ON:
        return LinkON;
    case LINK_OFF:
        return LinkOFF;
    default:
        return Unknown;
    }
}

EthernetLinkStatus EthernetClass::linkSpeed()
{
    switch (W5100.getLinkStatus(SPEED)) {
    case MBS_10:
        return Mbs10;
    case MBS_100:
        return Mbs100;
    default:
        return Unknown;
    }
}

EthernetLinkStatus EthernetClass::linkDuplex()
{
    switch (W5100.getLinkStatus(DUPLEX)) {
    case HALF_DUPLEX:
        return HalfDuplex;
    case FULL_DUPLEX:
        return FullDuplex;
    default:
        return Unknown;
    }
}

EthernetHardwareStatus EthernetClass::hardwareStatus()
{
    switch (W5100.getChip()) {
    case 51:
        return EthernetW5100;
    case 52:
        return EthernetW5200;
    case 55:
        return EthernetW5500;
    default:
        return EthernetNoHardware;
    }
}

int EthernetClass::maintain()
{
    int rc = DHCP_CHECK_NONE;
    if (_dhcp != NULL) {
        // we have a pointer to dhcp, use it
        rc = _dhcp->checkLease();
        switch (rc) {
        case DHCP_CHECK_NONE:
            //nothing done
            break;
        case DHCP_CHECK_RENEW_OK:
        case DHCP_CHECK_REBIND_OK:
            //we might have got a new IP.
            setDHCPParameters();
            break;
        default:
            //this is actually an error, it will retry though
            break;
        }
    }
    return rc;
}


void EthernetClass::MACAddress(uint8_t *mac_address)
{
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.getMACAddress(mac_address);
    SPI.endTransaction();
}

IPAddress EthernetClass::localIP()
{
    IPAddress ret;
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.getIPAddress(ret.raw_address());
    SPI.endTransaction();
    return ret;
}

IPAddress EthernetClass::subnetMask()
{
    IPAddress ret;
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.getSubnetMask(ret.raw_address());
    SPI.endTransaction();
    return ret;
}

IPAddress EthernetClass::gatewayIP()
{
    IPAddress ret;
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.getGatewayIp(ret.raw_address());
    SPI.endTransaction();
    return ret;
}

void EthernetClass::setMACAddress(const uint8_t *mac_address)
{
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.setMACAddress(mac_address);
    SPI.endTransaction();
}

void EthernetClass::setLocalIP(const IPAddress local_ip)
{
    IPAddress ip = local_ip;
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.setIPAddress(ip.raw_address());
    SPI.endTransaction();
}

void EthernetClass::setSubnetMask(const IPAddress subnet)
{
    IPAddress ip = subnet;
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.setSubnetMask(ip.raw_address());
    SPI.endTransaction();
}

void EthernetClass::setGatewayIP(const IPAddress gateway)
{
    IPAddress ip = gateway;
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.setGatewayIp(ip.raw_address());
    SPI.endTransaction();
}

void EthernetClass::setRetransmissionTimeout(uint16_t milliseconds)
{
    if (milliseconds > 6553) milliseconds = 6553;
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.setRetransmissionTime(milliseconds * 10);
    SPI.endTransaction();
}

void EthernetClass::setRetransmissionCount(uint8_t num)
{
    if (num > 7) num = 7;
    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
    W5100.setRetransmissionCount(num);
    SPI.endTransaction();
}


EthernetClass Ethernet;
