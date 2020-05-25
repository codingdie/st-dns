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

    UdpDnsRequest() = default;

    UdpDnsRequest(vector<std::string> &hosts);

    virtual ~UdpDnsRequest();

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
