//
// Created by codingdie on 2020/5/21.
//

#ifndef ST_DNS_DNSREQUEST_H
#define ST_DNS_DNSREQUEST_H

#include "DNSMessage.h"
#include <iostream>


using namespace std;

class EDNSAdditionalZone : public BasicData {

public:
    EDNSAdditionalZone(uint32_t len);

    static EDNSAdditionalZone *generate(uint32_t ip);
};

class UdpDnsRequest : public BasicData {
public:
    DNSHeader *dnsHeader = nullptr;
    DNSQueryZone *dnsQueryZone = nullptr;
    std::vector<string> hosts;

    EDNSAdditionalZone *ENDSZone = nullptr;

    UdpDnsRequest(const vector<std::string> &hosts);
    UdpDnsRequest(uint8_t *data, const vector<std::string> &hosts);


    UdpDnsRequest(uint8_t *data, uint64_t len, bool dataOwner);

    UdpDnsRequest(uint64_t len);

    UdpDnsRequest();

    virtual ~UdpDnsRequest();

    bool parse(int n);

    string getHost() const;
    DNSQuery::Type getQueryType() const;
    uint16_t getQueryTypeValue() const;

protected:
    void init(uint8_t *data, const vector<std::string> &hosts);
};

class TcpDnsRequest : public UdpDnsRequest {
public:
    const static uint8_t SIZE_HEADER = 2;
    explicit TcpDnsRequest(const vector<std::string> &hosts);

    virtual ~TcpDnsRequest();
};

#endif//ST_DNS_DNSREQUEST_H
