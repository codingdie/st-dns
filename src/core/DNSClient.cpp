//
// Created by codingdie on 2020/5/20.
//

#include "DNSClient.h"


#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include "DNSClient.h"
#include "DNSRequest.h"

using namespace boost::asio::ip;
using namespace boost::asio;
using namespace std;

std::string DNSClient::udpDns(std::string domain, std::string dnsServer) {
    try {
        boost::asio::io_context io_context;
        const udp &udp = udp::v4();
        udp::socket s(io_context, udp::endpoint(udp, 0));
        udp::endpoint addr(ip::make_address_v4(dnsServer), 53);
        DNSRequest dnsRequest(domain);
        size_t i = s.send_to(boost::asio::buffer(dnsRequest.data, dnsRequest.len), addr);

        char reply[1024];
        udp::endpoint sender_endpoint;
        size_t reply_length = s.receive_from(boost::asio::buffer(reply, 1024), sender_endpoint);
        std::cout << "Reply is: ";
        std::cout.write(reply, reply_length);
        std::cout << "\n";
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return "";

}
