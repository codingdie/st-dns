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

#include "RemoteDNSServer.h"
#include "STUtils.h"

using namespace std;
using namespace boost::property_tree;
using namespace std;
namespace st {
    namespace dns {
        class Config {
        public:
            static Config INSTANCE;
            string ip = "127.0.0.1";
            int port = 53;
            int dnsCacheExpire = 60 * 60;
            string dnsCacheFile = "/var/lib/st/dns/cache.txt";
            string baseConfDir = "/usr/local/etc/st/dns";
            vector<RemoteDNSServer *> servers;
            uint8_t parallel = 4;
            Config() = default;
            void load(const string &baseConfDir);
            RemoteDNSServer *getDNSServerById(string serverId);
        };

    }// namespace dns
}// namespace st

#endif//ST_DNS_CONFIG_H
