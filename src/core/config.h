//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_DNS_CONFIG_H
#define ST_DNS_CONFIG_H

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "st.h"
#include "protocol/protocol.h"
using namespace std;
using namespace boost::property_tree;
class remote_dns_server {
public:
    string ip;
    int port;
    string type;
    unordered_set<string> whitelist;
    unordered_set<string> blacklist;
    vector<string> areas;
    int timeout = 100;
    int dns_cache_expire = 10 * 60;

    static string generate_server_id(const string &serverIp, int serverPort) {
        return boost::algorithm::ireplace_all_copy(serverIp, ".", "_") + "_" + to_string(serverPort);
    }

    remote_dns_server(const string &ip, int port, const string &type);

    string id() const {
        return generate_server_id(ip, port);
    };

    static vector<remote_dns_server *> select_servers(const string &domain, const vector<remote_dns_server *> &servers);
};

namespace st {
    namespace dns {


        class config {
        public:
            static config INSTANCE;
            string ip = "127.0.0.1";
            int port = 53;
            string console_ip = "127.0.0.1";
            int console_port = 5757;
            uint32_t dns_cache_expire = 60 * 10;
            string base_conf_dir = "/usr/local/etc/st/dns";
            vector<remote_dns_server *> servers;
            bool area_resolve_optimize = false;
            st::areaip::area_ip_config area_ip_config;

            config() = default;
            void load(const string &base_conf_dir);
            remote_dns_server *get_dns_server_by_id(string serverId);
        };

    }// namespace dns
}// namespace st

#endif//ST_DNS_CONFIG_H
