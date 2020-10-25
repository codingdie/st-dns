//
// Created by codingdie on 2020/5/28.
//

#ifndef ST_DNS_DNSSESSION_H
#define ST_DNS_DNSSESSION_H

#include "STUtils.h"
#include "DNS.h"
#include <boost/asio.hpp>

class DNSSession {
public:
    boost::asio::ip::udp::endpoint clientEndpoint;
    UdpDnsRequest udpDnsRequest = UdpDnsRequest(1024);

    DNSSession(uint64_t id);

    virtual ~DNSSession();

private:
    uint64_t id;

public:
    uint64_t getId() const;

    string getHost() const;

};


#endif //ST_DNS_DNSSESSION_H
