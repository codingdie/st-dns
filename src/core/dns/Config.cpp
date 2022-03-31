//
// Created by System Administrator on 2020/10/8.
//
#include "Config.h"

st::dns::Config st::dns::Config::INSTANCE;
void st::dns::Config::load(const string &baseConfDir) {
    this->baseConfDir = baseConfDir;
    string configPath = baseConfDir + "/config.json";
    if (st::utils::file::exit(configPath)) {
        ptree tree;
        try {
            read_json(configPath, tree);
        } catch (json_parser_error &e) {
            Logger::ERROR << " parse config file " + configPath + " error!" << e.message() << END;
            exit(1);
        }
        this->ip = tree.get("ip", string("127.0.0.1"));
        this->port = stoi(tree.get("port", string("1080")));
        this->dnsCacheExpire = stoi(tree.get("dns_cache_expire", to_string(this->dnsCacheExpire)));
        this->dnsCacheFile = tree.get("dns_cache_file", this->dnsCacheFile);
        this->areaResolveOptimize = tree.get("area_resolve_optimize", false);
        if (!file::createIfNotExits(this->dnsCacheFile)) {
            Logger::ERROR << "create dnsCacheFile file error!" << this->dnsCacheFile << END;
            exit(1);
        }

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


                RemoteDNSServer *dnsServer = new RemoteDNSServer(serverIp, serverPort, type);
                auto whitelistNode = serverNode.get_child_optional("whitelist");
                if (whitelistNode.is_initialized()) {
                    auto whitelistArr = whitelistNode.get();
                    for (boost::property_tree::ptree::value_type &v : whitelistArr) {
                        dnsServer->whitelist.emplace(v.second.get_value<string>());
                    }
                }
                dnsServer->dnsCacheExpire = stoi(serverNode.get("dns_cache_expire", to_string(this->dnsCacheExpire)));
                dnsServer->timeout = serverNode.get("timeout", 100);
                auto areasNode = serverNode.get_child_optional("areas");
                if (areasNode.is_initialized()) {
                    auto areasArr = areasNode.get();
                    for (boost::property_tree::ptree::value_type &v : areasArr) {
                        string area = v.second.get_value<string>();
                        if (!area.empty()) {
                            if (!st::areaip::Manager::uniq().loadAreaIPs(area)) {
                                exit(1);
                            }
                            dnsServer->areas.emplace_back(area);
                        }
                    }
                }
                for (auto it = dnsServer->areas.begin(); it != dnsServer->areas.end(); it++) {
                    st::dns::SHM::write().initVirtualPort(st::utils::ipv4::strToIp(dnsServer->ip), dnsServer->port, *it);
                }
                servers.emplace_back(dnsServer);
            }
        }
        if (servers.empty()) {
            Logger::ERROR << "st-dns config no servers" << END;
            exit(1);
        }
        Logger::init(tree);
    } else {
        Logger::ERROR << "st-dns config file not exit！" << configPath << END;
        exit(1);
    }
}
RemoteDNSServer *st::dns::Config::getDNSServerById(string serverId) {
    for (auto it = servers.begin(); it != servers.end(); it++) {
        RemoteDNSServer *server = *it.base();
        if (server->id() == serverId) {
            return server;
        }
    }
    return nullptr;
}