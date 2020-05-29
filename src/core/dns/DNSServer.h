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
    std::map<uint16_t, DNSSession *> sessions;
    byte bufferData[1024];
    udp::endpoint clientEndpoint;
    udp::socket *socketS = nullptr;
    io_context ioContext;
    thread_pool pool;
    int i = 0;
public:
    DNSServer(uint16_t port);

private:
    void receive();

    void proxyDnsOverTcpTls(UdpDnsRequest *udpDnsRequest);
};


#endif //ST_DNS_DNSSERVER_H
