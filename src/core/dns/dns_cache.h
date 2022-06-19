//
// Created by codingdie on 2020/5/24.
//

#ifndef ST_DNS_DNS_CACHE_H
#define ST_DNS_DNS_CACHE_H

#include "utils/utils.h"

#include <iostream>
#include <mutex>

#include <iostream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;

class dns_record {

public:
    vector<uint32_t> ips;
    uint64_t expire_time = 0;
    string server;
    string domain = "";
    bool expire = false;
    bool match_area = false;

    string serialize();
    bool deserialize(const string &str);
};

class dns_cache {
private:
    long trusted_domain_count = 0;
    unordered_map<string, unordered_map<string, dns_record>> caches;

    void saveToFile();


public:
    dns_cache();

    uint32_t get_trusted_domain_count();

    static dns_cache INSTANCE;

    void load_from_file();

    void add(const string &domain, const vector<uint32_t> &ips, const string &dnsServer, const int expire,
                  const bool match_area);

    void query(const string &domain, dns_record &record);

    bool has_any_record(const string &domain);

    bool has_trusted_record(const string &domain);

    uint32_t server_count(const string &domain);

    unordered_set<string> query_not_match_area_servers(const string &domain);
};


#endif//ST_DNS_DNS_CACHE_H
