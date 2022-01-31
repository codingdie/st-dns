//
// Created by codingdie on 2020/5/24.
//

#ifndef ST_DNS_DNS_CACHE_H
#define ST_DNS_DNS_CACHE_H

#include "utils/STUtils.h"

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
    vector<uint32_t> ips;
    uint64_t expireTime = 0;
    string dnsServer;
    string domain = "";
    bool expire = false;
    bool matchArea = false;

    string serialize();
    boost::property_tree::ptree
    toPT();

    bool deserialize(const string &str);
};

class DNSCache {
private:
    long trustedDomainCount = 0;
    unordered_map<string, unordered_map<string, DNSRecord>> caches;

    void saveToFile();


public:
    DNSCache();

    uint32_t getTrustedDomainCount();

    static DNSCache INSTANCE;

    void loadFromFile();

    void addCache(const string &domain, const vector<uint32_t> &ips, const string &dnsServer, const int expire,
                  const bool matchArea);

    void query(const string &domain, DNSRecord &record);

    bool hasAnyRecord(const string &domain);
    bool hasTrustedRecord(const string &domain);
    uint32_t serverCount(const string &domain);

    unordered_set<string> queryNotMatchAreaServers(const string &domain);
};


#endif//ST_DNS_DNS_CACHE_H
