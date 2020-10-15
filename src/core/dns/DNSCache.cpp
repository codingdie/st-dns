//
// Created by codingdie on 2020/5/24.
//

#include "DNSCache.h"
#include "Config.h"

DNSCache DNSCache::INSTANCE;

static mutex rLock;

void DNSCache::addCache(const string &domain, const set<uint32_t> &ips, const string &dnsServer) {
    lock_guard<mutex> lockGuard(rLock);
    Logger::INFO << "addDNSCache" << domain << ipv4::ipsToStr(ips) << "from" << dnsServer << END;
    auto iterator = INSTANCE.caches.find(domain);
    unordered_map<string, DNSCacheRecord *> *serverCaches = nullptr;
    if (iterator == INSTANCE.caches.end()) {
        serverCaches = new unordered_map<string, DNSCacheRecord *>();
        INSTANCE.caches.emplace(domain, serverCaches);
    } else {
        serverCaches = iterator->second;
    }
    DNSCacheRecord *record = nullptr;
    auto treeIterator = serverCaches->find(dnsServer);
    if (treeIterator == serverCaches->end()) {
        record = new DNSCacheRecord;
        serverCaches->emplace(make_pair(dnsServer, record));
    } else {
        record = treeIterator->second;
    }
    record->ips.clear();
    for (uint32_t ip:ips) {
        record->ips.emplace(ip);
    }
    record->ctime = time::now();

}


set<uint32_t> DNSCache::query(const string &host) {
    std::set<uint32_t> result;
    auto iterator = INSTANCE.caches.find(host);
    if (iterator != INSTANCE.caches.end()) {
        auto allServerMap = iterator->second;
        for (auto it = allServerMap->begin(); it != allServerMap->end(); it++) {
            auto pair = *it;
            auto record = pair.second;
            long expire = time::now() - record->ctime;
            if (expire < st::dns::Config::INSTANCE.dnsCacheExpire * 1000) {
                for (auto ipIt = record->ips.begin(); ipIt != record->ips.end(); ipIt++) {
                    auto ip = *ipIt;
                    result.emplace(ip);
                }
            }

        }
    }
    return move(result);
}
