//
// Created by codingdie on 2020/5/20.
//

#ifndef ST_DNS_DNSCLIENT_H
#define ST_DNS_DNSCLIENT_H

#include "protocol/protocol.h"
#include "utils/utils.h"
#include <boost/asio.hpp>
#include <boost/asio/basic_datagram_socket.hpp>
#include <boost/asio/ssl.hpp>
#include <string>
#include <vector>
#include <thread>

using namespace boost::asio::ip;
using namespace boost::asio;
using namespace st::dns;

using namespace std;

class DNSClient {
public:
    static DNSClient INSTANCE;

    void udpDns(const string domain, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(std::vector<uint32_t> ips)> completeHandler);

    void tcpTlsDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const string &area, std::function<void(std::vector<uint32_t> ips)> completeHandler);

    void tcpDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const string &area, std::function<void(std::vector<uint32_t> ips)> completeHandler);

    void tcpTlsDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const unordered_set<string> &areas, std::function<void(std::vector<uint32_t> ips, bool loadAll)> completeHandler);

    void tcpDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const unordered_set<string> &areas, std::function<void(std::vector<uint32_t> ips, bool loadAll)> completeHandler);


    void tcpTlsDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(std::vector<uint32_t> ips)> completeHandler);

    void tcpDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(std::vector<uint32_t> ips)> completeHandler);


    void forwardUdp(udp_request &udpDnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(udp_response *)> callback);

    //    static tcp_response *EDNS(const std::string &domain, const std::string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout);


    DNSClient();

    virtual ~DNSClient();

private:
    io_context ioContext;
    boost::asio::io_context::work *ioWoker;
    std::thread *th;
    std::vector<uint32_t> parse(uint16_t length, pair<uint8_t *, uint32_t> lengthBytes, pair<uint8_t *, uint32_t> dataBytes, uint16_t dnsId);
    template<typename Result>
    bool isTimeoutOrError(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(Result)> completeHandler, Result defaultV);
    bool isTimeoutOrError(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(std::vector<uint32_t>)> completeHandler);
    bool isTimeoutOrError(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(udp_response *res)> completeHandler);

};

#endif//ST_DNS_DNSCLIENT_H