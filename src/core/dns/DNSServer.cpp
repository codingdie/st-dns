//
// Created by codingdie on 2020/5/19.
//

#include "DNSServer.h"
#include <iostream>
#include <ctime>
#include <string>
#include <boost/thread.hpp>
#include "Logger.h"
#include "DNS.h"
#include "DNSClient.h"

using namespace std::placeholders;
using namespace std;

std::string make_daytime_string() {
    using namespace std; // For time_t, time and ctime;
    time_t now = time(0);
    return ctime(&now);
}

DNSServer::DNSServer(st::dns::Config &config) : config(config), pool(10) {
    socketS = new udp::socket(ioContext, udp::endpoint(udp::v4(), 53));
}


void DNSServer::start() {
    Logger::INFO << "server stared" << END;
    receive();
    ioContext.run();

}

void DNSServer::receive() {
    Logger::INFO << "start receive" << ++i << END;

    socketS->async_receive_from(buffer(bufferData, 1024), clientEndpoint,
                                [&](boost::system::error_code errorCode, std::size_t size) {
                                    Logger::INFO << "receive" << i << END;
                                    if (!errorCode && size > 0) {
                                        boost::asio::post(pool, [&] {
                                            UdpDnsRequest *udpDnsRequest = new UdpDnsRequest(size);
                                            st::utils::copy(bufferData, udpDnsRequest->data, size);
                                            if (udpDnsRequest->parse()) {
                                                this->sessions.insert(make_pair(udpDnsRequest->dnsHeader->id,
                                                                                new DNSSession(udpDnsRequest,
                                                                                               clientEndpoint)));
                                                auto tcpResponse = DNSClient::tcpDns(udpDnsRequest->hosts,
                                                                                     config.dnsServer);
                                                if (tcpResponse != nullptr && tcpResponse->isValid()) {
                                                    auto udpResponse = tcpResponse->udpDnsResponse;
                                                    udpResponse->header->updateId(udpDnsRequest->dnsHeader->id);
                                                    socketS->async_send_to(buffer(udpResponse->data, udpResponse->len),
                                                                           clientEndpoint,
                                                                           [&](boost::system::error_code writeError,
                                                                               std::size_t writeSize) {
                                                                               delete tcpResponse;
                                                                               delete udpDnsRequest;
                                                                           });
                                                } else {
                                                    delete tcpResponse;
                                                    delete udpDnsRequest;
                                                }
                                            } else {
                                                delete udpDnsRequest;
                                                Logger::ERROR << "invalid dns request" << END;
                                            }

                                        });
                                    }
                                    receive();
                                });

}
