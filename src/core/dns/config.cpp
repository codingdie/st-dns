//
// Created by System Administrator on 2020/10/8.
//
#include "config.h"

st::dns::config st::dns::config::INSTANCE;
void st::dns::config::load(const string &base_conf_dir) {
    this->base_conf_dir = base_conf_dir;
    string config_path = base_conf_dir + "/config.json";
    if (st::utils::file::exit(config_path)) {
        ptree tree;
        try {
            read_json(config_path, tree);
        } catch (json_parser_error &e) {
            logger::ERROR << " parse config file " + config_path + " error!" << e.message() << END;
            exit(1);
        }
        this->ip = tree.get("ip", string("127.0.0.1"));
        this->port = stoi(tree.get("port", string("1080")));
        this->dns_cache_expire = stoi(tree.get("dns_cache_expire", to_string(this->dns_cache_expire)));
        this->dns_cache_file = tree.get("dns_cache_file", this->dns_cache_file);
        this->area_resolve_optimize = tree.get("area_resolve_optimize", false);
        if (!file::create_if_not_exits(this->dns_cache_file)) {
            logger::ERROR << "create dns_cache_file file error!" << this->dns_cache_file << END;
            exit(1);
        }

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


                remote_dns_server *dns_server = new remote_dns_server(serverIp, server_port, type);
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
                            if (!st::areaip::manager::uniq().load_area_ips(area)) {
                                exit(1);
                            }
                            dns_server->areas.emplace_back(area);
                        }
                    }
                }
                for (auto it = dns_server->areas.begin(); it != dns_server->areas.end(); it++) {
                    st::dns::shm::share().set_virtual_port(st::utils::ipv4::str_to_ip(dns_server->ip), dns_server->port, *it);
                }
                servers.emplace_back(dns_server);
            }
        }
        if (servers.empty()) {
            logger::ERROR << "st-dns config no servers" << END;
            exit(1);
        }
        logger::init(tree);
    } else {
        logger::ERROR << "st-dns config file not exit！" << config_path << END;
        exit(1);
    }
}
remote_dns_server *st::dns::config::get_dns_server_by_id(string serverId) {
    for (auto it = servers.begin(); it != servers.end(); it++) {
        remote_dns_server *server = *it.base();
        if (server->id() == serverId) {
            return server;
        }
    }
    return nullptr;
}

vector<remote_dns_server *>
remote_dns_server::calculateQueryServer(const string &domain, const vector<remote_dns_server *> &servers) {
    vector<remote_dns_server *> result;
    string fiDomain = st::dns::protocol::dns_domain::getFIDomain(domain);
    for (auto it = servers.begin(); it != servers.end(); it++) {
        remote_dns_server *server = *it.base();

        if ((fiDomain == "LAN" || fiDomain == "ARPA") && find(server->areas.begin(), server->areas.end(), "LAN") == server->areas.end()) {
            continue;
        }

        auto whiteIterator = server->whitelist.find(domain);
        if (whiteIterator != server->whitelist.end()) {
            result.clear();
            result.emplace_back(server);
            break;
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
