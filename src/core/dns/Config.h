//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_DNS_CONFIG_H
#define ST_DNS_CONFIG_H

#include <string>
#include <vector>
#include <boost/property_tree/json_parser.hpp>
#include "STUtils.h"
#include "RemoteDNSServer.h"
#include <boost/algorithm/string/replace.hpp>
#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost::property_tree;
using namespace std;
namespace st {
    namespace dns {


        class Config {
        public:
            static string BASE_CONF_PATH;
            string ip = "127.0.0.1";
            int port = 53;
            vector<RemoteDNSServer *> servers;

            Config() = default;

            Config(const string &baseConfDir) {
                BASE_CONF_PATH = boost::filesystem::absolute(baseConfDir).normalize().string();
                string configPath = BASE_CONF_PATH + "/st-dns.json";
                if (st::utils::file::exit(configPath)) {
                    ptree tree;
                    try {
                        read_json(configPath, tree);
                    } catch (json_parser_error e) {
                        Logger::ERROR << " parse config file " + configPath + " error!" << e.message() << END;
                        exit(1);
                    }
                    this->ip = tree.get("ip", string("127.0.0.1"));
                    this->port = stoi(tree.get("port", string("1080")));
                    auto serversNodes = tree.get_child("servers");
                    if (!serversNodes.empty()) {
                        for (auto it = serversNodes.begin(); it != serversNodes.end(); it++) {
                            auto serverNode = it->second;
                            string serverIp = serverNode.get("ip", "");
                            if (serverIp.empty()) {
                                Logger::ERROR << "config server ip empty!" << END;
                                exit(1);
                            }
                            int serverPort = serverNode.get("port", 53);
                            string type = serverNode.get("type", "UDP");
                            string filename = RemoteDNSServer::generateServerId(serverIp, serverPort);
                            string whitelist = serverNode.get("whitelist", BASE_CONF_PATH + "whitelist/" + filename);
                            string blacklist = serverNode.get("blacklist", BASE_CONF_PATH + "blacklist/" + filename);
                            string area = serverNode.get("area", "");
                            bool onlyAreaIp = serverNode.get("only_area_ip", false);
                            RemoteDNSServer *dnsServer = new RemoteDNSServer(serverIp, serverPort, type, whitelist, blacklist, area, onlyAreaIp);
                            servers.emplace_back(dnsServer);
                        }
                    }
                    if (servers.empty()) {
                        Logger::ERROR << "st-dns config no servers" << END;
                        exit(1);
                    }
                    for (auto it = servers.begin(); it != servers.end(); it++) {
                        RemoteDNSServer *remoteDnsServer = *(it.base());
                        if (!remoteDnsServer->init()) {
                            Logger::ERROR << "st-dns config  remote dns server init failed!" << remoteDnsServer->ip << END;
                            exit(1);
                        }
                    }

                } else {
                    Logger::INFO << "st-dns config file not exit！use default" << configPath << boost::filesystem::initial_path<boost::filesystem::path>().string() << END;
                }
            }


        };


    }
}


#endif //ST_DNS_CONFIG_H
