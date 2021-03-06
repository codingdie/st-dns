//
// Created by codingdie on 2020/5/22.
//

#ifndef ST_DNS_DNSRESPONSE_H
#define ST_DNS_DNSRESPONSE_H


#include "DNSMessage.h"
#include "DNSRequest.h"
#include <DNSCache.h>
#include <iostream>
using namespace std;
class DNSSession;

class UdpDNSResponse : public BasicData {
public:
    DNSHeader *header = nullptr;
    DNSQueryZone *queryZone = nullptr;
    vector<DNSResourceZone *> answerZones;
    uint32_t answerZonesSize = 0;
    unordered_set<uint32_t> ips;

    UdpDNSResponse(UdpDnsRequest &request, DNSRecord &record);

    UdpDNSResponse(uint8_t *data, uint64_t len);

    UdpDNSResponse(uint64_t len);

    virtual ~UdpDNSResponse();

    void parse(uint64_t maxReadable);

    void print() const override;
};


class TcpDNSResponse : public BasicData {

public:
    uint16_t dataSize = 0;
    UdpDNSResponse *udpDnsResponse = nullptr;

    virtual ~TcpDNSResponse();

    TcpDNSResponse(uint16_t len);

    void parse(uint64_t maxReadable);
};

#endif//ST_DNS_DNSRESPONSE_H
