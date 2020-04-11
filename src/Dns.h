// Arduino DNS client for WizNet5100-based Ethernet shield
// Copyright 2020 Pierre Casal
// (c) Copyright 2009-2010 MCQN Ltd.
// Released under Apache License, version 2.0

#ifndef DNSClient_h
#define DNSClient_h

#include "Ethernet.h"

#define	DNS_TIMEOUT		2000

class DNSClient
{
public:

	/** Start the instance with the DNS server
	    @param IP address of the configured DNS server
	    @param
	    @result none
	*/
	int begin(const IPAddress& aDNSServer);

	/** Convert a numeric IP address string into a four-byte IP address.
	    @param aIPAddrString IP address to convert
	    @param aResult IPAddress structure to store the returned IP address
	    @result 1 if aIPAddrString was successfully converted to an IP address,
	            else error code
	*/
	bool inet_aton(const char *aIPAddrString, IPAddress& aResult);

	/** Resolve the given hostname to an IP address.
	    @param aHostname Name to be resolved
	    @param aResult IPAddress structure to store the returned IP address
	    @result 1 if aIPAddrString was successfully converted to an IP address,
	            else error code
	*/
	int getHostByName(const char* aHostname, IPAddress& aResult, uint16_t timeout = DNS_TIMEOUT);

protected:

	uint16_t randomNumber();
	uint16_t BuildRequest(const char* aName);
	uint16_t ProcessResponse( uint16_t iRequestId, IPAddress& aAddress);
	IPAddress dnsDNSServer;
	EthernetUDP dnsUdp;
};

#endif
