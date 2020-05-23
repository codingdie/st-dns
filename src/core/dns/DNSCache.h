//
// Created by codingdie on 2020/5/24.
//

#ifndef ST_DNS_DNS_CACHE_H
#define ST_DNS_DNS_CACHE_H

#include <vector>
#include <map>
#include <iostream>
#include "DNS.h"

using namespace std;

class DNSCache {
private:
    static DNSCache INSTANCE;
    map<string, vector<uint32_t> *> caches;
public:
    void addCache(UdpDNSResponse *response);

    int32_t query(string &host);
};


#endif //ST_DNS_DNS_CACHE_H
