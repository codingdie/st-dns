//
// Created by codingdie on 2020/5/21.
//

#include "TCPSession.h"

boost::asio::ip::tcp::socket *TCPSession::getSocket() const {
    return socket;
}

TCPSession::TCPSession(boost::asio::ip::tcp::socket *socket, int id) : socket(socket), id(id) {}

long TCPSession::getId() const {
    return id;
}

