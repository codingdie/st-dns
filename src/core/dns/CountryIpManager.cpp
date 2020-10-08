//
// Created by System Administrator on 2020/10/8.
//

#include "CountryIpManager.h"
#include "Config.h"
#include <iostream>

CountryIpManager CountryIpManager::INSTANCE;

static mutex rLock;

bool CountryIpManager::isCountryIP(const string &countryCode, const uint32_t &ip) {
    rLock.lock();
    if (INSTANCE.caches.find(countryCode) == INSTANCE.caches.end()) {
        string dataPath = st::dns::Config::BASE_CONF_PATH + "/country-ips/" + countryCode;
        ifstream in(dataPath);
        string line;
        vector<pair<uint32_t, uint32_t>> *ips = new vector<pair<uint32_t, uint32_t>>();
        INSTANCE.caches.emplace(countryCode, ips);
        if (in) {
            while (getline(in, line)) {
                if (!line.empty()) {
                    unsigned long index = line.find_first_of(' ');
                    auto beginIp = st::utils::ip::strToIp(line.substr(0, index));
                    auto endIp = st::utils::ip::strToIp(line.substr(index + 1, line.length()));
                    ips->emplace_back(make_pair(beginIp, endIp));
                }
            }
            Logger::INFO << "load country" << countryCode << "ipRanges" << ips->size() << "from" << dataPath << END;
        } else {
            Logger::ERROR << countryCode << "ipRanges file not exits" << dataPath << END;
        }

    }
    rLock.unlock();
    //todo find fast
    auto iterator = INSTANCE.caches.find(countryCode);
    if (iterator != INSTANCE.caches.end()) {
        auto ipRanges = iterator->second;
        for (auto it = ipRanges->begin(); it != ipRanges->end(); it++) {
            pair<uint32_t, uint32_t> &ipRange = *it;
            if (ip <= ipRange.second && ip >= ipRange.first) {
                return true;
            }
        }
    }
    return false;
}
