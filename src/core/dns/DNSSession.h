//
// Created by codingdie on 2020/5/28.
//

#ifndef ST_DNS_DNSSESSION_H
#define ST_DNS_DNSSESSION_H

#include "STUtils.h"
#include "DNS.h"
#include <boost/asio.hpp>
#include "RemoteDNSServer.h"

class DNSSession {
public:
    boost::asio::ip::udp::endpoint clientEndpoint;
    UdpDnsRequest udpDnsRequest = UdpDnsRequest(1024);
    DNSRecord record;
    bool forward = false;
    UdpDNSResponse *udpDNSResponse = nullptr;

    DNSSession(uint64_t id);

    vector<RemoteDNSServer *> servers;

    virtual ~DNSSession();

private:
    uint64_t id;
    uint64_t time = 0;

public:
    uint64_t getId() const;

    string getHost() const;

    DNSQuery::Type getQueryType() const;

    void setTime(uint64_t time);

    uint64_t getTime() const;
};


#endif //ST_DNS_DNSSESSION_H
