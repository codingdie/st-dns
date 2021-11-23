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

    template<typename Result>
    bool isTimeoutOrError(boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(Result *)> completeHandler);

    void udpDns(const string domain, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(UdpDNSResponse *)> completeHandler);

    void tcpTlsDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(TcpDNSResponse *)> completeHandler);

    void tcpDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(TcpDNSResponse *)> completeHandler);

    void forwardUdp(UdpDnsRequest &udpDnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(UdpDNSResponse *)> callback);

    //    static TcpDNSResponse *EDNS(const std::string &domain, const std::string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout);



    DNSClient();

    virtual ~DNSClient();

private:
    io_context ioContext;
    boost::asio::io_context::work *ioWoker;
    std::thread *th;

    //    TcpDNSResponse *queryEDNS(const vector<string> &domains, const string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout);
};

#endif//ST_DNS_DNSCLIENT_H