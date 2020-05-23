//
// Created by codingdie on 2020/5/20.
//

#ifndef ST_DNS_DNSCLIENT_H
#define ST_DNS_DNSCLIENT_H

#include <string>
#include <boost/asio/basic_datagram_socket.hpp>
#include "DNS.h"
#include <boost/asio.hpp>
#include "LogUtils.h"

using namespace boost::asio::ip;
using namespace boost::asio;
using namespace std;

class DNSClient {
public:
    static std::string udpDns(std::string &domain, std::string &dnsServer);

    static std::string query(const string &dnsServer, const string &domain);
};

#endif //ST_DNS_DNSCLIENT_H