//
// Created by codingdie on 2020/5/21.
//

#ifndef ST_DNS_DNSOVERTCPPROXY_H
#define ST_DNS_DNSOVERTCPPROXY_H

#include <boost/asio.hpp>

class DNSOverTcpProxy {

public:
    static void proxy(boost::asio::ip::tcp::socket *socket);
};


#endif //ST_DNS_DNSOVERTCPPROXY_H
