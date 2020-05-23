//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_DNS_DNSSERVER_H
#define ST_DNS_DNSSERVER_H

#include "Config.h"
#include "TCPSession.h"

class DNSServer {
public:
    DNSServer(const Config &config);

    void start();

private:
    Config config;
    std::map<long, TCPSession *> tcpSessions;
};


#endif //ST_DNS_DNSSERVER_H
