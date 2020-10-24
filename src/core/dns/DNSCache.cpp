//
// Created by codingdie on 2020/5/24.
//

#include "DNSCache.h"
#include "Config.h"

DNSCache DNSCache::INSTANCE;

static mutex rLock;

void DNSCache::addCache(const string &domain, const set<uint32_t> &ips, const string &dnsServer, const int expire) {
    lock_guard<mutex> lockGuard(rLock);
    Logger::INFO << "addDNSCache" << domain << ipv4::ipsToStr(ips) << "from" << dnsServer << END;
    auto iterator = INSTANCE.caches.find(domain);
    DNSRecord *record = nullptr;
    if (iterator == INSTANCE.caches.end()) {
        record = new DNSRecord();
        INSTANCE.caches.emplace(domain, record);
    } else {
        record = iterator->second;
    }
    record->ips.clear();
    for (uint32_t ip:ips) {
        record->ips.emplace(ip);
    }
    record->expireTime = time::now() + expire;
    record->dnsServer = dnsServer;
}

void DNSCache::addCache(const string &domain, const set<uint32_t> &ips, const string &dnsServer) {
    addCache(domain, ips, dnsServer, st::dns::Config::INSTANCE.dnsCacheExpire * 1000);
}


bool DNSCache::query(const string &host, DNSRecord &record) {
    auto iterator = INSTANCE.caches.find(host);
    if (iterator != INSTANCE.caches.end()) {
        record.dnsServer = iterator->second->dnsServer;
        record.expireTime = iterator->second->expireTime;
        record.ips = iterator->second->ips;
        if (record.expireTime > st::utils::time::now()) {
            return false;
        }
    }
    return true;
}
