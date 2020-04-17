// Arduino DNS client for WizNet5100-based Ethernet shield
// Copyright 2020 Pierre Casal
// (c) Copyright 2009-2010 MCQN Ltd.
// Released under Apache License, version 2.0

#ifndef DNSClient_h
#define DNSClient_h

#include "Ethernet.h"

// if no timeout is provided with getHostByName(...), then 2000 milliseconds
#define	DNS_TIMEOUT		2000

class DNSClient
{
public:

	/** Start the instance with the DNS server
	    @param IP address of the configured DNS server
	    @param
	    @result SUCCESS or INVALID_SERVER
	*/
	int begin(const IPAddress& aDNSServer);

	/** Convert a numeric IP address string into a four-byte IP address.
	    @param aIPAddrString IP address to convert
	    @param aResult IPAddress structure to store the returned IP address
	    @result true if aIPAddrString was successfully converted to an IP address,
	            else false
	*/
	bool inet_aton(const char *aIPAddrString, IPAddress& aResult);

	/** Resolve the given hostname to an IP address.
	    @param aHostname Name to be resolved
	    @param aResult IPAddress structure to store the returned IP address
	    @result 1 if aIPAddrString was successfully converted to an IP address,
	            else error code (-1 to -8)
	*/
	int getHostByName(const char* aHostname, IPAddress& aResult, uint16_t timeout = DNS_TIMEOUT);

protected:

	uint16_t randomNumber();
	uint16_t BuildRequest(const char* aName);
	uint16_t ProcessResponse( uint16_t iRequestId, IPAddress& aAddress, int packetSize);
	IPAddress dnsDNSServer;
	EthernetUDP dnsUdp;
};

#endif
