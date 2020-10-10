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
#include <vector>

using namespace boost::asio::ip;
using namespace boost::asio;

using namespace std;

class DNSClient {
public:
    static DNSClient instance;

    static UdpDNSResponse *udpDns(const std::string &domain, const std::string &dnsServer, uint32_t port, uint64_t timeout);

    static UdpDNSResponse *udpDns(const std::vector<string> &domains, const std::string &dnsServer, uint32_t port, uint64_t timeout);

    static TcpDNSResponse *tcpDns(const std::string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout);

    static TcpDNSResponse *tcpDns(const std::vector<string> &domains, const std::string &dnsServer, uint16_t port, uint64_t timeout);


    DNSClient();

    virtual ~DNSClient();

private:
    boost::asio::io_context ioContext;


    boost::asio::io_context::work *ioContextWork;

    vector<thread> ioThreads;

    UdpDNSResponse *queryUdp(const std::vector<string> &domains, const std::string &dnsServer, uint32_t port, uint64_t timeout);

    TcpDNSResponse *queryTcpOverTls(const std::vector<string> &domains, const std::string &dnsServer, uint16_t port, uint64_t timeout);

};

#endif //ST_DNS_DNSCLIENT_H