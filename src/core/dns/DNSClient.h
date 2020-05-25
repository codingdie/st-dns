//
// Created by codingdie on 2020/5/20.
//

#ifndef ST_DNS_DNSCLIENT_H
#define ST_DNS_DNSCLIENT_H

#include <string>
#include <boost/asio/basic_datagram_socket.hpp>
#include <boost/asio.hpp>
#include "Logger.h"
#include "DNS.h"

using namespace boost::asio::ip;
using namespace boost::asio;

using namespace std;

class DNSClient {
public:
    static DNSClient instance;

    static UdpDNSResponse *udpDns(std::string &domain, std::string &dnsServer);

    static UdpDNSResponse *udpDns(std::vector<string> &domains, std::string &dnsServer);

    static UdpDNSResponse *tcpDns(std::string &domain, std::string &dnsServer);

    static UdpDNSResponse *tcpDns(std::vector<string> &domains, std::string &dnsServer);

    DNSClient();

    virtual ~DNSClient();

private:
    boost::asio::io_context ioContext;

    boost::asio::io_context::work *ioContextWork;

    thread *ioThread;

    UdpDNSResponse *queryUdp(std::vector<string> &domains, std::string &dnsServer);

    UdpDNSResponse *queryTcp(std::vector<string> &domains, std::string &dnsServer);
};

#endif //ST_DNS_DNSCLIENT_H