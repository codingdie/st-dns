//
// Created by codingdie on 2020/6/20.
//

#ifndef ST_DNS_REMOTEDNSSERVER_H
#define ST_DNS_REMOTEDNSSERVER_H

#include <string>
#include <set>
#include <iostream>
#include <fstream>
#include "Utils.h"
#include <boost/algorithm/string/replace.hpp>

using namespace std;

class RemoteDNSServer {
public:
    string ip;
    int port;
    string type;
    string whitelistFilePath;
    string blacklistFilePath;
    set<string> whitelist;
    set<string> blacklist;

    static string generateServerId(const string &serverIp, int serverPort) {
        return boost::algorithm::ireplace_all_copy(serverIp, ".", "_") + "_" + to_string(serverPort);
    }

    RemoteDNSServer(const string &ip, int port, const string &type, const string &whitelist, string blacklist) : ip(ip),
                                                                                                                 port(port),
                                                                                                                 type(type),
                                                                                                                 whitelistFilePath(
                                                                                                                         whitelist),
                                                                                                                 blacklistFilePath(
                                                                                                                         move(blacklist)) {};

    string id() const {
        return generateServerId(ip, port);
    };

    bool init();

    static const RemoteDNSServer &calculateQueryServer(const string &domain, const vector<RemoteDNSServer> &servers);
};

#include <string>
#include <vector>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include "Utils.h"

#endif //ST_DNS_REMOTEDNSSERVER_H