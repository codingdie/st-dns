//
// Created by codingdie on 2020/5/21.
//

#ifndef ST_DNS_DNSREQUEST_H
#define ST_DNS_DNSREQUEST_H

#include <iostream>
#include "DNSMessage.h"


using namespace std;

class UdpDnsRequest : public BasicData {
public:
    DNSHeader *dnsHeader = nullptr;
    DNSQueryZone *dnsQueryZone = nullptr;
    std::vector<string> hosts;

    UdpDnsRequest() = default;

    UdpDnsRequest(vector<std::string> &hosts);

    UdpDnsRequest(byte *data, uint64_t len);

    UdpDnsRequest(byte *data, uint64_t len, bool dataOwner);

    explicit UdpDnsRequest(uint64_t len);

    virtual ~UdpDnsRequest();

    bool parse();

    string getFirstHost();

protected:
    virtual void initDataZone();

};

class TcpDnsRequest : public UdpDnsRequest {
public:
    explicit TcpDnsRequest(vector<std::string> &hosts);

protected:
    void initDataZone() override;
};


#endif //ST_DNS_DNSREQUEST_H
