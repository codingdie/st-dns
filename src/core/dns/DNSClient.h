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

    void udpDns(const string domain, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(std::unordered_set<uint32_t> ips)> completeHandler);

    void tcpTlsDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const string &area, std::function<void(std::unordered_set<uint32_t> ips)> completeHandler);

    void tcpDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const string &area, std::function<void(std::unordered_set<uint32_t> ips)> completeHandler);

    void tcpTlsDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const unordered_set<string> &areas, std::function<void(std::unordered_set<uint32_t> ips)> completeHandler);

    void tcpDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const unordered_set<string> &areas, std::function<void(std::unordered_set<uint32_t> ips)> completeHandler);


    void tcpTlsDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(std::unordered_set<uint32_t> ips)> completeHandler);

    void tcpDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(std::unordered_set<uint32_t> ips)> completeHandler);


    void forwardUdp(UdpDnsRequest &udpDnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(UdpDNSResponse *)> callback);

    //    static TcpDNSResponse *EDNS(const std::string &domain, const std::string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout);


    DNSClient();

    virtual ~DNSClient();

private:
    io_context ioContext;
    boost::asio::io_context::work *ioWoker;
    std::thread *th;
    std::unordered_set<uint32_t> parse(uint16_t length, pair<uint8_t *, uint32_t> lengthBytes, pair<uint8_t *, uint32_t> dataBytes, uint16_t dnsId);

    template<typename Result>
    bool isTimeoutOrError(boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(Result)> completeHandler);
};

#endif//ST_DNS_DNSCLIENT_H