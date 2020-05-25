//
// Created by codingdie on 2020/5/21.
//

#ifndef ST_DNS_TCPSESSION_H
#define ST_DNS_TCPSESSION_H

#include <boost/asio.hpp>

class TCPSession {
public:
    TCPSession(boost::asio::ip::tcp::socket *socket, int id);

    boost::asio::ip::tcp::socket *getSocket() const;

private:
    boost::asio::ip::tcp::socket *socket;
    long id;
public:
    long getId() const;
};


#endif //ST_DNS_TCPSESSION_H
