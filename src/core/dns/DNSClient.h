//
// Created by codingdie on 2020/5/20.
//

#ifndef ST_DNS_DNSCLIENT_H
#define ST_DNS_DNSCLIENT_H

static const int DEFAULT_DNS_PORT = 53;

#include "DNS.h"
#include "STUtils.h"
#include <boost/asio.hpp>
#include <boost/asio/basic_datagram_socket.hpp>
#include <boost/asio/ssl.hpp>
#include <string>
#include <vector>

using namespace boost::asio::ip;
using namespace boost::asio;

using namespace std;

class DNSClient {
public:
    static DNSClient instance;

    static UdpDNSResponse *
    udpDns(const std::string &domain, const std::string &dnsServer, uint32_t port, uint64_t timeout);

    static UdpDNSResponse *
    udpDns(const std::vector<string> &domains, const std::string &dnsServer, uint32_t port, uint64_t timeout);

    static TcpDNSResponse *
    tcpTlsDns(const std::string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout);

    static TcpDNSResponse *
    tcpTlsDns(const std::vector<string> &domains, const std::string &dnsServer, uint16_t port, uint64_t timeout);

    static TcpDNSResponse *
    tcpDns(const std::string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout);

    static TcpDNSResponse *
    tcpDns(const std::vector<string> &domains, const std::string &dnsServer, uint16_t port, uint64_t timeout);

    static UdpDNSResponse *
    forwardUdpDNS(UdpDnsRequest &UdpDnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout);

    //    static TcpDNSResponse *EDNS(const std::string &domain, const std::string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout);


    DNSClient();

    virtual ~DNSClient();

private:
    UdpDNSResponse *
    queryUdp(const std::vector<string> &domains, const std::string &dnsServer, uint32_t port, uint64_t timeout);
   
    UdpDNSResponse *
    forwardUdp(UdpDnsRequest& UdpDnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout);

    TcpDNSResponse *
    queryTcpOverTls(const std::vector<string> &domains, const std::string &dnsServer, uint16_t port, uint64_t timeout);

    TcpDNSResponse *
    queryTcp(const std::vector<string> &domains, const std::string &dnsServer, uint16_t port, uint64_t timeout);

    //    TcpDNSResponse *queryEDNS(const vector<string> &domains, const string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout);

    bool
    isTimeoutOrError(std::function<void()> completeHandler, future<boost::system::error_code> &future,
                     uint64_t begin, uint64_t timeout, uint64_t &restTime, string logTag, const string step);


    void setSSLSocketTimeoutOpts(const string &logTag, ssl::stream<tcp::socket> &socket, uint64_t timeout) const;
};

#endif//ST_DNS_DNSCLIENT_H