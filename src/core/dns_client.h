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
#include <list>
#include <openssl/ssl.h>
using namespace boost::asio::ip;
using namespace boost::asio;
using namespace st::dns;

using namespace std;
#define dns_complete std::function<void(const std::vector<uint32_t> &ips)>
#define dns_multi_area_complete std::function<void(const std::vector<uint32_t> &ips, bool load_all)>
class dns_client {
public:
    void udp_dns(const string &domain, const std::string &dns_server, uint32_t port, uint64_t timeout, const dns_complete &complete_handler);

    void tcp_tls_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, pair<string, uint16_t> area, const dns_complete &complete_handler);

    void tcp_tls_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const vector<pair<string, uint16_t>> &areas, const dns_multi_area_complete &complete_handler);

    void tcp_tls_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const dns_complete &complete_handler);

    void tcp_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, pair<string, uint16_t> area, const dns_complete &complete_handler);

    void tcp_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const vector<pair<string, uint16_t>> &areas, const dns_multi_area_complete &complete_handler);

    void tcp_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const dns_complete &complete_handler);

    void forward_udp(protocol::udp_request &dns_request, const std::string &dns_server, uint32_t port, uint64_t timeout, const std::function<void(protocol::udp_response *)> &callback);

    dns_client();
    static dns_client &uniq();

    virtual ~dns_client();

private:
    io_context ic;
    boost::asio::ssl::context *ssl_ctx;
    boost::asio::io_context::work *iw;
    std::thread *th;
    //    std::unordered_map<string, boost::asio::ssl::context *> contexts;
    //    std::unordered_map<string, SSL_SESSION *> sessions;
    //    boost::asio::ssl::context &get_context(const string &server);
    //    void init_ssl_session(const string &server, boost::asio::ssl::stream<tcp::socket> *socket);
    //    bool save_ssl_session(const string &server, boost::asio::ssl::stream<tcp::socket> *socket);

    std::vector<uint32_t> parse(uint16_t length, pair<uint8_t *, uint32_t> length_bytea, pair<uint8_t *, uint32_t> data_bytes, uint16_t dnsId);
    template<typename Result>
    bool is_timeout_or_error(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(Result)> complete_handler, Result defaultV);
    bool is_timeout_or_error(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, const dns_complete &complete_handler);
    bool is_timeout_or_error(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, const std::function<void(protocol::udp_response *res)> &complete_handler);
};

#endif//ST_DNS_DNS_CLIENT_H