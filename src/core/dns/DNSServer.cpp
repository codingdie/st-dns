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

DNSServer::DNSServer(st::dns::Config &config) : config(config), num(0) {
    socketS = new udp::socket(ioContext, udp::endpoint(boost::asio::ip::make_address_v4(config.ip), config.port));
}


void DNSServer::start() {
    boost::asio::io_context::work ioContextWork(ioContext);
    vector<thread> threads;
    for (int i = 0; i < 8; i++) {
        threads.emplace_back([this]() {
            receive();
            ioContext.run();
        });
    }
    Logger::INFO << "st-dns start" << END;
    for (auto &th:threads) {
        th.join();
    }
    Logger::INFO << "st-dns end" << END;
}

void DNSServer::receive() {
    DNSSession *session = new DNSSession(num++);
    Logger::DEBUG << session->getId() << "begin receive dns request!" << END;

    socketS->async_receive_from(buffer(session->udpDnsRequest.data, session->udpDnsRequest.len), session->clientEndpoint,
                                [=, this](boost::system::error_code errorCode, std::size_t size) {
                                    Logger::DEBUG << session->getId() << "dns request received!" << END;
                                    if (!errorCode && size > 0) {
                                        session->udpDnsRequest.len = size;
                                        if (session->udpDnsRequest.parse()) {
                                            proxyDnsOverTcpTls(session);
                                        } else {
                                            delete session;
                                            Logger::ERROR << session->getId() << "invalid dns request" << END;
                                        }

                                    } else {
                                        delete session;
                                    }
                                    Logger::DEBUG << session->getId() << "finished!" << END;
                                    receive();
                                });

}

void DNSServer::proxyDnsOverTcpTls(DNSSession *session) {
    UdpDnsRequest &request = session->udpDnsRequest;
    udp::endpoint &clientEndpoint = session->clientEndpoint;
    auto id = request.dnsHeader->id;
    auto host = request.getFirstHost();
    set<uint32_t> ips = queryDNS(session);
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
        delete session;
    }

}

set<uint32_t> DNSServer::queryDNS(DNSSession *session) {
    auto host = session->udpDnsRequest.getFirstHost();
    set<uint32_t> ips;
    if (host == "localhost") {
        ips.emplace(2130706433);
    } else {
        DNSRecord record;
        auto expire = DNSCache::query(host, record);
        if (record.ips.empty() || expire) {
            rLock.lock();
            auto inQuerying = hostsInQuery.emplace(host).second == false;
            rLock.unlock();
            if (!inQuerying) {
                bool success = false;
                string logTag = "";
                if (record.ips.empty()) {
                    logTag = "query dns record" + host;
                    success = getDNSRecord(host, record);
                } else if (expire) {
                    RemoteDNSServer *server = config.getDNSServerById(record.dnsServer);
                    if (server != nullptr) {
                        success = getDNSRecord(host, record, server);
                    } else {
                        success = getDNSRecord(host, record);
                    }
                    logTag = "update dns record " + host + " from " + record.dnsServer;
                }
                if (!success) {
                    Logger::ERROR << session->getId() << logTag << "failed" << END;
                }
                rLock.lock();
                this->hostsInQuery.erase(host);
                rLock.unlock();
            }
        }
        ips = record.ips;
    }
    return move(ips);
}

bool DNSServer::getDNSRecord(const string &host, DNSRecord &record, const RemoteDNSServer *server) const {
    set<uint32_t> queryIps = queryDNS(host, server);
    if (!queryIps.empty()) {
        DNSCache::addCache(host, queryIps, server->id());
        DNSCache::query(host, record);
        return true;
    }
    return false;
}

bool DNSServer::getDNSRecord(const string &host, DNSRecord &record) {
    vector<RemoteDNSServer *> servers = RemoteDNSServer::calculateQueryServer(host, config.servers);
    for (auto it = servers.begin(); it != servers.end(); it++) {
        RemoteDNSServer *&server = *it;
        set<uint32_t> queryIps = queryDNS(host, server);
        if (!queryIps.empty()) {
            DNSCache::addCache(host, queryIps, server->id());
            DNSCache::query(host, record);
            return true;
        }
    }
    return false;
}

set<uint32_t> DNSServer::queryDNS(const string &host, const RemoteDNSServer *server) const {
    set<uint32_t> ips;
    if (server->type.compare("TCP_SSL") == 0) {
        auto tcpResponse = DNSClient::tcpDns(host, server->ip, server->port, 10000);
        if (tcpResponse != nullptr && tcpResponse->isValid()) {
            ips = move(tcpResponse->udpDnsResponse->ips);
        }
        if (tcpResponse != nullptr) {
            delete tcpResponse;
        }
    } else if (server->type.compare("UDP") == 0) {
        auto udpDnsResponse = DNSClient::udpDns(host, server->ip, server->port, 10000);
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
