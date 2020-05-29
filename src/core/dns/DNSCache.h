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
    static void addCache(UdpDNSResponse *response);

    static uint32_t queryRandom(const string &host);

    static vector<uint32_t> query(const string &host);
};


#endif //ST_DNS_DNS_CACHE_H
