//
// Created by System Administrator on 2020/10/8.
//

#include "AreaIpManager.h"
#include "Config.h"
#include <iostream>

AreaIpManager AreaIpManager::INSTANCE;

static mutex rLock;

bool AreaIpManager::isAreaIP(const string &areaReg, const uint32_t &ip) {
    string areaCode = areaReg;
    bool matchNot = false;
    if (areaReg[0] == '!') {
        areaCode = areaReg.substr(1, areaReg.size() - 1);
        matchNot = true;
    }
    rLock.lock();
    if (INSTANCE.caches.find(areaCode) == INSTANCE.caches.end()) {
        string dataPath = st::dns::Config::INSTANCE.baseConfDir + "/../area-ips/" + areaCode;
        if (!file::exit(dataPath)) {
            dataPath = st::dns::Config::INSTANCE.baseConfDir + "/../../area-ips/" + areaCode;
        }
        if (file::exit(dataPath)) {
            ifstream in(dataPath);
            string line;
            vector<pair<uint32_t, uint32_t>> *ips = new vector<pair<uint32_t, uint32_t>>();
            INSTANCE.caches.emplace(areaCode, ips);
            if (in) {
                while (getline(in, line)) {
                    if (!line.empty()) {
                        uint64_t index = line.find_first_of(' ');
                        auto beginIp = st::utils::ipv4::strToIp(line.substr(0, index));
                        auto endIp = st::utils::ipv4::strToIp(line.substr(index + 1, line.length()));
                        ips->emplace_back(make_pair(beginIp, endIp));
                    }
                }
                Logger::INFO << "load area" << areaCode << "ipRanges" << ips->size() << "from" << dataPath << END;
            } else {
                Logger::ERROR << areaCode << "ipRanges file not exits" << dataPath << END;
            }
        }
    }
    rLock.unlock();
    //todo find fast
    auto iterator = INSTANCE.caches.find(areaCode);
    if (iterator != INSTANCE.caches.end()) {
        auto ipRanges = iterator->second;
        for (auto it = ipRanges->begin(); it != ipRanges->end(); it++) {
            pair<uint32_t, uint32_t> &ipRange = *it;
            if (ip <= ipRange.second && ip >= ipRange.first) {
                return matchNot ? false : true;
            }
        }
    }
    return matchNot ? true : false;
}