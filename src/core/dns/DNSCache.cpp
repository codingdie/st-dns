//
// Created by codingdie on 2020/5/24.
//

#include "DNSCache.h"

DNSCache DNSCache::INSTANCE;

static mutex rLock;

void DNSCache::addCache(UdpDNSResponse *response) {
    lock_guard<mutex> lockGuard(rLock);
    if (response->isValid()) {
        string domain = response->queryZone->querys.front()->domain->domain;
        auto iterator = INSTANCE.caches.find(domain);
        set<uint32_t> *ips = nullptr;
        if (iterator == INSTANCE.caches.end()) {
            ips = new set<uint32_t>();
            INSTANCE.caches.insert(make_pair(domain, ips));
        } else {
            ips = iterator->second;
        }
        for (auto answer :response->ips) {
            ips->emplace(answer);
        }
    }
}


set<uint32_t> DNSCache::query(const string &host) {
    std::set<uint32_t> result;
    auto iterator = INSTANCE.caches.find(host);
    if (iterator != INSTANCE.caches.end()) {
        set<uint32_t> *vector = iterator->second;
        for (auto it = vector->begin(); it != vector->end(); it++) {
            uint32_t ip = *it;
            result.emplace(ip);
        }
    }
    return result;
}
