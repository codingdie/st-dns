//
// Created by System Administrator on 2020/10/8.
//

#ifndef ST_AREAIP_UTILS_H
#define ST_AREAIP_UTILS_H

#include "ipv4.h"
#include "logger.h"
#include "shell.h"
#include "string_utils.h"
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;

namespace st {
    namespace areaip {
        class area_ip_range {
        public:
            uint32_t start = 0;
            uint32_t end = 0;
            string area;

            string serialize() const { return to_string(start) + "\t" + to_string(end) + "\t" + area; }

            bool is_valid() const { return area.size() == 2 && end >= start && start != 0; }

            static area_ip_range parse(const string &str) {
                area_ip_range result;
                auto strs = st::utils::strutils::split(str, "\t");
                if (strs.size() == 3) {
                    try {
                        result.start = stoul(strs[0]);
                        result.end = stoul(strs[1]);
                        result.area = strs[2];
                    } catch (const std::exception &e) {
                        st::utils::logger::ERROR << "area_ip_range parse error!" << str << e.what() << END;
                    }
                }
                return result;
            }
            static area_ip_range parse(const string &rangeStr, const string &area) {
                area_ip_range result;
                uint64_t index = rangeStr.find_first_of('/');
                result.start = st::utils::ipv4::str_to_ip(rangeStr.substr(0, index));
                uint32_t ipNum = 1 << (32 - atoi(rangeStr.substr(index + 1, rangeStr.length()).c_str()));
                result.end = result.start + ipNum - 1;
                result.area = area;
                return result;
            }
        };
        uint16_t area_to_code(const string &areaCode);
        string code_to_area(uint16_t mark);
        class manager {
        public:
            manager();
            ~manager();
            bool load_area_ips(const string &areaCode);
            bool is_area_ip(const string &areaReg, const uint32_t &ip);
            bool is_area_ip(const vector<string> &areas, const uint32_t &ip);
            bool is_area_ip(const string &areaReg, const string &ip);
            string get_area(const uint32_t &ip);
            pair<string, vector<area_ip_range>> load_ip_info(const uint32_t &ip);
            static manager &uniq();

        private:
            const string IP_NET_AREA_FILE = "/etc/area-ips/IP_NET_AREA";
            unordered_map<string, vector<area_ip_range>> default_caches;
            unordered_map<uint32_t, string> net_caches;
            mutex default_lock;
            mutex net_lock;

            boost::asio::io_context *ctx;
            boost::asio::io_context::work *ctx_work = nullptr;
            std::thread *th = nullptr;
            boost::asio::deadline_timer *stat_timer;
            bool is_area_ip(const string &areaCode, const uint32_t &ip,
                            unordered_map<string, vector<area_ip_range>> &caches);
            string get_area(const uint32_t &ip, unordered_map<string, vector<area_ip_range>> &caches);
            string get_area(const uint32_t &ip, unordered_map<uint32_t, string> &caches);
            string get_area_code(const string &areaReg);
            string download_area_ips(const string &areaCode);
            void async_load_ip_info(const uint32_t &ip);
            void sync_net_area_ip();
        };
    }// namespace areaip
}// namespace st


#endif//ST_AREAIP_UTILS_H
