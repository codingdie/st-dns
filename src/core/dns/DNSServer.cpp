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
    boost::asio::io_context::work ioContextWork(ioContext);
    thread ioThread([this]() {
        receive();
        ioContext.run();
    });
    ioThread.join();
    Logger::INFO << "server end" << END;
}

void DNSServer::receive() {
    DNSSession *session = new DNSSession();
    socketS->async_receive_from(buffer(session->udpDnsRequest.data, session->udpDnsRequest.len),
                                session->clientEndpoint,
                                [=, this](boost::system::error_code errorCode, std::size_t size) {
                                    if (!errorCode && size > 0) {
                                        boost::asio::post(pool, [=, this] {
                                            session->udpDnsRequest.len = size;
                                            if (session->udpDnsRequest.parse()) {
                                                proxyDnsOverTcpTls(session);
                                            } else {
                                                delete session;
                                                Logger::ERROR << "invalid dns request" << END;
                                            }

                                        });
                                    }
                                    receive();
                                });

}

void DNSServer::proxyDnsOverTcpTls(DNSSession *session) {
    UdpDnsRequest &request = session->udpDnsRequest;
    udp::endpoint &clientEndpoint = session->clientEndpoint;
    auto id = request.dnsHeader->id;
    auto host = request.getFirstHost();
    set<uint32_t> ips = queryDNS(host);
    UdpDNSResponse *udpResponse = new UdpDNSResponse(id, host, ips);
    socketS->async_send_to(buffer(udpResponse->data, udpResponse->len), clientEndpoint,
                           [udpResponse, session](boost::system::error_code writeError, size_t writeSize) {
                               Logger::INFO << "proxy success!" << session->clientEndpoint.address().to_string()
                                            << session->udpDnsRequest.getFirstHost()
                                            << session->udpDnsRequest.getFirstHost() << ipsToStr(udpResponse->ips)
                                            << END;
                               delete udpResponse;
                               delete session;
                           });
}

set<uint32_t> DNSServer::queryDNS(const string &host) const {
    auto ips = DNSCache::query(host);
    if (ips.empty()) {
        auto tcpResponse = DNSClient::tcpDns(host, config.dnsServer, 3000);
        if (tcpResponse != nullptr && tcpResponse->isValid()) {
            ips = tcpResponse->udpDnsResponse->ips;
            DNSCache::addCache(tcpResponse->udpDnsResponse);
        }
        if (tcpResponse != nullptr) {
            delete tcpResponse;
        }
    }
    return ips;
}
