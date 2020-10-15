//
// Created by codingdie on 2020/6/20.
//

#include "RemoteDNSServer.h"
#include "Config.h"

bool RemoteDNSServer::init() {
    if (!file::createIfNotExits(blacklistFilePath)) {
        Logger::ERROR << "create blacklist file error!" << blacklistFilePath << END;
        return false;
    }
    if (!file::createIfNotExits(whitelistFilePath)) {
        Logger::ERROR << "create whitelist file error!" << whitelistFilePath << END;
        return false;
    }
    file::read(blacklistFilePath, blacklist);
    file::read(whitelistFilePath, whitelist);
    Logger::INFO << id() << "read blacklist size:" << blacklist.size() << END;
    Logger::INFO << id() << "read whitelist size:" << whitelist.size() << END;
    return true;
}

vector<RemoteDNSServer *> RemoteDNSServer::calculateQueryServer(const string &domain, const vector<RemoteDNSServer *> &servers) {
    set<string> &tunnelRealHosts = st::dns::Config::INSTANCE.stProxyTunnelRealHosts;
    vector<RemoteDNSServer *> result;
    for (auto it = servers.begin(); it != servers.end(); it++) {
        RemoteDNSServer *server = *it.base();
        if (tunnelRealHosts.find(domain) != tunnelRealHosts.end()) {
            result.emplace_back(server);
            continue;
        }
        auto whiteIterator = server->whitelist.find(domain);
        if (whiteIterator != server->whitelist.end()) {
            result.emplace_back(server);
            continue;
        }
        auto blackIterator = server->blacklist.find(domain);
        if (blackIterator != server->blacklist.end()) {
            continue;
        }
        if (server->onlyAreaDomain) {
            unsigned long pos = domain.find_last_of('.');
            if (pos == string::npos) {
                continue;
            }
            auto fiDomain = domain.substr(pos + 1);
            transform(fiDomain.begin(), fiDomain.end(), fiDomain.begin(), ::toupper);
            if (fiDomain != server->area) {
                continue;
            }
        }
        result.emplace_back(server);
    }

    return move(result);
}

RemoteDNSServer::RemoteDNSServer(const string &ip, int port, const string &type, const string &whitelistFilePath, const string &blacklistFilePath,
                                 const string &area, bool onlyAreaIp) : ip(ip), port(port), type(type), whitelistFilePath(
        whitelistFilePath), blacklistFilePath(blacklistFilePath), area(area), onlyAreaIp(onlyAreaIp) {
}
