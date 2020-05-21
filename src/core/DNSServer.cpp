//
// Created by 徐芃 on 2020/5/19.
//

#include "DNSServer.h"
#include <iostream>
#include <ctime>
#include <string>
#include <boost/asio.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace std::placeholders;

std::string make_daytime_string() {
    using namespace std; // For time_t, time and ctime;
    time_t now = time(0);
    return ctime(&now);
}

DNSServer::DNSServer(const Config &config) : config(config) {}


void DNSServer::start() {
    try {
        io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 13));

        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            std::string message = make_daytime_string();
            boost::system::error_code ignored_error;
            boost::asio::write(socket, boost::asio::buffer(message), ignored_error);
        }
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }


}
