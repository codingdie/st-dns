//
// Created by codingdie on 2020/5/28.
//

#ifndef ST_DNS_DNSSESSION_H
#define ST_DNS_DNSSESSION_H

#include "DNS.h"
#include "RemoteDNSServer.h"
#include "STUtils.h"
#include <boost/asio.hpp>
class DNSSession {
public:
    enum ProcessType {
        QUERY = 1,
        FORWARD = 2,
        DROP = 3
    };
    boost::asio::ip::udp::endpoint clientEndpoint;
    UdpDnsRequest udpDnsRequest;
    DNSRecord record;
    ProcessType processType = QUERY;
    UdpDNSResponse *udpDNSResponse = nullptr;

    DNSSession(uint64_t id);

    vector<RemoteDNSServer *> servers;
    virtual ~DNSSession();
    APMLogger stepLogger;

private:
    uint64_t id;
    uint64_t time = 0;

public:
    uint64_t getId() const;

    string getHost() const;

    DNSQuery::Type getQueryType() const;
    uint16_t getQueryTypeValue() const;

    void setTime(uint64_t time);

    uint64_t getTime() const;
};


#endif//ST_DNS_DNSSESSION_H
