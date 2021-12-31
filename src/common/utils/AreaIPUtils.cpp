//
// Created by System Administrator on 2020/10/8.
//

#include "AreaIPUtils.h"
#include "Logger.h"
#include "IPUtils.h"
#include <iostream>
#include "FileUtils.h"
namespace st {
    namespace areaip {
        using namespace st::utils;
        static unordered_map<string, vector<pair<uint32_t, uint32_t>> *> caches;
        static mutex rLock;
        string getAreaCode(const string &areaReg) {
            string areaCode = areaReg;
            if (areaReg[0] == '!') {
                areaCode = areaReg.substr(1, areaReg.size() - 1);
            }
            transform(areaCode.begin(), areaCode.end(), areaCode.begin(), ::toupper);
            return areaCode;
        }
        string downloadAreaIPs(const string &areaCode) {
            string areaCodeLow = areaCode;
            transform(areaCodeLow.begin(), areaCodeLow.end(), areaCodeLow.begin(), ::tolower);
            //https://raw.fastgit.org/herrbischoff/country-ip-blocks/master/ipv4/ae.cidr
            string filePath = "/tmp/" + areaCode + ".ips";
            if (!file::exit(filePath)) {
                string url = "https://raw.githubusercontent.com/herrbischoff/country-ip-blocks/master/ipv4/" + areaCodeLow + ".cidr";
                if (shell::exec("wget -q " + url + " -O " + filePath)) {
                    return filePath;
                } else {
                    Logger::ERROR << areaCode << "ips file downloadn failed!" << url << END;
                    file::del(filePath);
                    return "";
                }
            }
            return filePath;
        }

        std::pair<uint32_t, uint32_t> ipRange(const string &rangeStr) {
            uint64_t index = rangeStr.find_first_of('/');
            uint32_t beginIp = st::utils::ipv4::strToIp(rangeStr.substr(0, index));
            uint32_t ipNum = 1 << (32 - atoi(rangeStr.substr(index + 1, rangeStr.length()).c_str()));
            uint32_t endIp = beginIp + ipNum - 1;
            return make_pair(beginIp, endIp);
        }
        bool loadAreaIPs(const string &areaReg) {
            string areaCode = getAreaCode(areaReg);
            if (caches.find(areaCode) != caches.end()) {
                return true;
            }
            std::lock_guard<std::mutex> lg(rLock);
            if (caches.find(areaCode) == caches.end()) {
                string dataPath = downloadAreaIPs(areaCode);
                if (dataPath.compare("") == 0) {
                    return false;
                }
                ifstream in(dataPath);
                string line;
                vector<pair<uint32_t, uint32_t>> *ips = new vector<pair<uint32_t, uint32_t>>();
                caches.emplace(areaCode, ips);
                if (in) {
                    while (getline(in, line)) {
                        if (!line.empty()) {
                            ips->emplace_back(ipRange(line));
                        }
                    }
                    Logger::INFO << "load area" << areaCode << "ips" << ips->size() << "from" << dataPath << END;
                }
            }
            return true;
        }

        bool isAreaIP(const string &areaReg, const uint32_t &ip) {
            string areaCode = getAreaCode(areaReg);
            bool matchNot = areaReg[0] == '!';
            if (!loadAreaIPs(areaCode)) {
                return false;
            }
            //todo find fast
            auto iterator = caches.find(areaCode);
            if (iterator != caches.end()) {
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
        bool isAreaIP(const string &areaCode, const string &ip) {
            return isAreaIP(areaCode, st::utils::ipv4::strToIp(ip));
        }

    }// namespace areaip
}// namespace st