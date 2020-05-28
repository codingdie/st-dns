//
// Created by codingdie on 2020/5/28.
//

#include "DNSSession.h"

DNSSession::~DNSSession() {
    if (clientEndpoint != nullptr) {
        delete clientEndpoint;
    }
    if (udpDnsRequest != nullptr) {
        delete udpDnsRequest;
    }
}

DNSSession::DNSSession(UdpDnsRequest *udpDnsRequest, boost::asio::ip::udp::endpoint clientEndpoint) : udpDnsRequest(
        udpDnsRequest) {
    this->clientEndpoint = new boost::asio::ip::udp::endpoint(clientEndpoint);

}

