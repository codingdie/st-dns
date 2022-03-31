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
#include "StringUtils.h"
#include "IPUtils.h"

using namespace std;

namespace st {
    namespace areaip {
        class AreaIP {
        public:
            uint32_t start = 0;
            uint32_t end = 0;
            string area = "";

            string serialize() {
                return to_string(start) + "\t" + to_string(end) + "\t" + area;
            }

            static AreaIP parse(const string &str) {
                AreaIP result;
                auto strs = st::utils::strutils::split(str, "\t");
                result.start = stoul(strs[0]);
                result.end = stoul(strs[1]);
                result.area = strs[2];
                return result;
            }
            static AreaIP parse(const string &rangeStr, const string &area) {
                AreaIP result;
                uint64_t index = rangeStr.find_first_of('/');
                result.start = st::utils::ipv4::strToIp(rangeStr.substr(0, index));
                uint32_t ipNum = 1 << (32 - atoi(rangeStr.substr(index + 1, rangeStr.length()).c_str()));
                result.end = result.start + ipNum - 1;
                result.area = area;
                return result;
            }
        };
        uint16_t area2Code(const string &areaCode);
        string code2Area(uint16_t mark);
        class Manager {
        public:
            Manager();
            ~Manager();
            bool loadAreaIPs(const string &areaCode);
            bool isAreaIP(const string &areaReg, const uint32_t &ip);
            bool isAreaIP(const vector<string> &areas, const uint32_t &ip);
            bool isAreaIP(const string &areaReg, const string &ip);
            string getArea(const uint32_t &ip);
            bool loadIPInfo(const uint32_t &ip, AreaIP &ipInfo);
            static Manager &uniq();

        private:
            static Manager instance;
            static unordered_map<string, string> CN_AREA_2_AREA;
            static string CN_CCODE_JSON;
            unordered_map<string, vector<AreaIP>> defaultCaches;
            unordered_map<string, vector<AreaIP>> netCaches;
            mutex defaultLock;
            mutex netLock;
            bool isAreaIP(const string &areaCode, const uint32_t &ip, unordered_map<string, vector<AreaIP>> &caches);
            string getArea(const uint32_t &ip, unordered_map<string, vector<AreaIP>> &caches);
            string getAreaCode(const string &areaReg);
            string downloadAreaIPs(const string &areaCode);
            void asyncLoadIPInfo(const uint32_t &ip);
            void syncNetAreaIP();
            void initAreaCodeNameMap();

            std::pair<uint32_t, uint32_t> ipRange(const string &rangeStr);
            boost::asio::io_context *ioContext;
            boost::asio::io_context::work *ioContextWork = nullptr;
            std::thread *th = nullptr;
            boost::asio::deadline_timer *statTimer;
        };

    };// namespace areaip
}// namespace st


#endif//ST_AREAIP_UTILS_H
