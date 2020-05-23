//
// Created by 徐芃 on 2020/5/19.
//

#include "DNSServer.h"
#include <iostream>
#include <ctime>
#include <string>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "TCPSession.h"
#include "LogUtils.h"

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std::placeholders;
using namespace std;

std::string make_daytime_string() {
    using namespace std; // For time_t, time and ctime;
    time_t now = time(0);
    return ctime(&now);
}

DNSServer::DNSServer(const Config &config) : config(config) {}


void DNSServer::start() {
    try {
        io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 53));
        thread_pool pool(3);
        long id = 1;
        while (true) {
            tcp::socket *socket = new tcp::socket(io_context);
            acceptor.accept(*socket);
            TCPSession *tcpSession = new TCPSession(socket, id++);
            this->tcpSessions.insert(pair<long, TCPSession *>(tcpSession->getId(), tcpSession));
            boost::asio::post(pool, [tcpSession] {
                tcp::socket *socket = tcpSession->getSocket();
                boost::system::error_code error;
                while (socket->is_open()) {
                    std::string message = make_daytime_string();
                    boost::asio::write(*socket, boost::asio::buffer(message), error);
                    if (error.failed()) {
                        auto info = to_string(tcpSession->getId()) + " disconnect ";
                        LogUtils::error(info);
                        break;
                    }
                    sleep(1);
                }
            });


        }
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }


}
