//
// Created by codingdie on 2020/5/21.
//

#ifndef ST_DNS_DNSREQUEST_H
#define ST_DNS_DNSREQUEST_H

#include "DNSMessage.h"
#include <iostream>


using namespace std;

class UdpDnsRequest : public BasicData {
public:
    DNSHeader *dnsHeader = nullptr;
    DNSQueryZone *dnsQueryZone = nullptr;
    std::vector<string> hosts;


    UdpDnsRequest(const vector<std::string> &hosts);


    UdpDnsRequest(uint8_t *data, uint64_t len, bool dataOwner);

    UdpDnsRequest(uint64_t len);

    UdpDnsRequest();

    virtual ~UdpDnsRequest();

    bool parse(int n);

    string getHost() const;
    DNSQuery::Type getQueryType() const;
    uint16_t getQueryTypeValue() const;

protected:
    virtual void initDataZone();
};

class EDNSAdditionalZone : public BasicData {

public:
    EDNSAdditionalZone(uint32_t len);

    static EDNSAdditionalZone *generate(uint32_t ip);
};

class TcpDnsRequest : public UdpDnsRequest {
public:
    explicit TcpDnsRequest(const vector<std::string> &hosts);

    explicit TcpDnsRequest(const vector<std::string> &hosts, uint32_t ip);

    virtual ~TcpDnsRequest();

protected:
    void initDataZone() override;

private:
    EDNSAdditionalZone *ENDSZone = nullptr;
};

#endif//ST_DNS_DNSREQUEST_H
