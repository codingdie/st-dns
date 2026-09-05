//
// Created by System Administrator on 2020/10/8.
//
#include "config.h"
#include "utils/area_ip.h"

#include "command/proxy_command.h"
#include <regex>
#include <fstream>
#include <sstream>
st::dns::config st::dns::config::INSTANCE;

st::dns::config::config(const config &other) {
    runtime_owner = false;
    copy_from(other);
}

st::dns::config &st::dns::config::operator=(const config &other) {
    if (this == &other) {
        return *this;
    }
    bool current_runtime_owner = runtime_owner;
    unload();
    runtime_owner = current_runtime_owner;
    copy_from(other);
    return *this;
}

st::dns::config::~config() {
    unload();
}

void st::dns::config::unload() {
    if (!loaded) {
        return;
    }
    if (runtime_owner) {
        st::areaip::manager::uniq().stop();
        st::utils::apm_logger::disable();
        st::utils::logger::disable();
    }

    for (auto server : servers) {
        delete server;
    }
    servers.clear();

    for (auto rule : force_resolve_rules) {
        delete rule;
    }
    force_resolve_rules.clear();

    for (auto server : system_upstream_servers) {
        delete server;
    }
    system_upstream_servers.clear();

    ip = "127.0.0.1";
    port = 53;
    console_ip = "127.0.0.1";
    console_port = 5757;
    dns_cache_expire = 60 * 10;
    forward_max_running = 32;
    base_conf_dir = "/usr/local/etc/st/dns";
    loaded = false;
}

void st::dns::config::copy_from(const config &other) {
    ip = other.ip;
    port = other.port;
    console_ip = other.console_ip;
    console_port = other.console_port;
    dns_cache_expire = other.dns_cache_expire;
    forward_max_running = other.forward_max_running;
    base_conf_dir = other.base_conf_dir;
    area_ip_config = other.area_ip_config;
    auto_upstream_dns = other.auto_upstream_dns;
    resolv_conf_paths = other.resolv_conf_paths;

    for (auto server : other.servers) {
        servers.emplace_back(new remote_dns_server(*server));
    }
    for (auto rule : other.force_resolve_rules) {
        force_resolve_rules.emplace_back(new force_resolve_rule(*rule));
    }
    for (auto server : other.system_upstream_servers) {
        system_upstream_servers.emplace_back(new remote_dns_server(*server));
    }
    loaded = other.loaded;
}

void st::dns::config::load_system_dns() {
    if (!auto_upstream_dns) return;
    if (resolv_conf_paths.empty()) {
        logger::WARN << "auto_upstream_dns is enabled but resolv_conf_paths is empty" << END;
        return;
    }

    // 按优先级尝试每个候选路径，找到第一个存在且可读的文件
    string best_path;
    for (const auto &path : resolv_conf_paths) {
        ifstream test_file(path);
        if (test_file.is_open()) {
            best_path = path;
            break;
        }
    }
    if (best_path.empty()) {
        logger::WARN << "auto upstream DNS: none of the candidate resolv.conf paths found" << END;
        return;
    }

    ifstream resolv_file(best_path);
    string line;
    int count = 0;
    while (getline(resolv_file, line)) {
        // 去除前后空格
        size_t start = line.find_first_not_of(" \t");
        if (start == string::npos) continue;
        size_t end = line.find_last_not_of(" \t");
        string trimmed = line.substr(start, end - start + 1);
        // 跳过注释行
        if (trimmed.empty() || trimmed[0] == '#') continue;
        // 解析 nameserver <ip>
        istringstream iss(trimmed);
        string keyword, ip;
        iss >> keyword >> ip;
        if (keyword == "nameserver" && !ip.empty()) {
            auto *upstream = new remote_dns_server(ip, 53, "UDP");
            upstream->timeout = 2000;// 系统 DNS 一般在内网，2秒够用
            system_upstream_servers.emplace_back(upstream);
            count++;
            logger::INFO << "auto upstream DNS detected: " << ip << END;
        }
    }
    if (count == 0) {
        logger::WARN << "no nameserver found in " << best_path << END;
    } else {
        logger::INFO << "loaded " << count << " upstream DNS server(s) from " << best_path << END;
    }
}

void st::dns::config::load(const string &base_conf_dir) {
    unload();
    this->base_conf_dir = base_conf_dir;
    string config_path = base_conf_dir + "/config.json";
    if (st::utils::file::exists(config_path)) {
        ptree tree;
        try {
            read_json(config_path, tree);
        } catch (json_parser_error &e) {
            logger::ERROR << " parse config file " + config_path + " error!" << e.message() << END;
            exit(1);
        }
        logger::init(tree);
        st::utils::apm_logger::init();
        this->ip = tree.get("ip", string("127.0.0.1"));
        this->port = tree.get("port", port);
        this->console_port = tree.get("console_port", console_port);
        this->console_ip = tree.get("console_ip", string("127.0.0.1"));
        this->dns_cache_expire = stoi(tree.get("dns_cache_expire", to_string(this->dns_cache_expire)));
        this->forward_max_running = tree.get("forward_max_running", this->forward_max_running);
        if (this->forward_max_running == 0) {
            this->forward_max_running = 32;
        }
        this->auto_upstream_dns = tree.get("auto_upstream_dns", true);
        // 如果用户配置了 resolv_conf_paths 列表，则替换默认候选列表
        auto resolv_paths_node = tree.get_child_optional("resolv_conf_paths");
        if (resolv_paths_node.is_initialized()) {
            this->resolv_conf_paths.clear();
            for (auto &v : resolv_paths_node.get()) {
                string path = v.second.get_value<string>();
                if (!path.empty()) {
                    this->resolv_conf_paths.push_back(path);
                }
            }
        }

        load_system_dns();

        auto servers_nodes = tree.get_child("servers");
        if (!servers_nodes.empty()) {
            for (auto it = servers_nodes.begin(); it != servers_nodes.end(); it++) {
                auto server_node = it->second;
                string serverIp = server_node.get("ip", "");
                int server_port = server_node.get("port", 53);
                string type = server_node.get("type", "UDP");
                vector<string> areas;
                auto areas_node = server_node.get_child_optional("areas");
                if (areas_node.is_initialized()) {
                    auto areas_arr = areas_node.get();
                    for (boost::property_tree::ptree::value_type &v : areas_arr) {
                        string area = v.second.get_value<string>();
                        if (!area.empty()) {
                            st::areaip::manager::uniq().load_area_ips(area);
                            areas.emplace_back(area);
                        }
                    }
                }
                if (serverIp == "AUTO_LAN_IP") {
                    bool is_lan_udp_server = type == "UDP" && find(areas.begin(), areas.end(), "LAN") != areas.end();
                    if (!is_lan_udp_server) {
                        logger::ERROR << "config server ip AUTO_LAN_IP only support UDP LAN server!" << END;
                        exit(1);
                    }
                    if (system_upstream_servers.empty()) {
                        logger::ERROR << "config server ip AUTO_LAN_IP but no system upstream DNS detected!" << END;
                        exit(1);
                    }
                    serverIp = system_upstream_servers[0]->ip;
                    logger::INFO << "config UDP LAN server ip AUTO_LAN_IP resolved to" << serverIp << END;
                }
                if (serverIp.empty()) {
                    logger::ERROR << "config server ip empty!" << END;
                    exit(1);
                }


                auto *dns_server = new remote_dns_server(serverIp, server_port, type);
                auto whitelist_node = server_node.get_child_optional("whitelist");
                if (whitelist_node.is_initialized()) {
                    auto whitelistArr = whitelist_node.get();
                    for (boost::property_tree::ptree::value_type &v : whitelistArr) {
                        dns_server->whitelist.emplace(v.second.get_value<string>());
                    }
                }
                auto blacklist_node = server_node.get_child_optional("blacklist");
                if (blacklist_node.is_initialized()) {
                    auto blacklistArr = blacklist_node.get();
                    for (boost::property_tree::ptree::value_type &v : blacklistArr) {
                        dns_server->blacklist.emplace(v.second.get_value<string>());
                    }
                }
                dns_server->dns_cache_expire = stoi(server_node.get("dns_cache_expire", to_string(this->dns_cache_expire)));
                dns_server->timeout = server_node.get("timeout", 100);

                dns_server->areas = areas;
                servers.emplace_back(dns_server);
            }
        }
        if (servers.empty()) {
            logger::ERROR << "st-dns config no servers" << END;
            exit(1);
        }
        auto area_ip_config_node = tree.get_child_optional("area_ip_config");
        if (area_ip_config_node.is_initialized()) {
            this->area_ip_config.load(area_ip_config_node.get());
            areaip::manager::uniq().config(this->area_ip_config);
        }
        st::areaip::manager::uniq().start();

        auto force_resolve_rules_node = tree.get_child_optional("force_resolve_rules");
        if (force_resolve_rules_node.is_initialized()) {
            for (auto it = force_resolve_rules_node.get().begin(); it != force_resolve_rules_node.get().end(); it++) {
                auto rule_node = it->second;
                string pattern = rule_node.get("pattern", "");
                string regex_pattern = rule_node.get("regex", "");
                const string regex_prefix = "re:";
                bool re_prefixed_pattern = pattern.size() >= regex_prefix.size() &&
                                           pattern.compare(0, regex_prefix.size(), regex_prefix) == 0;
                if (re_prefixed_pattern) {
                    if (!regex_pattern.empty()) {
                        logger::WARN << "force resolve rule re pattern and regex cannot both be configured, skip!" << END;
                        continue;
                    }
                    regex_pattern = pattern.substr(regex_prefix.size());
                    pattern.clear();
                    if (regex_pattern.empty()) {
                        logger::WARN << "force resolve rule re pattern empty, skip!" << END;
                        continue;
                    }
                }
                if (pattern.empty() && regex_pattern.empty()) {
                    logger::WARN << "force resolve rule pattern and regex empty, skip!" << END;
                    continue;
                }
                if (!pattern.empty() && !regex_pattern.empty()) {
                    logger::WARN << "force resolve rule pattern and regex cannot both be configured, skip!" << END;
                    continue;
                }
                vector<uint32_t> ips;
                auto ips_node = rule_node.get_child_optional("ips");
                if (ips_node.is_initialized()) {
                    for (auto &ip_node : ips_node.get()) {
                        string ip_str = ip_node.second.get_value<string>();
                        uint32_t ip = st::utils::ipv4::str_to_ip(ip_str);
                        if (ip != 0) {
                            ips.push_back(ip);
                        }
                    }
                }
                if (!ips.empty()) {
                    try {
                        auto *rule = new force_resolve_rule(pattern, ips, regex_pattern);
                        force_resolve_rules.push_back(rule);
                        logger::INFO << "load force resolve rule"
                                     << (regex_pattern.empty() ? pattern : regex_pattern)
                                     << st::utils::ipv4::ips_to_str(ips) << END;
                    } catch (const std::regex_error &e) {
                        logger::WARN << "force resolve rule regex invalid, skip!"
                                     << regex_pattern << e.what() << END;
                    }
                } else {
                    logger::WARN << "force resolve rule ips empty"
                                 << (regex_pattern.empty() ? pattern : regex_pattern) << END;
                }
            }
        }
        loaded = true;
    } else {
        logger::ERROR << "st-dns config file not exit！" << config_path << END;
        exit(1);
    }
}

vector<remote_dns_server *>
remote_dns_server::select_servers(const string &domain, const vector<remote_dns_server *> &servers) {
    vector<remote_dns_server *> result;
    string fiDomain = st::dns::protocol::dns_domain::getFIDomain(domain);
    for (auto it = servers.begin(); it != servers.end(); it++) {
        remote_dns_server *server = *it.base();
        if ((fiDomain == "LAN" || fiDomain == "LOCAL" || fiDomain == "ARPA") && find(server->areas.begin(), server->areas.end(), "LAN") == server->areas.end()) {
            continue;
        }
        for (auto &regex : server->whitelist) {
            bool in_whitelist = domain == regex || regex.find("." + domain) != string::npos;
            if (!in_whitelist) {
                std::regex reg(regex);
                if (std::regex_match(domain, reg)) {
                    in_whitelist = true;
                }
            }
            if (in_whitelist) {
                result.clear();
                result.emplace_back(server);
                return result;
            }
        }
        bool in_blacklist = false;
        for (auto &pattern : server->blacklist) {
            if (domain == pattern || pattern.find("." + domain) != string::npos) {
                in_blacklist = true;
            } else {
                std::regex reg(pattern);
                if (std::regex_match(domain, reg)) {
                    in_blacklist = true;
                }
            }
            if (in_blacklist) break;
        }
        if (in_blacklist) {
            continue;
        }
        result.emplace_back(server);
    }

    return result;
}

remote_dns_server::remote_dns_server(const string &ip, int port, const string &type) : ip(ip), port(port), type(type) {
}

force_resolve_rule::force_resolve_rule(const string &pattern, const vector<uint32_t> &ips,
                                       const string &regex_pattern)
        : pattern(pattern), regex_pattern(regex_pattern), ips(ips) {
    if (!regex_pattern.empty()) {
        regex = std::regex(regex_pattern);
        use_regex = true;
    }
}

bool force_resolve_rule::match(const string &domain) const {
    if (use_regex) {
        return std::regex_match(domain, regex);
    }

    // 精确匹配
    if (pattern == domain) {
        return true;
    }

    // 通配符匹配 *.example.com
    if (pattern.size() > 2 && pattern[0] == '*' && pattern[1] == '.') {
        string suffix = pattern.substr(1); // .example.com
        // 检查域名是否以 .example.com 结尾
        if (domain.size() >= suffix.size() &&
            domain.compare(domain.size() - suffix.size(), suffix.size(), suffix) == 0) {
            return true;
        }
        // 检查是否完全匹配去掉 *. 后的部分 (example.com)
        string base = pattern.substr(2);
        if (domain == base) {
            return true;
        }
    }

    return false;
}
