//
// Created by 徐芃 on 2020/5/19.
//

#ifndef ST_DNS_DNSSERVER_H
#define ST_DNS_DNSSERVER_H

#include "Config.h"

class DNSServer {
public:
    DNSServer(const Config &config);

    void start();

private:
    Config config;
};


#endif //ST_DNS_DNSSERVER_H
