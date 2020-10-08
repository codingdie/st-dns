//
// Created by codingdie on 2020/5/19.
//

#include "DNSServer.h"
#include <string>
#include <boost/thread.hpp>
#include "IPUtils.h"
#include "DNSClient.h"
#include "STUtils.h"
#include "DNSCache.h"
#include "CountryIpManager.h"

using namespace std::placeholders;
using namespace std;


DNSServer::DNSServer(st::dns::Config &config) : config(config), pool(10) {
    socketS = new udp::socket(ioContext, udp::endpoint(boost::asio::ip::make_address_v4(config.ip), config.port));
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
    socketS->async_receive_from(buffer(session->udpDnsRequest.data, session->udpDnsRequest.len), session->clientEndpoint,
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
    if (ips.size() > 0) {
        UdpDNSResponse *udpResponse = new UdpDNSResponse(id, host, ips);
        socketS->async_send_to(buffer(udpResponse->data, udpResponse->len), clientEndpoint,
                               [udpResponse, session](boost::system::error_code writeError, size_t writeSize) {
                                   Logger::DEBUG << "dns success!" << session->udpDnsRequest.getFirstHost() << st::utils::ipv4::ipsToStr(
                                           udpResponse->ips) << END;
                                   delete udpResponse;
                                   delete session;
                               });
    } else {
        Logger::ERROR << "dns failed!" << session->udpDnsRequest.getFirstHost() << END;
    }

}

set<uint32_t> DNSServer::queryDNS(const string &host) const {
    auto ips = DNSCache::query(host);
    if (ips.empty()) {
        vector<RemoteDNSServer *> servers = RemoteDNSServer::calculateQueryServer(host, config.servers);
        for (auto it = servers.begin(); it != servers.end(); it++) {
            RemoteDNSServer *&server = *it;
            ips = queryDNS(host, server);
            if (!ips.empty()) {
                DNSCache::addCache(host, ips, server->id());
                break;
            }
        }

    }
    return move(ips);
}

set<uint32_t> DNSServer::queryDNS(const string &host, const RemoteDNSServer *server) const {
    set<uint32_t> ips;
    if (server->type.compare("TCP_SSL") == 0) {
        auto tcpResponse = DNSClient::tcpDns(host, server->ip, server->port, 5000);
        if (tcpResponse != nullptr && tcpResponse->isValid()) {
            ips = move(tcpResponse->udpDnsResponse->ips);
        }
        if (tcpResponse != nullptr) {
            delete tcpResponse;
        }
    } else if (server->type.compare("UDP") == 0) {
        auto udpDnsResponse = DNSClient::udpDns(host, server->ip, server->port, 5000);
        if (udpDnsResponse != nullptr && udpDnsResponse->isValid()) {
            ips = move(udpDnsResponse->ips);
        }
        if (udpDnsResponse != nullptr) {
            delete udpDnsResponse;
        }
    }
    if (!ips.empty() && server->whitelist.find(host) == server->whitelist.end() && !server->country.empty() && server->onlyCountryIp) {
        for (auto it = ips.begin(); it != ips.end();) {
            if (!CountryIpManager::isCountryIP(server->country, *it)) {
                it = ips.erase(it);
            } else {
                it++;
            }
        }
    }
    return move(ips);
}
