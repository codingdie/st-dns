//
// Created by codingdie on 2020/5/20.
//

#ifndef ST_DNS_DNSCLIENT_H
#define ST_DNS_DNSCLIENT_H

static const int DEFAULT_DNS_PORT = 53;

#include "DNS.h"
#include "utils/STUtils.h"
#include <boost/asio.hpp>
#include <boost/asio/basic_datagram_socket.hpp>
#include <boost/asio/ssl.hpp>
#include <string>
#include <vector>
#include <thread>

using namespace boost::asio::ip;
using namespace boost::asio;

using namespace std;

class DNSClient {
public:
    static DNSClient INSTANCE;
    static UdpDNSResponse *
    forwardUdpDNS(UdpDnsRequest &UdpDnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout);

    //    static TcpDNSResponse *EDNS(const std::string &domain, const std::string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout);

    void asyncForwardUdp(UdpDnsRequest &udpDnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(UdpDNSResponse *)> callback);

    UdpDNSResponse *
    udpDns(const std::vector<string> &domains, const std::string &dnsServer, uint32_t port, uint64_t timeout);

    void udpDns(const string domain, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(UdpDNSResponse *)> completeHandler);

    UdpDNSResponse *
    forwardUdp(UdpDnsRequest &UdpDnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout);

    TcpDNSResponse *
    tcpOverTls(const std::vector<string> &domains, const std::string &dnsServer, uint16_t port, uint64_t timeout);

    void
    tcpOverTls(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(TcpDNSResponse *)> completeHandler);


    void tcpDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(TcpDNSResponse *)> completeHandler);

    TcpDNSResponse *
    tcpDNS(const std::vector<string> &domains, const std::string &dnsServer, uint16_t port, uint64_t timeout);


    DNSClient();

    virtual ~DNSClient();

private:
    io_context ioContext;
    boost::asio::io_context::work *ioWoker;
    std::thread *th;

    //    TcpDNSResponse *queryEDNS(const vector<string> &domains, const string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout);
    bool
    isTimeoutOrError(std::function<void()> completeHandler, future<boost::system::error_code> &future,
                     uint64_t begin, uint64_t timeout, uint64_t &restTime, string logTag, const string step);


    void setSSLSocketTimeoutOpts(const string &logTag, ssl::stream<tcp::socket> &socket, uint64_t timeout) const;
};

#endif//ST_DNS_DNSCLIENT_H