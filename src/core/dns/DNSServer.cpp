//
// Created by codingdie on 2020/5/19.
//

#include "DNSServer.h"
#include <string>
#include <boost/thread.hpp>
#include "DNS.h"
#include "DNSClient.h"
#include "STUtils.h"
#include "DNSCache.h"

using namespace std::placeholders;
using namespace std;


DNSServer::DNSServer(st::dns::Config &config) : config(config), pool(10) {
    socketS = new udp::socket(ioContext, udp::endpoint(udp::v4(), 53));
}


void DNSServer::start() {
    Logger::INFO << "server stared" << END;
    receive();
    ioContext.run();

}

void DNSServer::receive() {
    socketS->async_receive_from(buffer(bufferData, 1024), clientEndpoint,
                                [&](boost::system::error_code errorCode, std::size_t size) {
                                    Logger::INFO << "receive" << i++ << END;
                                    if (!errorCode && size > 0) {
                                        boost::asio::post(pool, [&] {
                                            UdpDnsRequest *udpDnsRequest = new UdpDnsRequest(size);
                                            st::utils::copy(bufferData, udpDnsRequest->data, size);
                                            if (udpDnsRequest->parse()) {
                                                proxyDnsOverTcpTls(udpDnsRequest);
                                            } else {
                                                delete udpDnsRequest;
                                                Logger::ERROR << "invalid dns request" << END;
                                            }

                                        });
                                    }
                                    receive();
                                });

}

void DNSServer::proxyDnsOverTcpTls(UdpDnsRequest *udpDnsRequest) {

    auto ips = DNSCache::query(udpDnsRequest->getFirstHost());
    if (ips.empty()) {
        auto tcpResponse = DNSClient::tcpDns(udpDnsRequest->getFirstHost(), config.dnsServer, 3000);
        if (tcpResponse != nullptr && tcpResponse->isValid()) {
            ips = tcpResponse->udpDnsResponse->ips;
        }
        delete tcpResponse;
    }
    udpDnsRequest.h
    delete udpDnsRequest;
    UdpDNSResponse *udpDnsResponse = new UdpDNSResponse()
    socketS->async_send_to(buffer(udpResponse->data, udpResponse->len), clientEndpoint,
                           [&](boost::system::error_code writeError, size_t writeSize) {
                               Logger::INFO << "proxy success!"
                                            << st::utils::join(udpDnsRequest->dnsQueryZone->hosts, ",")
                                            << ipsToStr(udpResponse->ips) << END;
                               delete tcpResponse;
                               delete udpDnsRequest;
                           });


}
