//
// Created by codingdie on 2020/5/24.
//

#ifndef ST_DNS_DNS_CACHE_H
#define ST_DNS_DNS_CACHE_H

#include <set>
#include <map>
#include <iostream>
#include "STUtils.h"
#include <mutex>

using namespace std;

class DNSCache {
private:
    static DNSCache INSTANCE;
    map<string, map<string, set<uint32_t> *> *> caches;
public:

    static void addCache(const string &domain, const set<uint32_t> &ips, const string &dnsServer);

    static set<uint32_t> query(const string &host);
};


#endif //ST_DNS_DNS_CACHE_H
