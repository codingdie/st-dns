//
// Created by System Administrator on 2020/10/8.
//

#ifndef ST_DNS_AREAIPMANAGER_H
#define ST_DNS_AREAIPMANAGER_H

#include <iostream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;


class AreaIpManager {
private:
    static AreaIpManager INSTANCE;
    unordered_map<string, vector<pair<uint32_t, uint32_t>> *> caches;

public:
    static bool isAreaIP(const string &areaCode, const uint32_t &ip);
};


#endif//ST_DNS_AREAIPMANAGER_H
