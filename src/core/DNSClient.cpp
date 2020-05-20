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
        tcp::socket socket(io_context);
        tcp::endpoint addr(ip::make_address_v4(dnsServer), 53);
        boost::asio::connect(socket, &addr);
        socket.async_connect(addr, [](boost::system::error_code error) -> void {
            std::cout << error.message() << endl;
        });
        std::cout << "2" << endl;
        return "123";
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}
