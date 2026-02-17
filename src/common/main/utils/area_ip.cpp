//
// Created by System Administrator on 2020/10/8.
//

#include "area_ip.h"
#include "file.h"
#include "http.h"
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <queue>
using namespace boost::property_tree;


const int NET_QPS = 3;
namespace st {
    namespace areaip {

        using namespace st::utils;
        manager &manager::uniq() {
            static manager instance;
            return instance;
        }
        manager::manager()
            : random_engine(time::now()), last_load_ip_info_time(time::now()), last_load_area_ips_time(time::now()),
              ctx(), sche_ctx() {
            ctx_work = new boost::asio::io_context::work(ctx);
            sche_ctx_work = new boost::asio::io_context::work(sche_ctx);
            th = new thread([this]() { this->ctx.run(); });
            sche_th = new thread([this]() { this->sche_ctx.run(); });
            sync_timer = new boost::asio::deadline_timer(sche_ctx);
            vector<area_ip_range> ip_ranges;
            ip_ranges.emplace_back(area_ip_range::parse("192.168.0.0/16", "LAN"));
            ip_ranges.emplace_back(area_ip_range::parse("10.0.0.0/8", "LAN"));
            ip_ranges.emplace_back(area_ip_range::parse("172.16.0.0/16", "LAN"));
            ip_ranges.emplace_back(area_ip_range::parse("0.0.0.0/8", "LAN"));
            default_caches.emplace("LAN", ip_ranges);
            file::create_if_not_exits(IP_NET_AREA_FILE);
            sync_net_area_ip();
        }
        manager::~manager() {
            // 1. 取消定时器
            sync_timer->cancel();

            // 2. 删除 work 对象
            delete sche_ctx_work;
            delete ctx_work;

            // 3. 等待 50ms，让异步回调完成
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // 4. 停止 io_context
            sche_ctx.stop();
            ctx.stop();

            // 5. 等待线程退出
            if (th && th->joinable()) {
                th->join();
            }
            if (sche_th && sche_th->joinable()) {
                sche_th->join();
            }
            delete th;
            delete sync_timer;
            delete sche_th;
        }

        string manager::get_area_code(const string &areaReg) {
            string areaCode = areaReg;
            if (areaReg[0] == '!') {
                areaCode = areaReg.substr(1, areaReg.size() - 1);
            }
            transform(areaCode.begin(), areaCode.end(), areaCode.begin(), ::toupper);
            return areaCode;
        }
        string manager::download_area_ips(const string &area_code) {
            string areaCodeLow = area_code;
            transform(areaCodeLow.begin(), areaCodeLow.end(), areaCodeLow.begin(), ::tolower);
            string filePath = "/etc/area-ips/" + area_code;
            if (!file::exists(filePath)) {
                auto result =
                        utils::http::get("https://raw.githubusercontent.com",
                                         "/herrbischoff/country-ip-blocks/master/ipv4/" + areaCodeLow + ".cidr", {});
                if (result.first == 200 && !result.second.empty()) {
                    file::create_if_not_exits(filePath);
                    ofstream fs(filePath);
                    fs << result.second;
                    fs.flush();
                    fs.close();
                } else {
                    logger::ERROR << area_code << "ips file download failed!" << area_code << END;
                    return "";
                }
            }
            return filePath;
        }

        bool manager::load_area_ips(const string &area_code) {
            string areaCode = get_area_code(area_code);
            std::lock_guard<std::mutex> lg(default_lock);
            if (has_load_area_ips(areaCode)) {
                return true;
            } else {
                string path = download_area_ips(areaCode);
                if (path.empty()) {
                    return false;
                }
                ifstream in(path);
                string line;
                vector<area_ip_range> &ip_ranges = default_caches[areaCode];
                ip_ranges.clear();
                if (in) {
                    while (getline(in, line)) {
                        if (!line.empty()) {
                            ip_ranges.emplace_back(area_ip_range::parse(line, areaCode));
                        }
                    }
                    logger::INFO << "load area" << areaCode << "ips" << ip_ranges.size() << "from" << path << END;
                }
                if (ip_ranges.empty()) {
                    default_caches.erase(areaCode);
                    file::del(path);
                }
            }
            return true;
        }
        bool manager::has_load_area_ips(const string &areaCode) {
            return default_caches.find(areaCode) != default_caches.end();
        }

        void manager::async_load_area_ips(const string &area_code) {
            if (!has_load_area_ips(area_code) && time::now() - last_load_area_ips_time.load() > 1000) {
                last_load_area_ips_time = time::now();
                ctx.post([this, area_code]() { this->load_area_ips(area_code); });
            }
        }

        bool manager::is_area_ip(const std::vector<string> &areas, const uint32_t &ip) {
            if (areas.empty()) {
                return true;
            }
            for (const auto &area_reg : areas) {
                string area_code = get_area_code(area_reg);
                async_load_area_ips(area_code);
            }
            auto ip_area = get_area(ip, true);
            if (ip_area == "default") {
                return false;
            }
            return is_match_areas(areas, ip_area);
        }
        bool manager::is_match_areas(const vector<string> &areas, const string &area) {
            for (auto areaReg : areas) {
                bool matchNot = areaReg[0] == '!';
                string area_code = get_area_code(areaReg);
                if (!matchNot && area == area_code) {
                    return true;
                }
                if (matchNot && area != area_code) {
                    return true;
                }
            }
            return false;
        }
        bool manager::is_area_ip(const string &area_reg, const uint32_t &ip) {
            vector<string> areas({area_reg});
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
        void manager::async_load_ip_info_from_net(const uint32_t &ip) {
            ctx.post([this, ip]() {
                if (ips.emplace(ip).second) {
                    std::function<void(const uint32_t &ip)> do_load_ip_info = [this](const uint32_t &ip) {
                        string net_area;
                        {
                            std::lock_guard<std::mutex> lg(net_lock);
                            net_area = get_area(ip, net_caches);
                        }
                        if (net_area.empty()) {
                            auto area_code = load_ip_info(ip);
                            if (!area_code.empty()) {
                                {
                                    std::lock_guard<std::mutex> lg(net_lock);
                                    this->net_caches[ip] = area_code;
                                }
                                async_load_area_ips(area_code);
                            } else {
                                logger::ERROR << "async load ip info failed!" << st::utils::ipv4::ip_to_str(ip) << END;
                            }

                        } else {
                            logger::INFO << "async load ip info skipped!" << st::utils::ipv4::ip_to_str(ip) << END;
                        }
                        ips.erase(ip);
                    };
                    uint64_t now_time = time::now();
                    if (last_load_ip_info_time.load() <= now_time) {
                        last_load_ip_info_time = now_time + 1000L / NET_QPS;
                    } else {
                        last_load_ip_info_time.fetch_add(1000L / NET_QPS);
                    }
                    if (last_load_ip_info_time.load() > now_time) {
                        auto *delay = new boost::asio::deadline_timer(ctx);
                        delay->expires_from_now(
                                boost::posix_time::milliseconds(last_load_ip_info_time.load() - now_time));
                        delay->async_wait([=](boost::system::error_code ec) {
                            ctx.post(boost::bind(do_load_ip_info, ip));
                            delete delay;
                        });
                    } else {
                        ctx.post(boost::bind(do_load_ip_info, ip));
                    }
                }
            });
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
            if (ip > 0) {
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
        string manager::get_area(const uint32_t &ip, bool async_load_net) {
            string area;
            if (ip != 0) {
                {
                    std::lock_guard<std::mutex> lg(net_lock);
                    area = get_area(ip, net_caches);
                }
                if (area.empty()) {
                    {
                        std::lock_guard<std::mutex> lg(default_lock);
                        area = get_area(ip, default_caches);
                    }
                    if (area != "LAN" && async_load_net) {
                        async_load_ip_info_from_net(ip);
                    }
                }
            }
            return area.empty() ? "default" : area;
        }

        string manager::get_area(const uint32_t &ip) { return get_area(ip, true); }
        string manager::load_ip_info(const uint32_t &ip, const area_ip_net_interface &interface) {
            auto begin = time::now();
            auto splits = utils::strutils::split(interface.url, "/", interface.url.find_first_of("://") + 3, 1);
            auto host = splits[0];
            string area;
            auto uri = "/" + splits[1].replace(splits[1].find("{IP}"), 4, ipv4::ip_to_str(ip));
            auto result = st::utils::http::get(
                    splits[0], uri,
                    {{"User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like "
                                    "Gecko) Chrome/100.0.4896.75 Safari/537.3"},
                     {"Referer", host}});
            if (result.first == 200 && !result.second.empty()) {
                ptree tree;
                try {
                    stringstream ss(result.second);
                    read_json(ss, tree);
                    auto paths = strutils::split(interface.area_json_path, ".");
                    for (int i = 0; i < paths.size(); i++) {
                        if (i != paths.size() - 1) {
                            auto node = tree.get_child_optional(paths[i]);
                            if (node.is_initialized()) {
                                tree = node.get();
                            }
                        } else {
                            area = tree.get(paths[i], "");
                        }
                    }
                } catch (exception &e) {
                }
            }
            transform(area.begin(), area.end(), area.begin(), ::toupper);
            apm_logger::perf("load-net-ip-info", {{"success", to_string(!area.empty())}, {"url", interface.url}},
                             st::utils::time::now() - begin);
            if (!area.empty()) {
                logger::INFO << "load_ip_info from" << host + uri << " success!" << area << END;
            } else {
                logger::ERROR << "load_ip_info from" << host + uri << " failed!" << result.first << result.second
                              << END;
            }
            return area;
        }

        string manager::load_ip_info(const uint32_t &ip) {
            auto interfaces = this->conf.interfaces;
            std::shuffle(interfaces.begin(), interfaces.end(), random_engine);
            for (auto &interface : interfaces) {
                string area = load_ip_info(ip, interface);
                if (!area.empty()) {
                    return area;
                }
            }
            return "";
        }

        void manager::sync_net_area_ip() {
            unordered_set<string> final_record;
            ifstream in(IP_NET_AREA_FILE);
            if (in) {
                string line;
                while (getline(in, line)) {
                    if (!line.empty()) {
                        final_record.emplace(line);
                    }
                }
                in.close();
                auto ori_size = final_record.size();
                unordered_map<uint32_t, string> net_caches_duplicate;
                {
                    lock_guard<mutex> lockGuard(net_lock);
                    net_caches_duplicate = net_caches;
                }
                for (auto &net_cache : net_caches_duplicate) {
                    auto ip = net_cache.first;
                    auto area = net_cache.second;
                    final_record.emplace(st::utils::ipv4::ip_to_str(ip) + "\t" + area);
                }
                auto merged_size = final_record.size();
                if (ori_size != merged_size) {
                    string tmp =
                            IP_NET_AREA_FILE + "." + to_string(time::now()) + "." + to_string(random_engine()) + ".tmp";
                    ofstream fs(tmp);
                    auto tmp_crate_time = time::now() / 1000;
                    unordered_map<uint32_t, string> new_caches;
                    for (const auto &it : final_record) {
                        auto splits = st::utils::strutils::split(it, "\t");
                        uint32_t uip = st::utils::ipv4::str_to_ip(splits[0]);
                        auto area = splits[1];
                        if (splits.size() == 2 && uip != 9 && !area.empty()) {
                            fs << it << "\n";
                            new_caches[uip] = area;
                        } else {
                            logger::WARN << "invalid net ip record" << it << END;
                        }
                    }
                    fs.flush();
                    fs.close();
                    {
                        lock_guard<mutex> lockGuard(net_lock);
                        this->net_caches = new_caches;
                    }
                    auto last_write_time = boost::filesystem::last_write_time(IP_NET_AREA_FILE);
                    if (last_write_time < tmp_crate_time) {
                        boost::filesystem::rename(tmp, IP_NET_AREA_FILE);
                        logger::INFO << "sync net area ips success! before:" << ori_size << "after:" << merged_size
                                     << "valid:" << new_caches.size() << END;
                    } else {
                        logger::DEBUG << "sync net area ips skip! update conflict" << last_write_time << tmp_crate_time
                                      << END;
                        boost::filesystem::remove(tmp);
                    }
                } else {
                    logger::DEBUG << "sync net area ips skip! record not change" << END;
                }
            } else {
                file::create_if_not_exits(IP_NET_AREA_FILE);
                logger::ERROR << "sync net area ips skip! file not exits" << END;
            }
            sync_timer->expires_from_now(boost::posix_time::seconds(10 + random_engine() % 5));
            sync_timer->async_wait([=](boost::system::error_code ec) {
                // 任何错误都直接返回，避免访问可能已析构的对象
                if (ec) {
                    return;
                }
                this->sync_net_area_ip();
            });
        }
        void manager::config(const area_ip_config &config) { this->conf = config; }
        void area_ip_config::load(const boost::property_tree::ptree &tree) {
            auto interfaces_node = tree.get_child("interfaces");
            if (!interfaces_node.empty()) {
                this->interfaces.clear();
                for (auto it = interfaces_node.begin(); it != interfaces_node.end(); it++) {
                    auto interface_node = it->second;
                    area_ip_net_interface in;
                    in.area_json_path = interface_node.get("area_json_path", "");
                    in.url = interface_node.get("url", "");
                    if (in.url.empty() || in.area_json_path.empty()) {
                        continue;
                    }
                    this->interfaces.emplace_back(in);
                }
            }
        }


        void area_ip_config::load(const string &json) {
            ptree tree;
            std::stringstream ss(json);
            try {
                read_json(ss, tree);
                this->load(tree);
            } catch (json_parser_error &e) {
                logger::ERROR << " parse json" << json << "error!" << e.message() << END;
            }
        }
        area_ip_config::area_ip_config() {
            this->load(R"({"interfaces":[
                {"url":"http://ip.taobao.com/outGetIpInfo?ip={IP}&accessKey=alibaba-inc","area_json_path":"data.country_id"},
                {"url":"http://ip-api.com/json/{IP}?lang=zh-CN","area_json_path":"countryCode"}
            ]})");
        }
    }// namespace areaip
}// namespace st