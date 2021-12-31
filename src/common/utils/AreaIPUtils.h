//
// Created by System Administrator on 2020/10/8.
//

#ifndef ST_AREAIP_UTILS_H
#define ST_AREAIP_UTILS_H

#include <iostream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "ShellUtils.h"
using namespace std;

namespace st {
    namespace areaip {
        bool loadAreaIPs(const string &areaCode);
        std::pair<uint32_t, uint32_t> ipRange(const string &rangeStr);
        bool isAreaIP(const string &areaCode, const uint32_t &ip);
        bool isAreaIP(const string &areaCode, const string &ip);

    }// namespace areaip
}// namespace st


#endif//ST_AREAIP_UTILS_H
