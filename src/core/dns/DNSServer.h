//
// Created by codingdie on 2020/5/19.
//

#ifndef ST_DNS_DNSSERVER_H
#define ST_DNS_DNSSERVER_H

#include "Config.h"
#include "DNSSession.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;

class DNSServer {
public:
    DNSServer(st::dns::Config &config);

    void start();

private:
    st::dns::Config config;
    udp::socket *socketS = nullptr;
    io_context ioContext;
    thread_pool pool;
    int i = 0;

private:
    void receive();

    void proxyDnsOverTcpTls(DNSSession *session);

    set<uint32_t> queryDNS(const string &host) const;

    set<uint32_t> queryDNS(const string &host, const RemoteDNSServer *server) const;
};


#endif //ST_DNS_DNSSERVER_H
