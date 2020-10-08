#ifndef DNS_H
#define DNS_H

#include <string>

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
                int lastPos = 0;
                int num = 0;
                while (num < 4 && lastPos < ipStr.length()) {
                    auto pos = ipStr.find_first_of('.', lastPos);
                    uint32_t ipNum = stoi(ipStr.substr(lastPos, pos));
                    lastPos = pos + 1;
                    num++;
                    ip = ip | (ipNum << (32 - 8 * num));
                }
                return ip;
            }

            template<typename Collection> static string ipsToStr(Collection &ips) {
                string ipStr;
                for (uint32_t ip:ips) {
                    ipStr += ipToStr(ip);
                    ipStr += " ";
                }
                return ipStr;
            }
        }
    }
}

#endif