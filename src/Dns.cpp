// Arduino DNS client for WizNet5100-based Ethernet shield

// Copyright 2020 Pierre Casal

// (c) Copyright 2009-2010 MCQN Ltd.
// Released under Apache License, version 2.0

#include <Arduino.h>
#include "Ethernet.h"
#include "Dns.h"
#include "utility/w5100.h"

//#define DEBUG

// Various flags and header field values for a DNS message
#define UDP_HEADER_SIZE          8
#define DNS_HEADER_SIZE          12
#define TTL_SIZE                 4
#define QUERY_FLAG               (0)
#define RESPONSE_FLAG            (1<<15)
#define QUERY_RESPONSE_MASK      (1<<15)
#define OPCODE_STANDARD_QUERY    (0)
#define OPCODE_INVERSE_QUERY     (1<<11)
#define OPCODE_STATUS_REQUEST    (2<<11)
#define OPCODE_MASK              (15<<11)
#define AUTHORITATIVE_FLAG       (1<<10)
#define TRUNCATION_FLAG          (1<<9)
#define RECURSION_DESIRED_FLAG   (1<<8)
#define RECURSION_AVAILABLE_FLAG (1<<7)
#define RESP_NO_ERROR            (0)
#define RESP_FORMAT_ERROR        (1)
#define RESP_SERVER_FAILURE      (2)
#define RESP_NAME_ERROR          (3)
#define RESP_NOT_IMPLEMENTED     (4)
#define RESP_REFUSED             (5)
#define RESP_MASK                (15)
#define TYPE_A                   (0x0001)
#define CLASS_IN                 (0x0001)
#define LABEL_COMPRESSION_MASK   (0xC0)

// Port number that DNS servers listen on
#define DNS_PORT        53
// Number of retries
#define MAX_RETRIES		2			// Warning if watchdog is used

// Possible return codes
#define SUCCESS          1
#define TIMED_OUT        -1
#define INVALID_SERVER   -2
#define TRUNCATED        -3
#define INVALID_RESPONSE -4
#define BAD_RESPONSE	 -5
#define INV_RESPONSE 	 -6
#define BAD_SIZE		 -7
#define NO_ANSWER		 -8


// *******************************************************
// Start the DNS process, check for correct IP address
// *******************************************************
int DNSClient::begin(const IPAddress& aDNSServer)
{
    dnsDNSServer = aDNSServer;

    // Check we've got a valid DNS server to use
    if (dnsDNSServer == IPAddress((uint32_t)0) ) {
        return INVALID_SERVER;
    }
    return SUCCESS;
}

// *******************************************************
// Verify if the given string is an IP address or a server name
// *******************************************************
bool DNSClient::inet_aton(const char* address, IPAddress& result)
{
    uint16_t acc = 0; // Accumulator
    uint8_t dots = 0;

    while (*address) {
        char c = *address++;
        if (c >= '0' && c <= '9') {
            acc = acc * 10 + (c - '0');
            if (acc > 255) {
                // Value out of [0..255] range
                return false;
            }
        } else if (c == '.') {
            if (dots == 3) {
                // Too much dots (there must be 3 dots)
                return false;
            }
            result[dots++] = acc;
            acc = 0;
        } else {
            // Invalid char
            return false;
        }
    }

    if (dots != 3) {
        // Too few dots (there must be 3 dots)
        return false;
    }
    result[3] = acc;
    //Serial.print("@IP: ");    //Serial.println(result);
    return true;
}

// *******************************************************
// Request the IP address of a given server name
// *******************************************************
int DNSClient::getHostByName(const char* aHostname, IPAddress& dnsResult, uint16_t timeout)
{
	// Verify timeouts
    uint16_t operation_timeout = DNS_TIMEOUT;
    if ( timeout < REPLY_TIMEOUT ) {
        timeout = REPLY_TIMEOUT;
        operation_timeout = REPLY_TIMEOUT;
    }
    // See if it's a numeric IP address
    if (inet_aton(aHostname, dnsResult)) {
        // It is, our work here is done
        return SUCCESS;
    }

    int ret = 0;
    int packetSize = 0;

    uint32_t endWait = millis() + operation_timeout;

    while ( millis() < endWait ) {
        // Find a socket to use
        ret = dnsUdp.begin();
        if ( ret == SUCCESS ) {
			//Serial.println("dnsUdp.begin()");
            // Successful get socket
            ret = dnsUdp.beginPacket(dnsDNSServer, DNS_PORT);
            if ( ret == SUCCESS ) {
				//Serial.println("dnsUdp.beginPacket()");
                // Build the request
                uint16_t iRequestId = BuildRequest(aHostname);
                // Send the request
                ret = dnsUdp.endPacket();
                if ( ret == SUCCESS ) {
					//Serial.println("dnsUdp.endPacket()");
                    // Wait for a response packet
                    ret = TIMED_OUT;
                    uint32_t stopWait = millis() + timeout;
                    while ( millis() < stopWait ) {
                        packetSize = dnsUdp.parsePacket();
                        if ( packetSize > 0 ) {
							//Serial.println("dnsUdp.parsePacket()");
                            // We've got something
                            if ( ( dnsUdp.remoteIP() != dnsDNSServer ) || ( dnsUdp.remotePort() != DNS_PORT ) ) {
                                // It's not from who we expected
                                ret = INVALID_SERVER;
                            } else {
                                // Process the packet
                                ret = ProcessResponse(iRequestId, dnsResult);
                            }
                            break;	// exit if something received
                        }
                        delay(5);
                    }
                }
            }
            // Close the socket now
            dnsUdp.stop();
            if ( ret == SUCCESS ) break;
		}
        delay(100);   // Between retries
    }
#if 0
    uint32_t stop = millis();
    Serial.print("@IP\t");		Serial.println(dnsResult);
    Serial.print("Time\t");		Serial.print(stop - (endWait-operation_timeout));
    Serial.print("ms\nResult\t");	Serial.println(ret);
#endif
    return ret;
}


// *******************************************************
// Build the request sent
// *******************************************************
uint16_t DNSClient::BuildRequest(const char* aName)
{
    // Build header
    //                                    1  1  1  1  1  1
    //      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    //    |                      ID                       |
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    //    |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    //    |                    QDCOUNT                    |
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    //    |                    ANCOUNT                    |
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    //    |                    NSCOUNT                    |
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    //    |                    ARCOUNT                    |
    //    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    // As we only support one request at a time at present, we can simplify
    // some of this header

    uint16_t iRequestId = 1024 + (millis() & 0x03FF);// generate a 'random'ID
    uint16_t twoByteBuffer;

    // FIXME We should also check that there's enough space available to write to, rather
    // FIXME than assume there's enough space (as the code does at present)
    dnsUdp.write((uint8_t*)&iRequestId, sizeof(iRequestId));

    uint16_t tempo = (QUERY_FLAG | OPCODE_STANDARD_QUERY | RECURSION_DESIRED_FLAG);
    twoByteBuffer = htons(tempo);//Because arduino boards.txt level
    dnsUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

    twoByteBuffer = htons(1);  // One question record
    dnsUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

    twoByteBuffer = 0;  // Zero answer records
    dnsUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

    dnsUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));
    // and zero additional records
    dnsUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

    // Build question
    const char* start =aName;
    const char* end =start;
    uint8_t len;
    // Run through the name being requested
    while (*end) {
        // Find out how long this section of the name is
        end = start;
        while (*end && (*end != '.') ) {
            end++;
        }

        if (end-start > 0) {
            // Write out the size of this section
            len = end-start;
            dnsUdp.write(&len, sizeof(len));
            // And then write out the section
            dnsUdp.write((uint8_t*)start, end-start);
        }
        start = end+1;
    }

    // We've got to the end of the question name, so
    // terminate it with a zero-length section
    len = 0;
    dnsUdp.write(&len, sizeof(len));
    // Finally the type and class of question
    twoByteBuffer = htons(TYPE_A);
    dnsUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

    twoByteBuffer = htons(CLASS_IN);  // Internet class of question
    dnsUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));
    // Success!  Everything buffered okay
    return iRequestId;
}

// *******************************************************
// Analyze the DNS answer
// *******************************************************
uint16_t DNSClient::ProcessResponse(uint16_t iRequestId, IPAddress& aAddress)
{
    // We've had a reply!
    // Read the UDP header
    union {
        uint8_t  byte[DNS_HEADER_SIZE]; // Enough space to reuse for the DNS header
        uint16_t word[DNS_HEADER_SIZE/2];
    } header;

    // Check that it's a response from the right server and the right port
    if ( (dnsDNSServer != dnsUdp.remoteIP()) || (dnsUdp.remotePort() != DNS_PORT) ) {
        // It's not from who we expected
        return INVALID_SERVER;
    }

    // Read through the rest of the response
    if (dnsUdp.available() < DNS_HEADER_SIZE) {
        return TRUNCATED;
    }
    dnsUdp.read(header.byte, DNS_HEADER_SIZE);

    uint16_t header_flags = htons(header.word[1]);
    // Check that it's a response to this request
    if ((iRequestId != (header.word[0])) ||
            ((header_flags & QUERY_RESPONSE_MASK) != (uint16_t)RESPONSE_FLAG) ) {
        // Mark the entire packet as read
        dnsUdp.flush();
        return INVALID_RESPONSE;
    }
    // Check for any errors in the response (or in our request)
    // although we don't do anything to get round these
    if ( (header_flags & TRUNCATION_FLAG) || (header_flags & RESP_MASK) ) {
        // Mark the entire packet as read
        dnsUdp.flush();
        return BAD_RESPONSE;
    }

    // And make sure we've got (at least) one answer
    uint16_t answerCount = htons(header.word[3]);
    if (answerCount == 0) {
        // Mark the entire packet as read
        dnsUdp.flush();
        return INV_RESPONSE;
    }

    // Skip over any questions
    for (uint16_t i=0; i < htons(header.word[2]); i++) {
        // Skip over the name
        uint8_t len;
        do {
            dnsUdp.read(&len, sizeof(len));
            if (len > 0) {
                // Don't need to actually read the data out for the string, just
                // advance ptr to beyond it
                dnsUdp.read((uint8_t *)NULL, (size_t)len);
            }
        } while (len != 0);

        // Now jump over the type and class
        dnsUdp.read((uint8_t *)NULL, 4);
    }

    // Now we're up to the bit we're interested in, the answer
    // There might be more than one answer (although we'll just use the first
    // type A answer) and some authority and additional resource records but
    // we're going to ignore all of them.

    for (uint16_t i=0; i < answerCount; i++) {
        // Skip the name
        uint8_t len;
        do {
            dnsUdp.read(&len, sizeof(len));
            if ((len & LABEL_COMPRESSION_MASK) == 0) {
                // It's just a normal label
                if (len > 0) {
                    // And it's got a length
                    // Don't need to actually read the data out for the string,
                    // just advance ptr to beyond it
                    dnsUdp.read((uint8_t *)NULL, len);
                }
            } else {
                // This is a pointer to a somewhere else in the message for the
                // rest of the name.  We don't care about the name, and RFC1035
                // says that a name is either a sequence of labels ended with a
                // 0 length octet or a pointer or a sequence of labels ending in
                // a pointer.  Either way, when we get here we're at the end of
                // the name
                // Skip over the pointer
                dnsUdp.read((uint8_t *)NULL, 1); // we don't care about the byte
                // And set len so that we drop out of the name loop
                len = 0;
            }
        } while (len != 0);

        // Check the type and class
        uint16_t answerType;
        uint16_t answerClass;
        dnsUdp.read((uint8_t*)&answerType, sizeof(answerType));
        dnsUdp.read((uint8_t*)&answerClass, sizeof(answerClass));

        // Ignore the Time-To-Live as we don't do any caching
        dnsUdp.read((uint8_t *)NULL, TTL_SIZE); // don't care about the returned bytes

        // And read out the length of this answer
        // Don't need header_flags anymore, so we can reuse it here
        dnsUdp.read((uint8_t*)&header_flags, sizeof(header_flags));

        if ( (htons(answerType) == TYPE_A) && (htons(answerClass) == CLASS_IN) ) {
            if (htons(header_flags) != 4) {
                // It's a weird size
                // Mark the entire packet as read
                dnsUdp.flush();
                return BAD_SIZE;
            }
            dnsUdp.read(aAddress.raw_address(), 4);
            return SUCCESS;
        } else {
            // This isn't an answer type we're after, move onto the next one
            dnsUdp.read((uint8_t *)NULL, htons(header_flags));
        }
    }

    // Mark the entire packet as read
    dnsUdp.flush();

    // If we get here then we haven't found an answer
    return NO_ANSWER;
}

