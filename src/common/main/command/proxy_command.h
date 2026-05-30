//
// Created by codingdie on 10/20/22.
//

#ifndef ST_PROXY_COMMAND_H
#define ST_PROXY_COMMAND_H
#include "st.h"
#include <unordered_set>
namespace st {
    namespace command {
        namespace proxy {

            static uint16_t register_area_port(const string& ip, uint16_t port, const string &area) {
                auto begin = time::now();
                auto cm = "proxy register area virtual port --area=" + area + " --port=" + to_string(port) +
                          " --ip=" + ip;
                auto result = st::console::client::command("127.0.0.1", 5858, cm, 100);
                apm_logger::perf("register-area-port", {}, st::utils::time::now() - begin);
                if (result.first) {
                    return stoi(result.second);
                } else {
                    return 0;
                }
            }
            static vector<string> get_ip_available_proxy_areas(const string& ip) {
                auto begin = time::now();
                auto cm = "proxy ip available areas --ip=" + ip;
                auto result = st::console::client::command("127.0.0.1", 5858, cm, 1000);
                vector<string> areas;
                apm_logger::perf("get-ip-available-proxy-areas", {}, st::utils::time::now() - begin);
                if (result.first) {
                    areas = st::utils::strutils::split(result.second, ",");
                } else {
                    logger::ERROR << "get_ip_available_proxy_areas error! ip:" << ip << "msg" << result.second << END;
                }
                return areas;
            }

            static bool parse_ipv4_address(const string &ip_str, uint32_t &ip) {
                auto parts = st::utils::strutils::split(ip_str, ".");
                if (parts.size() != 4) {
                    return false;
                }
                uint32_t result = 0;
                for (const auto &part : parts) {
                    if (part.empty()) {
                        return false;
                    }
                    uint32_t value = 0;
                    for (char c : part) {
                        if (c < '0' || c > '9') {
                            return false;
                        }
                        value = value * 10 + static_cast<uint32_t>(c - '0');
                        if (value > 255) {
                            return false;
                        }
                    }
                    result = (result << 8U) | value;
                }
                ip = result;
                return true;
            }

            static unordered_set<uint32_t> parse_blacklist_ips(const string &response) {
                unordered_set<uint32_t> result;
                auto lines = st::utils::strutils::split(response, "\n");
                for (auto line : lines) {
                    st::utils::strutils::trim(line);
                    if (line.empty()) {
                        continue;
                    }
                    auto columns = st::utils::strutils::split(line, "\t", 0, 1);
                    if (columns.empty()) {
                        continue;
                    }
                    string ip_str = st::utils::strutils::trim(std::move(columns[0]));
                    uint32_t ip = 0;
                    if (!parse_ipv4_address(ip_str, ip)) {
                        logger::WARN << "skip invalid proxy blacklist ip" << ip_str << END;
                        continue;
                    }
                    result.emplace(ip);
                }
                return result;
            }

            static pair<bool, unordered_set<uint32_t>> get_blacklist_ips() {
                auto begin = time::now();
                auto result = st::console::client::command("127.0.0.1", 5858, "proxy blacklist", 1000);
                apm_logger::perf("get-proxy-blacklist", {}, st::utils::time::now() - begin);
                if (!result.first) {
                    logger::ERROR << "get proxy blacklist error!" << result.second << END;
                    return make_pair(false, unordered_set<uint32_t>{});
                }
                return make_pair(true, parse_blacklist_ips(result.second));
            }
        }// namespace proxy
    }// namespace command
}// namespace st
#endif//ST_PROXY_COMMAND_H
