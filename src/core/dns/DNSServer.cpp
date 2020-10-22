//
// Created by codingdie on 2020/5/19.
//

#include "DNSServer.h"
#include <string>
#include "IPUtils.h"
#include "DNSClient.h"
#include "STUtils.h"
#include "DNSCache.h"
#include "AreaIpManager.h"
#include <mutex>
#include <unordered_set>

static mutex rLock;
using namespace std::placeholders;
using namespace std;

DNSServer::DNSServer(st::dns::Config &config) : config(config) {
    socketS = new udp::socket(ioContext, udp::endpoint(boost::asio::ip::make_address_v4(config.ip), config.port));
}


void DNSServer::start() {
    boost::asio::io_context::work ioContextWork(ioContext);
    vector<thread> threads;
    for (int i = 0; i < 1; i++) {
        threads.emplace_back([this]() {
            receive();
            ioContext.run();
        });
    }
    for (auto &th:threads) {
        th.join();
    }
    Logger::INFO << "server end" << END;
}

void DNSServer::receive() {
    auto curNum = num++;
    Logger::DEBUG << curNum << "begin receive!" << END;

    DNSSession *session = new DNSSession();
    socketS->async_receive_from(buffer(session->udpDnsRequest.data, session->udpDnsRequest.len), session->clientEndpoint,
                                [=, this](boost::system::error_code errorCode, std::size_t size) {
                                    Logger::DEBUG << curNum << "received!" << END;

                                    if (!errorCode && size > 0) {
                                        session->udpDnsRequest.len = size;
                                        if (session->udpDnsRequest.parse()) {
                                            proxyDnsOverTcpTls(session);
                                        } else {
                                            delete session;
                                            Logger::ERROR << "invalid dns request" << END;
                                        }

                                    }
                                    Logger::DEBUG << curNum << "finished!" << END;
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
        if (*ips.begin() == 0) {
            Logger::ERROR << "dns failed!" << session->udpDnsRequest.getFirstHost() << END;
        }

    }

}

set<uint32_t> DNSServer::queryDNS(const string &host) {
    auto ips = DNSCache::query(host);

    if (ips.empty()) {
        if (host == "localhost") {
            ips.emplace(2130706433);
        } else {
            {
                rLock.lock();
                auto exits = hostsInQuery.find(host) == hostsInQuery.end();
                rLock.unlock();
                if (exits) {
                    vector<RemoteDNSServer *> servers = RemoteDNSServer::calculateQueryServer(host, config.servers);
                    for (auto it = servers.begin(); it != servers.end(); it++) {
                        RemoteDNSServer *&server = *it;
                        ips = queryDNS(host, server);
                        if (!ips.empty()) {
                            DNSCache::addCache(host, ips, server->id());
                            break;
                        }
                    }
                    rLock.lock();
                    this->hostsInQuery.erase(host);
                    rLock.unlock();
                }
            }
        }
    }
    if (ips.empty()) {
        ips.emplace(0);
        DNSCache::addCache(host, ips, "error", 1000 * 10);
    }
    return move(ips);
}

set<uint32_t> DNSServer::queryDNS(const string &host, const RemoteDNSServer *server) const {
    set<uint32_t> ips;
    if (server->type.compare("TCP_SSL") == 0) {
        auto tcpResponse = DNSClient::tcpDns(host, server->ip, server->port, 3000);
        if (tcpResponse != nullptr && tcpResponse->isValid()) {
            ips = move(tcpResponse->udpDnsResponse->ips);
        }
        if (tcpResponse != nullptr) {
            delete tcpResponse;
        }
    } else if (server->type.compare("UDP") == 0) {
        auto udpDnsResponse = DNSClient::udpDns(host, server->ip, server->port, 3000);
        if (udpDnsResponse != nullptr && udpDnsResponse->isValid()) {
            ips = move(udpDnsResponse->ips);
        }
        if (udpDnsResponse != nullptr) {
            delete udpDnsResponse;
        }
    }
    filterIPByArea(host, server, ips);

    return move(ips);
}

void DNSServer::filterIPByArea(const string &host, const RemoteDNSServer *server, set<uint32_t> &ips) const {
    set<string> &tunnelRealHosts = st::dns::Config::INSTANCE.stProxyTunnelRealHosts;

    if (!ips.empty() && tunnelRealHosts.find(host) == tunnelRealHosts.end() && server->whitelist.find(
            host) == server->whitelist.end() && !server->area.empty() && server->onlyAreaIp) {
        for (auto it = ips.begin(); it != ips.end();) {
            if (!AreaIpManager::isAreaIP(server->area, *it)) {
                it = ips.erase(it);
                Logger::DEBUG << host << "remove not area ip" << st::utils::ipv4::ipToStr(*it) << "from" << server->ip << server->area << END;
            } else {
                it++;
            }
        }
    }
}
