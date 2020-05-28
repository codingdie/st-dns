//
// Created by codingdie on 2020/5/28.
//

#ifndef ST_DNS_DNSSESSION_H
#define ST_DNS_DNSSESSION_H

#include "DNS.h"
#include <boost/asio.hpp>

class DNSSession {
public:
    boost::asio::ip::udp::endpoint *clientEndpoint = nullptr;
    UdpDnsRequest *udpDnsRequest = nullptr;

    DNSSession(UdpDnsRequest *udpDnsRequest, boost::asio::ip::udp::endpoint clientEndpoint);

    virtual ~DNSSession();

};


#endif //ST_DNS_DNSSESSION_H
