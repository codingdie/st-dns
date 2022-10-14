//
// Created by System Administrator on 2020/10/8.
//

#include "area_ip.h"
#include "file.h"
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <queue>
using namespace boost::property_tree;


const int NET_QPS = 1;
namespace st {
    namespace areaip {

        using namespace st::utils;
        manager &manager::uniq() {
            static manager instance;
            return instance;
        }
        manager::manager() {
            ctx = new boost::asio::io_context();
            stat_timer = new boost::asio::deadline_timer(*ctx);
            ctx_work = new boost::asio::io_context::work(*ctx);
            th = new thread([=]() { ctx->run(); });
            vector<area_ip_range> ips;
            ips.emplace_back(area_ip_range::parse("192.168.0.0/16", "LAN"));
            ips.emplace_back(area_ip_range::parse("10.0.0.0/8", "LAN"));
            ips.emplace_back(area_ip_range::parse("172.16.0.0/16", "LAN"));
            ips.emplace_back(area_ip_range::parse("0.0.0.0/8", "LAN"));
            default_caches.emplace("LAN", ips);
            sync_net_area_ip();
        }
        manager::~manager() {
            stat_timer->cancel();
            delete ctx_work;
            ctx->stop();
            th->join();
            delete ctx;
            delete th;
            delete stat_timer;
        }

        string manager::get_area_code(const string &areaReg) {
            string areaCode = areaReg;
            if (areaReg[0] == '!') {
                areaCode = areaReg.substr(1, areaReg.size() - 1);
            }
            transform(areaCode.begin(), areaCode.end(), areaCode.begin(), ::toupper);
            return areaCode;
        }
        string manager::download_area_ips(const string &areaCode) {
            string areaCodeLow = areaCode;
            transform(areaCodeLow.begin(), areaCodeLow.end(), areaCodeLow.begin(), ::tolower);
            string filePath = "/etc/area-ips/" + areaCode;
            if (!file::exit(filePath)) {
                file::create_if_not_exits(filePath);
                string url = "https://raw.githubusercontent.com/herrbischoff/country-ip-blocks/master/ipv4/" +
                             areaCodeLow + ".cidr";
                if (shell::exec("wget -q " + url + " -O " + filePath)) {
                    return filePath;
                } else {
                    logger::ERROR << areaCode << "ips file downloadn failed!" << url << END;
                    file::del(filePath);
                    return "";
                }
            }
            return filePath;
        }

        bool manager::load_area_ips(const string &areaReg) {
            string areaCode = get_area_code(areaReg);
            if (default_caches.find(areaCode) != default_caches.end()) {
                return true;
            }
            std::lock_guard<std::mutex> lg(default_lock);
            if (default_caches.find(areaCode) == default_caches.end()) {
                string dataPath = download_area_ips(areaCode);
                if (dataPath.empty()) {
                    return false;
                }
                ifstream in(dataPath);
                string line;
                vector<area_ip_range> &ips = default_caches[areaCode];
                if (in) {
                    while (getline(in, line)) {
                        if (!line.empty()) {
                            ips.emplace_back(area_ip_range::parse(line, areaCode));
                        }
                    }
                    logger::INFO << "load area" << areaCode << "ips" << ips.size() << "from" << dataPath << END;
                }
            }
            return true;
        }


        bool manager::is_area_ip(const std::vector<string> &areas, const uint32_t &ip) {
            if (areas.empty()) {
                return true;
            }
            for (const auto &areaReg : areas) {
                string areaCode = get_area_code(areaReg);
                if (!load_area_ips(areaCode)) {
                    return false;
                }
            }
            auto ipArea = get_area(ip);
            if (ipArea == "default") {
                return false;
            }
            for (auto areaReg : areas) {
                bool matchNot = areaReg[0] == '!';
                string areaCode = get_area_code(areaReg);
                if (!matchNot && ipArea == areaCode) {
                    return true;
                }
                if (matchNot && ipArea != areaCode) {
                    return true;
                }
            }
            return false;
        }
        bool manager::is_area_ip(const string &areaReg, const uint32_t &ip) {
            vector<string> areas({areaReg});
            return is_area_ip(areas, ip);
        }

        bool manager::is_area_ip(const string &areaReg, const string &ip) {
            return is_area_ip(areaReg, st::utils::ipv4::str_to_ip(ip));
        }
        uint16_t area_to_code(const string &areaCode) {
            if (areaCode.length() == 2 && areaCode[0] >= 'A' && areaCode[1] >= 'A' && areaCode[1] <= 'Z' &&
                areaCode[0] <= 'Z') {
                uint8_t firstCode = areaCode[0] - 'A';
                uint8_t secondCode = areaCode[1] - 'A';
                return firstCode * 26 + secondCode;
            }
            return 0;
        }
        string code_to_area(uint16_t mark) {
            if (mark >= 0 && mark <= 26 * 26 - 1) {
                string area = "##";
                area[0] = (char) ('A' + mark / 26);
                area[1] = (char) ('A' + mark % 26);
                return area;
            }
            return "";
        }
        void manager::async_load_ip_info(const uint32_t &ip) {
            std::function<void(const uint32_t &ip)> doLoadIPInfo = [this](const uint32_t &ip) {
                uint64_t begin = st::utils::time::now();
                if (get_area(ip, net_caches).empty()) {
                    auto ipInfos = load_ip_info(ip);
                    apm_logger::perf("load-net-ipinfo", {{"success", to_string(!ipInfos.first.empty())}},
                                     st::utils::time::now() - begin);
                    if (!ipInfos.first.empty()) {
                        auto area = ipInfos.first;
                        ofstream fs(IP_NET_AREA_FILE, std::ios_base::out | std::ios_base::app);
                        if (fs.is_open()) {
                            std::lock_guard<std::mutex> lg(net_lock);
                            this->net_caches[ip] = area;
                            fs << st::utils::ipv4::ip_to_str(ip) << "\t" << area << "\n";
                            logger::INFO << "async load ip info" << st::utils::ipv4::ip_to_str(ip) << area << END;
                            fs.flush();
                        }
                    } else {
                        logger::ERROR << "async load ip info failed!" << st::utils::ipv4::ip_to_str(ip) << END;
                    }
                    uint64_t cost = st::utils::time::now() - begin;
                    uint64_t timeout = 1000L / NET_QPS;
                    if (cost < timeout) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(timeout - cost));
                    }
                } else {
                    logger::INFO << "async load ip info skipped!" << st::utils::ipv4::ip_to_str(ip) << END;
                }
            };
            ctx->post(boost::bind(doLoadIPInfo, ip));
        }

        bool manager::is_area_ip(const string &areaCode, const uint32_t &ip,
                                 const unordered_map<string, vector<area_ip_range>> &caches) {
            //todo find fast
            auto iterator = caches.find(areaCode);
            if (iterator != caches.end()) {
                for (auto &it : iterator->second) {
                    if (ip <= it.end && ip >= it.start) {
                        return true;
                    }
                }
            }
            return false;
        }

        string manager::get_area(const uint32_t &ip, const unordered_map<string, vector<area_ip_range>> &caches) {
            if (ip != 0) {
                for (auto it = caches.begin(); it != caches.end(); it++) {
                    string area = (*it).first;
                    if (is_area_ip(area, ip, caches)) {
                        return area;
                    }
                }
            }
            return "";
        }

        string manager::get_area(const uint32_t &ip, const unordered_map<uint32_t, string> &caches) {
            if (caches.find(ip) != caches.end()) {
                return caches.at(ip);
            }
            return "";
        }

        string manager::get_area(const uint32_t &ip) {
            string area;
            if (ip != 0) {
                area = get_area(ip, net_caches);
                if (area.empty()) {
                    area = get_area(ip, default_caches);
                    async_load_ip_info(ip);
                }
            }
            return area.empty() ? "default" : area;
        }

        pair<string, vector<area_ip_range>> manager::load_ip_info(const uint32_t &ip) {
            vector<area_ip_range> ip_ranges;
            string result;
            string area;
            string command =
                    "curl -s -m 10 --location --request GET \"https://ipinfo.io/widget/" + ipv4::ip_to_str(ip) +
                    "?token=" + to_string(time::now()) +
                    R"(" --header "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/100.0.4896.75 Safari/537.3" --header "Referer: https://ipinfo.io/")";
            if (shell::exec(command, result)) {
                ptree tree;
                try {
                    std::stringstream ss(result);
                    read_json(ss, tree);
                    area = tree.get("country", "");
                    auto abuse = tree.get_child_optional("abuse");
                    if (area.empty() && abuse.is_initialized()) {
                        area = abuse.get().get("country", "");
                    }
                    if (!area.empty() && area.size() == 2) {
                        auto asn = tree.get_child_optional("asn");
                        if (asn.is_initialized()) {
                            auto route = asn.get().get("route", "");
                            if (!route.empty()) {
                                ip_ranges.emplace_back(area_ip_range::parse(route, area));
                            }
                        } else {
                            if (abuse.is_initialized()) {
                                auto route = abuse.get().get("network", "");
                                if (!route.empty()) {
                                    ip_ranges.emplace_back(area_ip_range::parse(route, area));
                                }
                            }
                        }
                    }
                } catch (exception &e) {
                }
            } else {
                logger::ERROR << "load_ip_info curl failed!" << command << result << END;
            }
            if (ip_ranges.empty()) {
                logger::ERROR << "load_ip_info failed!" << result << END;
            }
            return make_pair(area, ip_ranges);
        }
        void manager::sync_net_area_ip() {
            stat_timer->expires_from_now(boost::posix_time::seconds(30));
            stat_timer->async_wait([=](boost::system::error_code ec) {
                if (!ec) {
                    this->sync_net_area_ip();
                }
            });
            lock_guard<mutex> lockGuard(net_lock);
            unordered_set<string> finalRecord;
            ifstream in(IP_NET_AREA_FILE);
            if (!in) {
                return;
            }
            string line;
            while (getline(in, line)) {
                if (!line.empty()) {
                    finalRecord.emplace(line);
                }
            }
            in.close();
            for (auto &net_cache : net_caches) {
                auto ip = net_cache.first;
                auto area = net_cache.second;
                finalRecord.emplace(st::utils::ipv4::ip_to_str(ip) + "\t" + area);
            }
            ofstream fileStream(IP_NET_AREA_FILE);
            if (fileStream.is_open()) {
                unordered_map<uint32_t, string> newCaches;

                for (const auto &it : finalRecord) {
                    auto splits = st::utils::strutils::split(it, "\t");
                    if (splits.size() == 2) {
                        fileStream << it << "\n";
                        newCaches[st::utils::ipv4::str_to_ip(splits[0])] = splits[1];
                    }
                }
                fileStream.flush();
                fileStream.close();
                logger::INFO << "sync net area ips success! before:" << this->net_caches.size()
                             << "after:" << newCaches.size() << END;
                this->net_caches = newCaches;
            }
        }


    }// namespace areaip
}// namespace st