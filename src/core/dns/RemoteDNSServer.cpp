//
// Created by codingdie on 2020/6/20.
//

#include "RemoteDNSServer.h"
#include "Config.h"
#include "DNS.h"

vector<RemoteDNSServer *>
RemoteDNSServer::calculateQueryServer(const string &domain, const vector<RemoteDNSServer *> &servers) {
    vector<RemoteDNSServer *> result;
    string fiDomain = DNSDomain::getFIDomain(domain);
    for (auto it = servers.begin(); it != servers.end(); it++) {
        RemoteDNSServer *server = *it.base();

        if ((fiDomain == "LAN" || fiDomain == "ARPA") && server->areas.find("LAN") == server->areas.end()) {
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

RemoteDNSServer::RemoteDNSServer(const string &ip, int port, const string &type) : ip(ip), port(port), type(type) {
}
