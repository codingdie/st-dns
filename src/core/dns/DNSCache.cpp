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
        vector<uint32_t> *ips = INSTANCE.caches.at(domain);
        if (ips == nullptr) {
            ips = new vector<uint32_t>();
            INSTANCE.caches.insert(make_pair(domain, ips));
        }
        for (auto answer :response->ips) {
            ips->emplace_back(answer);
        }
    }
}

int32_t DNSCache::query(string &host) {
    vector<uint32_t> *&vector = INSTANCE.caches.at(host);
    if (vector != nullptr && !vector->empty()) {
        srand(time(nullptr));
        return vector->at(random() % vector->size());
    } else {
        return 0;
    }

}
