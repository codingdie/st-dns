#ifndef DNS_H
#define DNS_H

#include "string_utils.h"
#include <string>
#include <unordered_set>

using namespace std;
namespace st {
    namespace utils {
        namespace ipv4 {
            inline string ip_to_str(uint32_t ip) {
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

            inline uint32_t str_to_ip(const string &ipStr) {
                uint32_t ip = 0L;
                std::size_t lastPos = 0;
                std::size_t num = 0;
                while (num < 4 && lastPos < ipStr.length()) {
                    auto pos = ipStr.find_first_of('.', lastPos);
                    if ((num != 3 && pos == string::npos) || (num == 3 && pos != string::npos)) {
                        return 0;
                    }
                    auto numStr = ipStr.substr(lastPos, pos);
                    if (numStr.empty()) {
                        return 0;
                    }
                    for (string::size_type i = 0; i < numStr.length(); i++) {
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

            inline vector<uint32_t> str_to_ips(const string &ipStr) {
                vector<uint32_t> ips;
                auto ipStrs = strutils::split(ipStr, ",");
                for (string &str : ipStrs) {
                    uint32_t ip = str_to_ip(str);
                    if (ip > 0) {
                        ips.emplace_back(ip);
                    }
                }
                return ips;
            }

            template<typename Collection>
            inline string ips_to_str(const Collection &ips) {
                string ipStr;
                for (uint32_t ip : ips) {
                    ipStr += ip_to_str(ip);
                    ipStr += ",";
                }
                return ipStr.substr(0, ipStr.length() - 1);
            }

        }// namespace ipv4
    }    // namespace utils
}// namespace st

#endif