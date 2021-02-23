//
// Created by codingdie on 2020/5/24.
//

#ifndef ST_DNS_DNS_CACHE_H
#define ST_DNS_DNS_CACHE_H

#include "STUtils.h"


#include <iostream>
#include <mutex>

#include <iostream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;

class DNSRecord {

public:
    unordered_set<uint32_t> ips;
    uint64_t expireTime = 0;
    string dnsServer;
    string host = "";
    bool expire = false;
    bool matchArea = false;

    string serialize();
    boost::property_tree::ptree
    toPT();

    bool deserialize(const string &str);
};

class DNSCache {
private:
    unordered_map<string, unordered_map<string, DNSRecord>> caches;

    void saveToFile();


public:
    DNSCache();

    uint32_t getTotalCount();

    static DNSCache INSTANCE;

    void loadFromFile();

    void addCache(const string &domain, const unordered_set<uint32_t> &ips, const string &dnsServer, const int expire,
                  const bool matchArea);

    void query(const string &host, DNSRecord &recode);

    unordered_set<string> queryNotMatchAreaServers(const string &host);
};


#endif//ST_DNS_DNS_CACHE_H
