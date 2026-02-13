//
// Created by System Administrator on 2020/10/8.
//
#include "config.h"
#include "utils/area_ip.h"

#include "command/proxy_command.h"
#include <regex>
st::dns::config st::dns::config::INSTANCE;

st::dns::config::~config() {
    for (auto server : servers) {
        delete server;
    }
    servers.clear();
    for (auto rule : force_resolve_rules) {
        delete rule;
    }
    force_resolve_rules.clear();
}

void st::dns::config::load(const string &base_conf_dir) {
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
        this->ip = tree.get("ip", string("127.0.0.1"));
        this->port = tree.get("port", port);
        this->console_port = tree.get("console_port", console_port);
        this->console_ip = tree.get("console_ip", string("127.0.0.1"));
        this->dns_cache_expire = stoi(tree.get("dns_cache_expire", to_string(this->dns_cache_expire)));

        auto servers_nodes = tree.get_child("servers");
        if (!servers_nodes.empty()) {
            for (auto it = servers_nodes.begin(); it != servers_nodes.end(); it++) {
                auto server_node = it->second;
                string serverIp = server_node.get("ip", "");
                if (serverIp.empty()) {
                    logger::ERROR << "config server ip empty!" << END;
                    exit(1);
                }
                int server_port = server_node.get("port", 53);
                string type = server_node.get("type", "UDP");
                string filename = remote_dns_server::generate_server_id(serverIp, server_port);


                auto *dns_server = new remote_dns_server(serverIp, server_port, type);
                auto whitelist_node = server_node.get_child_optional("whitelist");
                if (whitelist_node.is_initialized()) {
                    auto whitelistArr = whitelist_node.get();
                    for (boost::property_tree::ptree::value_type &v : whitelistArr) {
                        dns_server->whitelist.emplace(v.second.get_value<string>());
                    }
                }
                dns_server->dns_cache_expire = stoi(server_node.get("dns_cache_expire", to_string(this->dns_cache_expire)));
                dns_server->timeout = server_node.get("timeout", 100);

                auto areas_node = server_node.get_child_optional("areas");
                if (areas_node.is_initialized()) {
                    auto areas_arr = areas_node.get();
                    for (boost::property_tree::ptree::value_type &v : areas_arr) {
                        string area = v.second.get_value<string>();
                        if (!area.empty()) {
                            st::areaip::manager::uniq().load_area_ips(area);
                            dns_server->areas.emplace_back(area);
                        }
                    }
                }
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

        auto force_resolve_rules_node = tree.get_child_optional("force_resolve_rules");
        if (force_resolve_rules_node.is_initialized()) {
            for (auto it = force_resolve_rules_node.get().begin(); it != force_resolve_rules_node.get().end(); it++) {
                auto rule_node = it->second;
                string pattern = rule_node.get("pattern", "");
                if (pattern.empty()) {
                    logger::WARN << "force resolve rule pattern empty, skip!" << END;
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
                    auto *rule = new force_resolve_rule(pattern, ips);
                    force_resolve_rules.push_back(rule);
                    logger::INFO << "load force resolve rule" << pattern << st::utils::ipv4::ips_to_str(ips) << END;
                } else {
                    logger::WARN << "force resolve rule ips empty" << pattern << END;
                }
            }
        }
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
        if ((fiDomain == "LAN" || fiDomain == "ARPA") && find(server->areas.begin(), server->areas.end(), "LAN") == server->areas.end()) {
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
        auto blackIterator = server->blacklist.find(domain);
        if (blackIterator != server->blacklist.end()) {
            continue;
        }
        result.emplace_back(server);
    }

    return result;
}

remote_dns_server::remote_dns_server(const string &ip, int port, const string &type) : ip(ip), port(port), type(type) {
}

force_resolve_rule::force_resolve_rule(const string &pattern, const vector<uint32_t> &ips) : pattern(pattern), ips(ips) {
}

bool force_resolve_rule::match(const string &domain) const {
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
