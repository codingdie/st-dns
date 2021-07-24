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

using namespace std::placeholders;
using namespace std;

DNSServer::DNSServer(st::dns::Config &config) : rid(time::now()), config(config), counter(0) {
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
    ioWoker = new boost::asio::io_context::work(ioContext);
    vector<thread> threads;
    DNSCache::INSTANCE.loadFromFile();
    Logger::INFO << "st-dns start, listen at" << config.ip << config.port << END;
    for (int i = 0; i < config.parallel; i++) {
        threads.emplace_back([this]() {
            receive();
            ioContext.run();
        });
    }
    state = 1;
    for (auto &th : threads) {
        th.join();
    }
    Logger::INFO << "st-dns end" << END;
}

void DNSServer::shutdown() {
    this->state = 2;
    ioContext.stop();
    delete ioWoker;
    Logger::INFO << "st-dns server stoped, listen at" << config.ip + ":" + to_string(config.port) << END;
}

void DNSServer::waitStart() {
    while (state == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}
void DNSServer::receive() {
    uint64_t id = rid.fetch_add(1);
    Logger::traceId = id;
    DNSSession *session = new DNSSession(id);
    socketS->async_receive_from(buffer(session->udpDnsRequest.data, session->udpDnsRequest.len),
                                session->clientEndpoint,
                                [=](boost::system::error_code errorCode, std::size_t size) {
                                    Logger::traceId = session->getId();
                                    Logger::DEBUG << "dns request" << ++counter << "received!" << END;
                                    session->setTime(time::now());
                                    session->stepLogger.start();
                                    if (!errorCode && size > 0) {
                                        bool parsed = session->udpDnsRequest.parse(size);
                                        session->stepLogger.step("parseRequest");
                                        if (parsed) {
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


void DNSServer::processSession(DNSSession *session) {
    auto complete = [=](DNSSession *se) {
        se->stepLogger.step("buildResponse", se->record.toPT());
        udp::endpoint &clientEndpoint = se->clientEndpoint;
        UdpDNSResponse *udpResponse = se->udpDNSResponse;
        if (udpResponse == nullptr) {
            udpResponse = new UdpDNSResponse(se->udpDnsRequest, se->record);
            se->udpDNSResponse = udpResponse;
        }

        socketS->async_send_to(buffer(udpResponse->data, udpResponse->len), clientEndpoint,
                               [=](boost::system::error_code writeError, size_t writeSize) {
                                   Logger::traceId = se->getId();
                                   se->stepLogger.step("response");
                                   endDNSSession(se);
                               });
    };
    session->record.host = session->getHost();
    session->stepLogger.addDimension("queryType", session->getQueryTypeValue());
    session->stepLogger.addDimension("domain", session->getHost());
    if (session->getQueryType() == DNSQuery::A) {
        session->stepLogger.addDimension("processMethod", "resolve");
        queryDNSRecord(session, complete);
    } else if (session->getQueryType() != DNSQuery::AAAA) {
        session->processType = DNSSession::ProcessType::FORWARD;
        session->stepLogger.addDimension("processMethod", "forward");
        forwardUdpDNSRequest(session, complete);
    } else {
        session->processType = DNSSession::ProcessType::DROP;
        session->stepLogger.addDimension("processMethod", "drop");
        complete(session);
    }
}

void DNSServer::endDNSSession(DNSSession *session) {
    bool success = session->udpDNSResponse != nullptr;
    if (session->processType == DNSSession::ProcessType::QUERY) {
        success &= !session->record.ips.empty();
    }
    session->stepLogger.addMetric("cacheTotalCount", DNSCache::INSTANCE.getTrustedCount());
    session->stepLogger.addMetric("inQueryingHostCount", watingSessions.size());
    session->stepLogger.addDimension("success", success);
    session->stepLogger.end();

    Logger::INFO << "dns from" << session->clientEndpoint.address().to_string() << session->clientEndpoint.port() << session->processType << (success ? "success!" : "failed!");
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
        auto id = se->udpDnsRequest.dnsHeader->id;
        se->udpDNSResponse = new UdpDNSResponse(se->udpDnsRequest, se->record);
        finalComplete(se);
    };
    auto host = session->getHost();
    DNSRecord &record = session->record;
    if (host == "localhost") {
        record.dnsServer = "st-dns";
        record.host = host;
        record.expireTime = std::numeric_limits<uint64_t>::max();
        session->stepLogger.addDimension("dnsRecordSource", "cache");
        complete(session);
    } else {
        DNSCache::INSTANCE.query(host, record);
        if (record.ips.empty()) {
            string fiDomain = DNSDomain::getFIDomain(host);
            if (fiDomain == "LAN") {
                DNSCache::INSTANCE.query(DNSDomain::removeFIDomain(host), record);
                record.host = host;
            }
        }
        if (record.ips.empty()) {
            session->stepLogger.addDimension("dnsRecordSource", "server");
            queryDNSRecordFromServer(session, complete);
        } else {
            session->stepLogger.addDimension("dnsRecordSource", "cache");
            if (record.expire || !record.matchArea) {
                updateDNSRecord(record);
            }
            complete(session);
        }
    }
}

void DNSServer::queryDNSRecordFromServer(DNSSession *session, std::function<void(DNSSession *session)> completeHandler) {
    auto record = session->record;
    auto host = record.host;
    vector<RemoteDNSServer *> &servers = session->servers;
    calRemoteDNSServers(record, servers);
    if (servers.empty()) {
        Logger::ERROR << host << "queryDNSRecordFromServer calRemoteDNSServers empty!" << END;
        completeHandler(session);
        return;
    }
    if (beginQueryRemote(host, session)) {
        syncDNSRecordFromServer(
                host, [=](DNSRecord &record) {
                    auto sessions = endQueryRemote(host);
                    for (auto it = sessions.begin(); it != sessions.end(); it++) {
                        DNSSession *se = *it;
                        se->record = record;
                        completeHandler(se);
                    }
                },
                servers, 0, DNSCache::INSTANCE.hasAnyRecord(host));
    } else {
        Logger::DEBUG << host << "is in query!" << END;
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
        complete(nullptr);
        return;
    }
    RemoteDNSServer *server = servers[pos];
    DNSClient::INSTANCE.asyncForwardUdp(session->udpDnsRequest, server->ip, server->port, server->timeout, [=](UdpDNSResponse *response) {
        if (response == nullptr && pos + 1 < servers.size()) {
            forwardUdpDNSRequest(session, complete, servers, pos + 1);
        } else {
            complete(response);
        }
    });
}
void DNSServer::calRemoteDNSServers(const DNSRecord &record, vector<RemoteDNSServer *> &servers) {
    auto host = record.host;
    if (record.ips.empty()) {
        servers = RemoteDNSServer::calculateQueryServer(host, config.servers);
    } else if (!record.matchArea) {
        unordered_set<string> notMatchServers = DNSCache::INSTANCE.queryNotMatchAreaServers(host);
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
bool DNSServer::beginQueryRemote(const string host, DNSSession *session) {
    bool result = false;
    rLock.lock();

    auto it = watingSessions.find(host);
    if (it == watingSessions.end()) {
        unordered_set<DNSSession *> ses;
        watingSessions.emplace(make_pair(host, ses));
        result = true;
        Logger::DEBUG << host << "real beginQueryRemote" << END;
    }
    if (session != nullptr) {
        watingSessions[host].emplace(session);
    }
    rLock.unlock();
    Logger::DEBUG << host << "beginQueryRemote" << END;
    return result;
}
unordered_set<DNSSession *> DNSServer::endQueryRemote(const string host) {
    unordered_set<DNSSession *> result;
    rLock.lock();
    result = watingSessions[host];
    watingSessions.erase(host);
    rLock.unlock();
    Logger::DEBUG << host << "endQueryRemote" << END;
    return result;
}
void DNSServer::updateDNSRecord(DNSRecord record) {
    Logger::DEBUG << "begin updateDNSRecord !" << record.host << END;

    auto host = record.host;
    vector<RemoteDNSServer *> servers;
    calRemoteDNSServers(record, servers);
    if (servers.empty()) {
        Logger::ERROR << host << "updateDNSRecord calRemoteDNSServers empty!" << END;
        return;
    }
    if (beginQueryRemote(host, nullptr)) {
        if (!servers.empty()) {
            syncDNSRecordFromServer(
                    host, [=](DNSRecord &record) {
                        endQueryRemote(host);
                        Logger::DEBUG << "updateDNSRecord finished!" << END;
                    },
                    servers, 0, false);
        } else {
            Logger::ERROR << "updateDNSRecord servers empty!" << END;
            endQueryRemote(host);
        }
    } else {
        Logger::DEBUG << "updateDNSRecord inQuerying" << host << END;
    }
}

void DNSServer::syncDNSRecordFromServer(const string host, std::function<void(DNSRecord &record)> complete,
                                        vector<RemoteDNSServer *> servers,
                                        int pos, bool completedWithAnyRecord) {
    if (pos >= servers.size()) {
        Logger::ERROR << "not known host" << host << END;
        DNSRecord record;
        DNSCache::INSTANCE.query(host, record);
        complete(record);
        return;
    }
    RemoteDNSServer *server = servers[pos];
    string logTag = host + " syncDNSRecordFromServer " + server->id();
    thread_pool *th = this->getThreadPool(server);
    if (th != nullptr) {
        uint64_t traceId = Logger::traceId;
        auto task = [=]() {
            Logger::traceId = traceId;
            Logger::DEBUG << logTag << "begin" << END;
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
            Logger::DEBUG << logTag << (ips.empty() ? "failed" : "success") << END;
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
            DNSRecord record;
            DNSCache::INSTANCE.query(host, record);
            if (!record.ips.empty() && completedWithAnyRecord) {
                complete(record);
                Logger::DEBUG << logTag << "finished" << END;
                updateDNSRecord(record);
            } else {
                if (ips.empty() && pos + 1 < servers.size()) {
                    syncDNSRecordFromServer(host, complete, servers, pos + 1, completedWithAnyRecord);
                } else {
                    Logger::DEBUG << logTag << "finished" << END;
                    complete(record);
                }
            }
        };
        boost::asio::post(*th, task);
        Logger::DEBUG << logTag << "assigned" << END;
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

thread_pool *DNSServer::getThreadPool(RemoteDNSServer *server) {
    auto thPool = workThreads.find(server->id());
    auto traceId = Logger::traceId;
    if (thPool != workThreads.end()) {
        return thPool->second;
    }
    Logger::ERROR << server->id() << "find thread pool empty" << END;
    return nullptr;
}
