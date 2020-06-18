//
// Created by codingdie on 2020/5/28.
//

#ifndef ST_DNS_DNSSESSION_H
#define ST_DNS_DNSSESSION_H

#include "DNS.h"
#include <boost/asio.hpp>

class DNSSession {
public:
    boost::asio::ip::udp::endpoint clientEndpoint;
    UdpDnsRequest udpDnsRequest = UdpDnsRequest(1024);

    DNSSession();

    virtual ~DNSSession();

};


#endif //ST_DNS_DNSSESSION_H
