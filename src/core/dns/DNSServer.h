//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_DNS_DNSSERVER_H
#define ST_DNS_DNSSERVER_H

#include "Config.h"
#include "DNSSession.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "STUtils.h"
#include <string>
#include <unordered_set>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;

class DNSServer {
public:
    DNSServer(st::dns::Config &config);

    void start();

private:
    st::dns::Config config;
    udp::socket *socketS = nullptr;
    io_context ioContext;
    unordered_set<string> hostsInQuery;

    void receive();

    void proxyDnsOverTcpTls(DNSSession *session);

    set<uint32_t> queryDNS(const string &host);

    set<uint32_t> queryDNS(const string &host, const RemoteDNSServer *server) const;

    void filterIPByArea(const string &host, const RemoteDNSServer *server, set<uint32_t> &ips) const;
};


#endif //ST_DNS_DNSSERVER_H
