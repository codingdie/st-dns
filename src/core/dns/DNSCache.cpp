//
// Created by codingdie on 2020/5/24.
//

#include "DNSCache.h"

DNSCache DNSCache::INSTANCE;

static mutex rLock;

void DNSCache::addCache(const string &domain, const set<uint32_t> &ips, const string &dnsServer) {
    lock_guard<mutex> lockGuard(rLock);
    Logger::INFO << "addDNSCache" << domain << ip::ipsToStr(ips) << "from" << dnsServer << END;
    auto iterator = INSTANCE.caches.find(domain);
    map<string, set<uint32_t> *> *serverCaches = nullptr;
    if (iterator == INSTANCE.caches.end()) {
        serverCaches = new map<string, set<uint32_t> *>();
        INSTANCE.caches.emplace(make_pair(domain, serverCaches));
    } else {
        serverCaches = iterator->second;
    }
    auto treeIterator = serverCaches->find(dnsServer);
    set<uint32_t> *ipCaches = nullptr;
    if (treeIterator == serverCaches->end()) {
        ipCaches = new set<uint32_t>();
        serverCaches->emplace(make_pair(dnsServer, ipCaches));
    } else {
        ipCaches = treeIterator->second;
    }
    for (uint32_t ip:ips) {
        ipCaches->emplace(ip);
    }

}


set<uint32_t> DNSCache::query(const string &host) {
    std::set<uint32_t> result;
    auto iterator = INSTANCE.caches.find(host);
    if (iterator != INSTANCE.caches.end()) {
        auto allServerMap = iterator->second;
        for (auto it = allServerMap->begin(); it != allServerMap->end(); it++) {
            auto pair = *it;
            auto ipSets = pair.second;
            for (auto ipIt = ipSets->begin(); ipIt != ipSets->end(); ipIt++) {
                auto ip = *ipIt;
                result.emplace(ip);

            }
        }
    }
    return result;
}
