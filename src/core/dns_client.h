//
// Created by codingdie on 2020/5/20.
//

#ifndef ST_DNS_DNS_CLIENT_H
#define ST_DNS_DNS_CLIENT_H

#include "protocol/protocol.h"
#include "st.h"
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

class dns_client {
public:
    static dns_client INSTANCE;

    void udp_dns(const string &domain, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(std::vector<uint32_t> ips)> complete_handler);

    void tcp_tls_dns(const string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const string &area, std::function<void(std::vector<uint32_t> ips)> complete_handler);

    void tcp_tls_dns(const string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const unordered_set<string> &areas, std::function<void(std::vector<uint32_t> ips, bool loadAll)> complete_handler);

    void tcp_tls_dns(const string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(std::vector<uint32_t> ips)> complete_handler);

    void tcp_dns(const string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const string &area, std::function<void(std::vector<uint32_t> ips)> complete_handler);

    void tcp_dns(const string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const unordered_set<string> &areas, std::function<void(std::vector<uint32_t> ips, bool loadAll)> complete_handler);

    void tcp_dns(const string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(std::vector<uint32_t> ips)> complete_handler);

    void forward_udp(protocol::udp_request &udpDnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(protocol::udp_response *)> callback);

    //    static tcp_response *EDNS(const std::string &domain, const std::string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout);

    dns_client();

    virtual ~dns_client();

private:
    io_context ioContext;
    boost::asio::io_context::work *ioWoker;
    std::thread *th;
    std::vector<uint32_t> parse(uint16_t length, pair<uint8_t *, uint32_t> lengthBytes, pair<uint8_t *, uint32_t> dataBytes, uint16_t dnsId);
    template<typename Result>
    bool is_timeout_or_error(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(Result)> complete_handler, Result defaultV);
    bool is_timeout_or_error(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(std::vector<uint32_t>)> complete_handler);
    bool is_timeout_or_error(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(protocol::udp_response *res)> complete_handler);
};

#endif//ST_DNS_DNS_CLIENT_H