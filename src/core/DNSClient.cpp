//
// Created by codingdie on 2020/5/20.
//

#include "DNSClient.h"


#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include "DNSClient.h"

using boost::asio::ip::tcp;
using namespace boost::asio;

using namespace std;

std::string DNSClient::dns(std::string host, std::string dnsServer) {
    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        tcp::socket socket(io_context);
        tcp::endpoint addr(ip::make_address_v4("127.0.0.1"), 2020);
        boost::asio::connect(socket, addr);
        for (;;) {
            boost::array<char, 128> buf;
            boost::system::error_code error;

            size_t len = socket.read_some(boost::asio::buffer(buf), error);

            if (error == boost::asio::error::eof)
                break; // Connection closed cleanly by peer.
            else if (error)
                throw boost::system::system_error(error); // Some other error.

            std::cout.write(buf.data(), len);
        }
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}
