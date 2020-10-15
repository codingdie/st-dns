//
// Created by codingdie on 2020/5/24.
//

#ifndef ST_DNS_DNS_CACHE_H
#define ST_DNS_DNS_CACHE_H

#include "STUtils.h"


#include <iostream>
#include <mutex>

#include <set>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <mutex>
#include <utility>

using namespace std;

class DNSCacheRecord {

public:
    set<uint32_t> ips;
    uint64_t ctime;
};

class DNSCache {
private:
    static DNSCache INSTANCE;
    unordered_map<string, unordered_map<string, DNSCacheRecord *> *> caches;
public:

    static void addCache(const string &domain, const set<uint32_t> &ips, const string &dnsServer);

    static set<uint32_t> query(const string &host);
};


#endif //ST_DNS_DNS_CACHE_H
