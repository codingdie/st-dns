//
// Created by System Administrator on 2020/10/8.
//

#ifndef ST_AREAIP_UTILS_H
#define ST_AREAIP_UTILS_H

#include "ShellUtils.h"
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
using namespace std;

namespace st {
    namespace areaip {
        bool loadAreaIPs(const string &areaCode);
        std::pair<uint32_t, uint32_t> ipRange(const string &rangeStr);
        bool isAreaIP(const string &areaReg, const uint32_t &ip);
        bool isAreaIP(const vector<string> &areas, const uint32_t &ip);
        bool isAreaIP(const string &areaReg, const string &ip);
        uint16_t area2Code(const string &areaCode);
        string code2Area(uint16_t mark);
        string getArea(const uint32_t &ip);

    }// namespace areaip
}// namespace st


#endif//ST_AREAIP_UTILS_H
