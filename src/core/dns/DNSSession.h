//
// Created by codingdie on 2020/5/28.
//

#ifndef ST_DNS_DNSSESSION_H
#define ST_DNS_DNSSESSION_H

#include "DNS.h"
#include "RemoteDNSServer.h"
#include "utils/STUtils.h"
#include <boost/asio.hpp>
class DNSSession {

private:
    uint64_t id;
    uint64_t time = 0;

public:
    enum ProcessType {
        QUERY = 1,
        FORWARD = 2,
        DROP = 3
    };
    APMLogger apmLogger;
    boost::asio::ip::udp::endpoint clientEndpoint;
    UdpDnsRequest udpDnsRequest;
    DNSRecord record;
    ProcessType processType = QUERY;
    UdpDNSResponse *udpDNSResponse = nullptr;
    vector<RemoteDNSServer *> servers;

    DNSSession(uint64_t id);

    virtual ~DNSSession();


    uint64_t getId() const;

    string getHost() const;

    DNSQuery::Type getQueryType() const;

    uint16_t getQueryTypeValue() const;

    void setTime(uint64_t time);

    uint64_t getTime() const;
};


#endif//ST_DNS_DNSSESSION_H
