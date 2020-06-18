//
// Created by codingdie on 2020/5/20.
//

#ifndef ST_DNS_DNSCLIENT_H
#define ST_DNS_DNSCLIENT_H

static const int DEFAULT_DNS_PORT = 53;

#include <string>
#include <boost/asio/basic_datagram_socket.hpp>
#include <boost/asio.hpp>
#include "STUtils.h"
#include "DNS.h"
#include <boost/asio/ssl.hpp>

using namespace boost::asio::ip;
using namespace boost::asio;

using namespace std;

class DNSClient {
public:
    static DNSClient instance;

    static UdpDNSResponse *udpDns(const std::string &domain, const std::string &dnsServer);

    static UdpDNSResponse *udpDns(std::vector<string> &domains, const std::string &dnsServer);

    static UdpDNSResponse *udpDns(const std::string &domain, const std::string &dnsServer, uint32_t port);

    static UdpDNSResponse *udpDns(std::vector<string> &domains, const std::string &dnsServer, uint32_t port);

    static TcpDNSResponse *tcpDns(const std::string &domain, const std::string &dnsServer);

    static TcpDNSResponse *tcpDns(const std::string &domain, const std::string &dnsServer, uint32_t timout);

    static TcpDNSResponse *tcpDns(std::vector<string> &domains, const std::string &dnsServer);

    static TcpDNSResponse *tcpDns(std::vector<string> &domains, const std::string &dnsServer, uint32_t timout);

    DNSClient();

    virtual ~DNSClient();

private:
    boost::asio::io_context ioContext;

    boost::asio::ssl::context *sslCtx;

    boost::asio::io_context::work *ioContextWork;

    thread *ioThread;

    UdpDNSResponse *queryUdp(std::vector<string> &domains, const std::string &dnsServer, uint32_t port);

    TcpDNSResponse *queryTcp(std::vector<string> &domains, const std::string &dnsServer);

    TcpDNSResponse *queryTcp(std::vector<string> &domains, const std::string &dnsServer, uint32_t timeout);
};

#endif //ST_DNS_DNSCLIENT_H