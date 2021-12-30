//
// Created by codingdie on 2020/6/20.
//

#ifndef ST_DNS_REMOTEDNSSERVER_H
#define ST_DNS_REMOTEDNSSERVER_H

#include "utils/STUtils.h"
#include <boost/algorithm/string/replace.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>

using namespace std;

class RemoteDNSServer {
public:
    string ip;
    int port;
    string type;
    unordered_set<string> whitelist;
    unordered_set<string> blacklist;
    string area;
    bool onlyAreaIp;
    int timeout = 100;
    int dnsCacheExpire = 60 * 60;

    static string generateServerId(const string &serverIp, int serverPort) {
        return boost::algorithm::ireplace_all_copy(serverIp, ".", "_") + "_" + to_string(serverPort);
    }

    RemoteDNSServer(const string &ip, int port, const string &type, const string &area, bool onlyAreaIp);

    string id() const {
        return generateServerId(ip, port);
    };

    static vector<RemoteDNSServer *> calculateQueryServer(const string &domain, const vector<RemoteDNSServer *> &servers);
};
#endif//ST_DNS_REMOTEDNSSERVER_H