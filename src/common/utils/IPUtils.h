#ifndef DNS_H
#define DNS_H

#include "StringUtils.h"
#include <string>
#include <unordered_set>

using namespace std;
namespace st {
    namespace utils {
        namespace ipv4 {
            static string ipToStr(uint32_t ip) {
                string ipStr;
                ipStr += to_string((ip & 0xFF000000U) >> 24U);
                ipStr += ".";
                ipStr += to_string((ip & 0x00FF0000U) >> 16U);
                ipStr += ".";
                ipStr += to_string((ip & 0x0000FF00U) >> 8U);
                ipStr += ".";
                ipStr += to_string((ip & 0x000000FFU));
                return ipStr;
            }

            static uint32_t strToIp(const string &ipStr) {
                uint32_t ip = 0L;
                std::size_t lastPos = 0;
                std::size_t num = 0;
                while (num < 4 && lastPos < ipStr.length()) {
                    auto pos = ipStr.find_first_of('.', lastPos);
                    if ((num != 3 && pos == string::npos) || (num == 3 && pos != string::npos)) {
                        return 0;
                    }
                    auto numStr = ipStr.substr(lastPos, pos);
                    if(numStr.empty()){
                        return 0;
                    }
                    for (auto i = 0; i < numStr.length(); i++) {
                        if (numStr[0] < '0' || numStr[0] > '9') {
                            return 0;
                        }
                    }
                    uint32_t ipSeg = stoi(numStr);
                    if (ipSeg > 255) {
                        return 0;
                    }
                    lastPos = pos + 1;
                    num++;
                    ip = ip | (ipSeg << (32 - 8 * num));
                }
                return ip;
            }

            static unordered_set<uint32_t> strToIps(const string &ipStr) {
                unordered_set<uint32_t> ips;
                auto ipStrs = strutils::split(ipStr, ",");
                for (string &str : ipStrs) {
                    uint32_t ip = strToIp(str);
                    if (ip > 0) {
                        ips.emplace(ip);
                    }
                }
                return ips;
            }

            template<typename Collection>
            static string ipsToStr(const Collection &ips) {
                string ipStr;
                for (uint32_t ip : ips) {
                    ipStr += ipToStr(ip);
                    ipStr += ",";
                }
                return ipStr.substr(0, ipStr.length() - 1);
            }

        }// namespace ipv4
    }    // namespace utils
}// namespace st

#endif