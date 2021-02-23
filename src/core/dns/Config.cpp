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
        } catch (json_parser_error e) {
            Logger::ERROR << " parse config file " + configPath + " error!" << e.message() << END;
            exit(1);
        }
        this->ip = tree.get("ip", string("127.0.0.1"));
        this->port = stoi(tree.get("port", string("1080")));
        this->dnsCacheExpire = stoi(tree.get("dns_cache_expire", to_string(this->dnsCacheExpire)));
        this->stProxyConfDir = tree.get("st_proxy_conf", string(""));
        this->parallel = stoi(tree.get("parallel", to_string(this->parallel)));
        auto logConfig = tree.get_child_optional("log");
        if (logConfig.is_initialized()) {
            this->logger.logLevel = logConfig.get().get<int>("level");
            auto rawLogServerConfig = logConfig.get().get_child_optional("raw_log_server");
            auto apmLogServerConfig = logConfig.get().get_child_optional("apm_log_server");
            if (rawLogServerConfig.is_initialized()) {
                this->logger.rawLogServer.ip = rawLogServerConfig.get().get<string>("ip", "");
                this->logger.rawLogServer.port = rawLogServerConfig.get().get<uint16_t>("port", 0);
                Logger::udpServerIP = this->logger.rawLogServer.ip;
                Logger::udpServerPort = this->logger.rawLogServer.port;
            }
            if (apmLogServerConfig.is_initialized()) {
                this->logger.apmLogServer.ip = apmLogServerConfig.get().get<string>("ip", "");
                this->logger.apmLogServer.port = apmLogServerConfig.get().get<uint16_t>("port", 0);
                APMLogger::udpServerIP = this->logger.apmLogServer.ip;
                APMLogger::udpServerPort = this->logger.apmLogServer.port;
            }
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
                string whitelist = serverNode.get("whitelist", baseConfDir + "/whitelist/" + filename);
                string blacklist = serverNode.get("blacklist", baseConfDir + "/blacklist/" + filename);
                string area = serverNode.get("area", "");
                bool onlyAreaIp = serverNode.get("only_area_ip", false);

                RemoteDNSServer *dnsServer = new RemoteDNSServer(serverIp, serverPort, type, whitelist,
                                                                 blacklist, area, onlyAreaIp);
                dnsServer->dnsCacheExpire = stoi(tree.get("dns_cache_expire", to_string(this->dnsCacheExpire)));
                dnsServer->parallel = stoi(tree.get("parallel", to_string(this->parallel)));
                bool onlyAreaDomain = serverNode.get("only_area_domain", false);
                dnsServer->onlyAreaDomain = onlyAreaDomain;
                int timeout = serverNode.get("timeout", 100);
                dnsServer->timeout = timeout;
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
                Logger::ERROR << "st-dns config  remote dns server init failed!" << remoteDnsServer->ip
                              << END;
                exit(1);
            }
        }

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