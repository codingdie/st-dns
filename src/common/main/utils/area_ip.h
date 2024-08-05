//
// Created by System Administrator on 2020/10/8.
//

#ifndef ST_AREA_IP_UTILS_H
#define ST_AREA_IP_UTILS_H

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
#include <random>

using namespace std;

namespace st {
    namespace areaip {
        class area_ip_net_interface {
        public:
            std::string url;
            std::string area_json_path;
        };
        class area_ip_config {
        public:
            void load(const std::string &json);
            void load(const boost::property_tree::ptree &json);
            std::vector<area_ip_net_interface> interfaces;
            area_ip_config();
        };
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
            bool load_area_ips(const string &area_code);
            void async_load_area_ips(const string &area_code);
            bool is_area_ip(const string &areaReg, const uint32_t &ip);
            bool is_area_ip(const vector<string> &areas, const uint32_t &ip);
            bool is_area_ip(const string &areaReg, const string &ip);
            string get_area(const uint32_t &ip);
            string get_area(const uint32_t &ip, bool async_load_net);
            static manager &uniq();
            void config(const area_ip_config &config);

            void async_load_ip_info_from_net(const uint32_t &ip);

        private:
            const string IP_NET_AREA_FILE = "/etc/area-ips/IP_NET_AREA";
            area_ip_config conf;
            std::default_random_engine random_engine;
            unordered_map<string, vector<area_ip_range>> default_caches;
            unordered_map<uint32_t, string> net_caches;
            unordered_set<uint32_t> ips;
            mutex default_lock;
            mutex net_lock;
            std::atomic_uint64_t last_load_ip_info_time;
            std::atomic_uint64_t last_load_area_ips_time;
            boost::asio::io_context ctx;
            boost::asio::io_context::work *ctx_work = nullptr;
            std::thread *th = nullptr;

            boost::asio::io_context sche_ctx;
            boost::asio::io_context::work *sche_ctx_work = nullptr;
            std::thread *sche_th = nullptr;
            boost::asio::deadline_timer *sync_timer;
            bool is_area_ip(const string &areaCode, const uint32_t &ip,
                            const unordered_map<string, vector<area_ip_range>> &caches);
            string get_area(const uint32_t &ip, const unordered_map<string, vector<area_ip_range>> &caches);
            string get_area(const uint32_t &ip, const unordered_map<uint32_t, string> &caches);
            static string get_area_code(const string &areaReg);
            string download_area_ips(const string &area_code);
            void sync_net_area_ip();
            string load_ip_info(const uint32_t &ip);
            string load_ip_info(const uint32_t &ip, const area_ip_net_interface &interface);
            bool has_load_area_ips(const string &areaCode);
        };
    }// namespace areaip
}// namespace st


#endif//ST_AREA_IP_UTILS_H
