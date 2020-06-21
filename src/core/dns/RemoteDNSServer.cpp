//
// Created by codingdie on 2020/6/20.
//

#include "RemoteDNSServer.h"

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

const RemoteDNSServer &
RemoteDNSServer::calculateQueryServer(const string &domain, const vector<RemoteDNSServer> &servers) {
    for (auto it = servers.begin(); it != servers.end(); it++) {
        const RemoteDNSServer &server = *it.base();
        auto whiteIterator = server.whitelist.find(domain);
        if (whiteIterator != server.whitelist.end()) {
            return server;
        }
        auto blackIterator = server.blacklist.find(domain);
        if (blackIterator == server.blacklist.end()) {
            continue;
        }
        return server;
    }

    return *servers.begin().base();
}
