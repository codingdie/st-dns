//
// Created by codingdie on 2020/5/19.
//

#include "DNSServer.h"

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "AreaIpManager.h"
#include "DNSCache.h"
#include "DNSClient.h"
#include "IPUtils.h"
#include "STUtils.h"

static mutex rLock;
static unordered_set<string> hostsInQuery;

using namespace std::placeholders;
using namespace std;

DNSServer::DNSServer(st::dns::Config &config) : rid(1), config(config) {
    try {
        socketS = new udp::socket(ioContext, udp::endpoint(boost::asio::ip::make_address_v4(config.ip), config.port));
        for (auto it = config.servers.begin(); it != config.servers.end(); it++) {
            auto server = *it;
            workThreads.emplace(make_pair(server->id(), new thread_pool(server->parallel)));
        }
    } catch (const boost::system::system_error &e) {
        Logger::ERROR << "bind address error" << config.ip << config.port << e.what() << END;
        exit(1);
    }
}

void DNSServer::start() {
    boost::asio::io_context::work ioContextWork(ioContext);
    vector<thread> threads;
    DNSCache::INSTANCE.loadFromFile();
    Logger::INFO << "st-dns start, listen at" << config.ip << config.port << END;
    for (int i = 0; i < config.parallel; i++) {
        threads.emplace_back([this]() {
            receive();
            ioContext.run();
        });
    }
    for (auto &th : threads) {
        th.join();
    }
    Logger::INFO << "st-dns end" << END;
}

void DNSServer::receive() {
    DNSSession *session = new DNSSession(rid.fetch_add(1));
    socketS->async_receive_from(buffer(session->udpDnsRequest.data, session->udpDnsRequest.len),
                                session->clientEndpoint,
                                [=](boost::system::error_code errorCode, std::size_t size) {
                                    Logger::traceId = session->getId();
                                    Logger::DEBUG << "dns request received!" << END;
                                    session->setTime(time::now());
                                    if (!errorCode && size > 0) {
                                        session->udpDnsRequest.len = size;
                                        if (session->udpDnsRequest.parse()) {
                                            processSession(session);
                                        } else {
                                            Logger::ERROR << "invalid dns request" << END;
                                            endDNSSession(session);
                                        }
                                    } else {
                                        endDNSSession(session);
                                    }
                                    receive();
                                });
}

void DNSServer::endDNSSession(const DNSSession *session) const {
    bool success = session->udpDNSResponse != nullptr;
    if (!session->forward) {
        success &= !session->record.ips.empty();
    }
    Logger::INFO << "dns from" << session->clientEndpoint.address().to_string() << session->clientEndpoint.port() << (session->forward ? "forward" : "query") << (success ? "success!" : "failed!");
    Logger::INFO << "type:" << session->getQueryType();
    Logger::INFO << "rlen:" << session->udpDnsRequest.len;
    if (session->udpDNSResponse != nullptr) {
        Logger::INFO << "relen:" << session->udpDNSResponse->len;
    }
    if (!session->udpDnsRequest.getHost().empty()) {
        Logger::INFO << session->udpDnsRequest.getHost();
    }
    if (!session->record.ips.empty()) {
        Logger::INFO << st::utils::ipv4::ipsToStr(session->record.ips) << session->record.dnsServer;
    }
    Logger::INFO << "cost:" << time::now() - session->getTime() << END;
    // if (session->forward) {
    //     session->udpDNSResponse->printHex();
    // }
    delete session;
}
void DNSServer::queryDNSRecord(DNSSession *session, std::function<void(DNSSession *)> finalComplete) {
    auto complete = [=](DNSSession *se) {
        auto id = session->udpDnsRequest.dnsHeader->id;
        session->udpDNSResponse = new UdpDNSResponse(id, session->record);
        finalComplete(session);
    };
    auto host = session->getHost();
    DNSRecord &record = session->record;

    if (host == "localhost") {
        record.dnsServer = "st-dns";
        record.host = host;
        record.expireTime = std::numeric_limits<uint64_t>::max();
        complete(session);
    } else {
        DNSCache::INSTANCE.query(host, record);
        if (record.ips.empty()) {
            queryDNSRecordFromServer(session, complete);
        } else {
            if (record.expire || !record.matchArea) {
                updateDNSRecord(record);
            }
            complete(session);
        }
    }
}

void DNSServer::processSession(DNSSession *session) {
    session->record.host = session->getHost();

    auto complete = [=](DNSSession *session) {
        udp::endpoint &clientEndpoint = session->clientEndpoint;
        UdpDNSResponse *udpResponse = session->udpDNSResponse;
        if (udpResponse != nullptr) {
            socketS->async_send_to(buffer(udpResponse->data, udpResponse->len), clientEndpoint,
                                   [=](boost::system::error_code writeError, size_t writeSize) {
                                       Logger::traceId = session->getId();
                                       endDNSSession(session);
                                   });
        } else {
            endDNSSession(session);
        }
    };
    if (session->getQueryType() == DNSQuery::A || session->getQueryType() == DNSQuery::CNAME) {
        queryDNSRecord(session, complete);
    } else {
        session->forward = true;
        forwardUdpDNSRequest(session, complete);
    }
}


void DNSServer::queryDNSRecordFromServer(DNSSession *session, std::function<void(DNSSession *session)> completeHandler) {
    auto record = session->record;
    auto host = record.host;
    vector<RemoteDNSServer *> &servers = session->servers;
    calRemoteDNSServers(record, servers);
    if (beginQuery(host)) {
        syncDNSRecordFromServer(
                host, [=](DNSRecord &record) {
                    endQuery(host);
                    session->record = move(record);
                    completeHandler(session);
                },
                servers, 0);
    } else {
        completeHandler(session);
    }
}
void DNSServer::forwardUdpDNSRequest(DNSSession *session, std::function<void(DNSSession *session)> completeHandler) {
    vector<RemoteDNSServer *> &servers = session->servers;
    for (auto &it : config.servers) {
        if (it->type == "UDP") {
            servers.emplace_back(it);
        }
    }
    if (!servers.empty()) {
        forwardUdpDNSRequest(
                session, [=](UdpDNSResponse *res) {
                    session->udpDNSResponse = res;
                    completeHandler(session);
                },
                servers, 0);
    } else {
        completeHandler(session);
    }
}
void DNSServer::forwardUdpDNSRequest(DNSSession *session, std::function<void(UdpDNSResponse *)> complete, vector<RemoteDNSServer *> servers, int pos) {
    if (pos >= servers.size()) {
        DNSRecord record;
        complete(nullptr);
        return;
    }
    RemoteDNSServer *server = servers[pos];
    auto thPool = workThreads.find(server->id());
    auto traceId = Logger::traceId;
    if (thPool != workThreads.end()) {
        thread_pool *ctx = thPool->second;
        thread_pool &pool = *ctx;
        auto task = [=]() {
            Logger::traceId = traceId;
            UdpDNSResponse *response = DNSClient::forwardUdpDNS(session->udpDnsRequest, server->ip, server->port, server->timeout);
            if (response == nullptr && pos + 1 < servers.size()) {
                forwardUdpDNSRequest(session, complete, servers, pos + 1);
            } else {
                complete(response);
            }
        };
        boost::asio::post(pool, task);
    }
}
void DNSServer::calRemoteDNSServers(const DNSRecord &record, vector<RemoteDNSServer *> &servers) {
    auto host = record.host;
    if (record.ips.empty()) {
        servers = RemoteDNSServer::calculateQueryServer(host, config.servers);
    } else if (!record.matchArea) {
        unordered_set<string> notMatchAreaServers = DNSCache::INSTANCE.queryNotMatchAreaServers(host);
        auto notMatchServers = notMatchAreaServers;
        for (auto it = config.servers.begin(); it != config.servers.end(); it++) {
            RemoteDNSServer *server = *it.base();
            if (notMatchServers.find(server->id()) == notMatchServers.end()) {
                servers.emplace_back(server);
            }
        }
    } else {
        RemoteDNSServer *server = this->config.getDNSServerById(record.dnsServer);
        if (server != nullptr) {
            servers.emplace_back(server);
        } else {
            servers = RemoteDNSServer::calculateQueryServer(host, config.servers);
        }
    }
}
bool DNSServer::beginQuery(const string host) {
    bool result = false;
    rLock.lock();
    if (hostsInQuery.find(host) == hostsInQuery.end()) {
        hostsInQuery.emplace(host);
        result = true;
    }
    rLock.unlock();
    return result;
}
void DNSServer::endQuery(const string host) {
    rLock.lock();
    hostsInQuery.erase(host);
    rLock.unlock();
}
void DNSServer::updateDNSRecord(DNSRecord record) {
    auto host = record.host;
    vector<RemoteDNSServer *> servers;
    calRemoteDNSServers(record, servers);
    if (beginQuery(host)) {
        if (!servers.empty()) {
            syncDNSRecordFromServer(
                    host, [=](DNSRecord &record) {
                        endQuery(host);
                        Logger::DEBUG << "updateDNSRecord finished!" << END;
                    },
                    servers, 0);
        } else {
            Logger::ERROR << "updateDNSRecord servers empty!" << END;
            endQuery(host);
        }
    } else {
        Logger::TRACE << "updateDNSRecord inQuerying" << host << END;
    }
}

void DNSServer::syncDNSRecordFromServer(const string host, std::function<void(DNSRecord &record)> complete,
                                        vector<RemoteDNSServer *> servers,
                                        int pos) {
    if (pos >= servers.size()) {
        Logger::ERROR << "not known host" << host << END;
        DNSRecord record;
        DNSCache::INSTANCE.query(host, record);
        complete(record);
        return;
    }
    RemoteDNSServer *server = servers[pos];
    auto thPool = workThreads.find(server->id());
    auto traceId = Logger::traceId;
    if (thPool != workThreads.end()) {
        thread_pool *ctx = thPool->second;
        thread_pool &pool = *ctx;
        auto task = [=]() {
            Logger::traceId = traceId;
            Logger::DEBUG << host << "query dns from" << server->id() << "begin" << END;
            unordered_set<uint32_t> ips;
            if (server->type.compare("TCP_SSL") == 0) {
                auto tcpResponse = DNSClient::tcpTlsDns(host, server->ip, server->port, server->timeout);
                if (tcpResponse != nullptr && tcpResponse->isValid()) {
                    ips = move(tcpResponse->udpDnsResponse->ips);
                }
                if (tcpResponse != nullptr) {
                    delete tcpResponse;
                }
            } else if (server->type.compare("TCP") == 0) {
                auto tcpResponse = DNSClient::tcpDns(host, server->ip, server->port, server->timeout);
                if (tcpResponse != nullptr && tcpResponse->isValid()) {
                    ips = move(tcpResponse->udpDnsResponse->ips);
                }
                if (tcpResponse != nullptr) {
                    delete tcpResponse;
                }
            } else if (server->type.compare("UDP") == 0) {
                auto udpDnsResponse = DNSClient::udpDns(host, server->ip, server->port, server->timeout);
                if (udpDnsResponse != nullptr && udpDnsResponse->isValid()) {
                    ips = move(udpDnsResponse->ips);
                }
                if (udpDnsResponse != nullptr) {
                    delete udpDnsResponse;
                }
            }
            unordered_set<uint32_t> oriIps = ips;
            filterIPByArea(host, server, ips);
            if (!ips.empty()) {
                DNSCache::INSTANCE.addCache(host, ips, server->id(),
                                            server->dnsCacheExpire, true);
            } else {
                if (!oriIps.empty()) {
                    DNSCache::INSTANCE.addCache(host, oriIps, server->id(),
                                                server->dnsCacheExpire, false);
                }
            }
            Logger::DEBUG << host << "query dns from" << server->id() << (ips.empty() ? "invalid" : "success") << END;
            if (ips.empty() && pos + 1 < servers.size()) {
                syncDNSRecordFromServer(host, complete, servers, pos + 1);
            } else {
                DNSRecord record;
                DNSCache::INSTANCE.query(host, record);
                complete(record);
            }
        };
        boost::asio::post(pool, task);
    }
}

void DNSServer::filterIPByArea(const string host, RemoteDNSServer *server, unordered_set<uint32_t> &ips) {
    if (!ips.empty() && server->whitelist.find(host) == server->whitelist.end() && !server->area.empty() &&
        server->onlyAreaIp) {
        for (auto it = ips.begin(); it != ips.end();) {
            auto ip = *it;
            if (!AreaIpManager::isAreaIP(server->area, ip)) {
                it = ips.erase(it);
                Logger::DEBUG << host << "remove not area ip" << st::utils::ipv4::ipToStr(ip) << "from" << server->ip
                              << server->area << END;
            } else {
                it++;
            }
        }
    }
}
