//
// Created by System Administrator on 2020/10/11.
//

#ifndef ST_DNS_WHOISCLIENT_H
#define ST_DNS_WHOISCLIENT_H

#include "STUtils.h"
#include "asio/STUtils.h"

class WhoisClient {
public:
    static string whois(const string &domain, const string &whoisServerIp, const u_int16_t &whoisServerPort, const u_int32_t &timeout);
};


#endif //ST_DNS_WHOISCLIENT_H
