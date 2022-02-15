//
// Created by System Administrator on 2020/10/8.
//

#include "AreaIPUtils.h"
#include "FileUtils.h"
#include "IPUtils.h"
#include "Logger.h"
#include <iostream>
#include <boost/locale.hpp>
#include <boost/regex.hpp>
#include <boost/property_tree/json_parser.hpp>
using namespace boost::property_tree;
namespace st {
    namespace areaip {
        using namespace st::utils;
        static unordered_map<string, vector<pair<uint32_t, uint32_t>> *> caches;
        static mutex rLock;
        static unordered_map<string, string> CN_AREA_2_AREA({{"日本", "JP"}});

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
            string filePath = "/etc/area-ips/" + areaCode;
            if (!file::exit(filePath)) {
                file::createIfNotExits(filePath);
                string url = "https://raw.githubusercontent.com/herrbischoff/country-ip-blocks/master/ipv4/" +
                             areaCodeLow + ".cidr";
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
            if (areaCode.compare("LAN") == 0) {
                vector<pair<uint32_t, uint32_t>> *ips = new vector<pair<uint32_t, uint32_t>>();
                ips->emplace_back(ipRange("192.168.0.0/16"));
                ips->emplace_back(ipRange("10.0.0.0/8"));
                ips->emplace_back(ipRange("172.16.0.0/16"));
                ips->emplace_back(ipRange("0.0.0.0/8"));
                caches.emplace(areaCode, ips);
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
        bool isAreaIP(const std::vector<string> &areas, const uint32_t &ip) {
            if (areas.empty()) {
                return true;
            }
            bool result = false;
            for (auto it = areas.begin(); it != areas.end(); it++) {
                string area = *it;
                result |= isAreaIP(area, ip);
            }
            return result;
        }
        uint16_t area2Code(const string &areaCode) {
            if (areaCode.length() == 2 && areaCode[0] >= 'A' && areaCode[1] >= 'A' && areaCode[1] <= 'Z' &&
                areaCode[0] <= 'Z') {
                uint8_t firstCode = areaCode[0] - 'A';
                uint8_t secondCode = areaCode[1] - 'A';
                return firstCode * 26 + secondCode;
            }
            return 0;
        }
        string code2Area(uint16_t mark) {
            if (mark >= 0 && mark <= 26 * 26 - 1) {
                string area = "##";
                area[0] = (char) ('A' + mark / 26);
                area[1] = (char) ('A' + mark % 26);
                return area;
            }
            return "";
        }
        string getArea(const uint32_t &ip) {
            if (ip == 0) {
                return "default";
            }
            for (auto it = caches.begin(); it != caches.end(); it++) {
                string area = (*it).first;
                if (isAreaIP(area, ip)) {
                    return area;
                }
            }
            return "default";
        }
        string getAreaFromIP138(const uint32_t &ip) {
            string result;
            if (shell::exec("curl -H \"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/98.0.4758.80 Safari/537.36\" --location --request GET \"https://www.ip138.com/iplookup.asp?ip=" + st::utils::ipv4::ipToStr(ip) + "&action=2\"", result)) {
                result = boost::locale::conv::to_utf<char>(result.c_str(), std::string("gb2312"));
                boost::regex expression("{\".*ip_c_list.*\\}");
                boost::smatch what;
                if (boost::regex_search(result, what, expression, boost::regex_constants::match_not_dot_newline)) {
                    result = what[0].str();
                    ptree tree;
                    try {
                        std::stringstream results(result);
                        read_json(results, tree);
                        auto node = tree.get_child("ip_c_list");
                        if (!node.empty()) {
                            auto first = node.ordered_begin();
                            string areaCN = first->second.get("ct", "");
                            if(CN_AREA_2_AREA.find(areaCN)!=CN_AREA_2_AREA.end()){
                                return CN_AREA_2_AREA[areaCN];
                            }
                        }

                    } catch (json_parser_error &e) {
                        Logger::ERROR << " getAreaFromNet not valid json" << result << END;
                    }
                }
            }
            return "";
        }
        string getAreaFromNet(const uint32_t &ip) {
            return getAreaFromIP138(ip);
        }

    }// namespace areaip
}// namespace st