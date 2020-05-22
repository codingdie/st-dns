//
// Created by codingdie on 2020/5/20.
//

#ifndef ST_DNS_DNSCLIENT_H
#define ST_DNS_DNSCLIENT_H

#include <string>


class DNSClient {

public:
    static std::string udpDns(std::string domain, std::string dnsServer);
};


#endif //ST_DNS_DNSCLIENT_H
