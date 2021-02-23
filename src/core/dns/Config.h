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

        class UDPLogServer {
        public:
            string ip = "";
            uint16_t port = 0;
        };
        class LogConfig {
        public:
            uint8_t logLevel = 2;
            UDPLogServer rawLogServer;
            UDPLogServer apmLogServer;
        };
        class Config {
        public:
            static Config INSTANCE;
            string ip = "127.0.0.1";
            int port = 53;
            int dnsCacheExpire = 60 * 60;
            string dnsCacheFile = "";
            string baseConfDir = "/usr/local/etc/st/dns";
            vector<RemoteDNSServer *> servers;
            string stProxyConfDir = "";
            uint8_t parallel = 4;
            LogConfig logger;
            Config() = default;
            void load(const string &baseConfDir);
            RemoteDNSServer *getDNSServerById(string serverId);
        };

    }// namespace dns
}// namespace st

#endif//ST_DNS_CONFIG_H
