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

DNSServer::DNSServer(st::dns::Config &config) : rid(time::now()), config(config) {
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
    delete ioWoker;
    ioContext.stop();
    Logger::INFO << "st-dns server stoped, listen at" << config.ip + ":" + to_string(config.port) << END;
}

void DNSServer::waitStart() {
    cout << state << endl;
    while (state == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    cout << state << endl;
}
void DNSServer::receive() {
    uint64_t id = rid.fetch_add(1);
    Logger::traceId = id;
    DNSSession *session = new DNSSession(id);
    socketS->async_receive_from(buffer(session->udpDnsRequest.data, session->udpDnsRequest.len),
                                session->clientEndpoint,
                                [=](boost::system::error_code errorCode, std::size_t size) {
                                    Logger::traceId = session->getId();
                                    Logger::DEBUG << "dns request received!" << END;
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
        if (udpResponse != nullptr) {
            socketS->async_send_to(buffer(udpResponse->data, udpResponse->len), clientEndpoint,
                                   [=](boost::system::error_code writeError, size_t writeSize) {
                                       Logger::traceId = se->getId();
                                       se->stepLogger.step("response");
                                       endDNSSession(se);
                                   });
        } else {
            endDNSSession(se);
        }
    };
    session->record.host = session->getHost();
    session->stepLogger.addDimension("queryType", session->getQueryTypeValue());
    session->stepLogger.addDimension("domain", session->getHost());
    if (session->getQueryType() == DNSQuery::A || session->getQueryType() == DNSQuery::CNAME) {
        session->stepLogger.addDimension("processMethod", "resolve");
        queryDNSRecord(session, complete);
    } else if (session->getQueryType() != DNSQuery::AAAA) {
        session->processType = DNSSession::ProcessType::FORWARD;
        session->stepLogger.addDimension("processMethod", "forward");
        forwardUdpDNSRequest(session, complete);
    } else {
        session->processType = DNSSession::ProcessType::DROP;
        session->stepLogger.addDimension("processMethod", "drop");
        auto id = session->udpDnsRequest.dnsHeader->id;
        session->udpDNSResponse = new UdpDNSResponse(id, session->record);
        complete(session);
    }
}

void DNSServer::endDNSSession(DNSSession *session) {
    bool success = session->udpDNSResponse != nullptr;
    if (session->processType == DNSSession::ProcessType::QUERY) {
        success &= !session->record.ips.empty();
    }
    session->stepLogger.addMetric("cacheTotalCount", DNSCache::INSTANCE.getTotalCount());
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
        se->udpDNSResponse = new UdpDNSResponse(id, se->record);
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
        completeHandler(session);
        return;
    }
    if (beginQuery(host, session)) {
        syncDNSRecordFromServer(
                host, [=](DNSRecord &record) {
                    auto sessions = endQuery(host);
                    Logger::INFO << END;
                    for (auto it = sessions.begin(); it != sessions.end(); it++) {
                        DNSSession *se = *it;
                        se->record = record;
                        completeHandler(se);
                    }
                },
                servers, 0, true);
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
bool DNSServer::beginQuery(const string host, DNSSession *session) {
    bool result = false;
    rLock.lock();
    auto it = watingSessions.find(host);
    if (it == watingSessions.end()) {
        unordered_set<DNSSession *> ses;
        watingSessions.emplace(make_pair(host, ses));
        result = true;
    }
    if (session != nullptr) {
        watingSessions[host].emplace(session);
    }
    rLock.unlock();
    return result;
}
unordered_set<DNSSession *> DNSServer::endQuery(const string host) {
    unordered_set<DNSSession *> result;
    rLock.lock();
    result = watingSessions[host];
    watingSessions.erase(host);
    rLock.unlock();
    return result;
}
void DNSServer::updateDNSRecord(DNSRecord record) {
    Logger::DEBUG << "begin updateDNSRecord !" << record.host << END;

    auto host = record.host;
    vector<RemoteDNSServer *> servers;
    calRemoteDNSServers(record, servers);
    if (beginQuery(host, nullptr)) {
        if (!servers.empty()) {
            syncDNSRecordFromServer(
                    host, [=](DNSRecord &record) {
                        endQuery(host);
                        Logger::DEBUG << "updateDNSRecord finished!" << END;
                    },
                    servers, 0, false);
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
                                        int pos, bool completedWithAnyRecord) {
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
            Logger::DEBUG << host << "query dns from" << server->id() << (ips.empty() ? "not match area" : "success") << END;
            DNSRecord record;
            DNSCache::INSTANCE.query(host, record);
            if (!record.ips.empty() && completedWithAnyRecord) {
                complete(record);
                updateDNSRecord(record);
                return;
            }
            if (ips.empty() && pos + 1 < servers.size()) {
                syncDNSRecordFromServer(host, complete, servers, pos + 1, completedWithAnyRecord);
            } else {
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
