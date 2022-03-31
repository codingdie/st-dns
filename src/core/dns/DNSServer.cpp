//
// Created by codingdie on 2020/5/19.
//

#include "DNSServer.h"

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "DNSCache.h"
#include "DNSClient.h"
#include "utils/STUtils.h"
static mutex rLock;

using namespace std::placeholders;
using namespace std;

DNSServer::DNSServer(st::dns::Config &config) : rid(time::now()), config(config), counter(0) {
    try {
        socketS = new udp::socket(ioContext, udp::endpoint(boost::asio::ip::make_address_v4(config.ip), config.port));
    } catch (const boost::system::system_error &e) {
        Logger::ERROR << "bind address error" << config.ip << config.port << e.what() << END;
        exit(1);
    }
}

void DNSServer::start() {
    ioWoker = new boost::asio::io_context::work(ioContext);
    DNSCache::INSTANCE.loadFromFile();
    Logger::INFO << "st-dns start, listen at" << config.ip << config.port << END;
    receive();
    state++;
    ioContext.run();
    Logger::INFO << "st-dns end" << END;
}

void DNSServer::shutdown() {
    this->state = 2;
    socketS->cancel();
    ioContext.stop();
    delete ioWoker;
    socketS->close();
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
                                    session->apmLogger.start();
                                    if (!errorCode && size > 0) {
                                        bool parsed = session->udpDnsRequest.parse(size);
                                        session->apmLogger.step("parseRequest");
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

uint32_t DNSServer::getExpire(DNSRecord &record) {
    uint32_t expire = config.dnsCacheExpire;
    if (!record.matchArea) {
        expire = 1;
    }
    expire = max(expire, (uint32_t) 1 * 10);
    expire = min(expire, (uint32_t) 10 * 60);
    return expire;
}

void DNSServer::processSession(DNSSession *session) {
    auto complete = [=](DNSSession *se) {
        se->apmLogger.step("buildResponse");
        udp::endpoint &clientEndpoint = se->clientEndpoint;
        UdpDNSResponse *udpResponse = se->udpDNSResponse;
        if (udpResponse == nullptr) {
            udpResponse = new UdpDNSResponse(se->udpDnsRequest, se->record, getExpire(se->record));
            se->udpDNSResponse = udpResponse;
        }
        socketS->async_send_to(buffer(udpResponse->data, udpResponse->len), clientEndpoint,
                               [=](boost::system::error_code writeError, size_t writeSize) {
                                   Logger::traceId = se->getId();
                                   se->apmLogger.step("response");
                                   endDNSSession(se);
                               });
    };
    session->record.domain = session->getHost();
    session->apmLogger.addDimension("queryType", session->getQueryTypeValue());
    session->apmLogger.addDimension("domain", session->getHost());
    if (session->getQueryType() == DNSQuery::A) {
        queryDNSRecord(session, complete);
    } else if (session->getQueryType() == DNSQuery::AAAA) {
        session->processType = DNSSession::ProcessType::DROP;
        session->apmLogger.addDimension("processType", "drop");
        complete(session);
    } else {
        session->processType = DNSSession::ProcessType::FORWARD;
        session->apmLogger.addDimension("processType", "forward");
        forwardUdpDNSRequest(session, complete);
    }
}

void DNSServer::endDNSSession(DNSSession *session) {
    bool success = session->udpDNSResponse != nullptr;
    if (session->processType == DNSSession::ProcessType::QUERY) {
        success &= !session->record.ips.empty();
    }

    session->apmLogger.addMetric("trustedDomainCount", DNSCache::INSTANCE.getTrustedDomainCount());
    session->apmLogger.addMetric("inQueryingDomainCount", watingSessions.size());
    session->apmLogger.addMetric("memLeak", st::mem::leakSize());
    session->apmLogger.addDimension("success", to_string(success));
    auto firstIPArea = session->udpDNSResponse != nullptr ? session->udpDNSResponse->fistIPArea() : "";
    auto areas = session->udpDNSResponse != nullptr ? session->udpDNSResponse->IPAreas() : vector<string>({});
    session->apmLogger.addDimension("firstIPArea", firstIPArea);
    session->apmLogger.end();

    Logger::INFO << "dns request prcocess" << session->processType << (success ? "success!" : "failed!");
    Logger::INFO << "type" << session->getQueryType();
    if (!session->udpDnsRequest.getHost().empty()) {
        Logger::INFO << "domain" << session->udpDnsRequest.getHost();
    }
    if (!session->record.ips.empty()) {
        Logger::INFO << st::utils::ipv4::ipsToStr(session->record.ips) << session->record.dnsServer;
    }
    Logger::INFO << "cost" << time::now() - session->getTime() << END;
    delete session;
}

void DNSServer::queryDNSRecord(DNSSession *session, std::function<void(DNSSession *)> finalComplete) {
    auto complete = [=](DNSSession *se) {
        se->udpDNSResponse = new UdpDNSResponse(se->udpDnsRequest, se->record, getExpire(se->record));
        finalComplete(se);
    };
    auto host = session->getHost();
    DNSRecord &record = session->record;
    if (host == "localhost") {
        record.dnsServer = "st-dns";
        record.domain = host;
        record.expireTime = std::numeric_limits<uint64_t>::max();
        session->apmLogger.addDimension("processType", "local");
        complete(session);
    } else {
        DNSCache::INSTANCE.query(host, record);
        if (record.ips.empty()) {
            string fiDomain = DNSDomain::getFIDomain(host);
            if (fiDomain == "LAN") {
                DNSCache::INSTANCE.query(DNSDomain::removeFIDomain(host), record);
                record.domain = host;
            }
        }
        if (record.ips.empty()) {
            session->apmLogger.addDimension("processType", "remote");
            queryDNSRecordFromServer(session, complete);
        } else {
            session->apmLogger.addDimension("processType", "cache");
            if (record.expire || !record.matchArea) {
                updateDNSRecord(record);
            }
            complete(session);
        }
    }
}

void DNSServer::queryDNSRecordFromServer(DNSSession *session, std::function<void(DNSSession *session)> completeHandler) {
    auto record = session->record;
    auto host = record.domain;
    vector<RemoteDNSServer *> &servers = session->servers;
    calRemoteDNSServers(record, servers);
    if (servers.empty()) {
        Logger::ERROR << host << "queryDNSRecordFromServer calRemoteDNSServers empty!" << END;
        completeHandler(session);
        return;
    }
    if (beginQueryRemote(host, session)) {
        syncDNSRecordFromServer(
                host, [=](DNSRecord record) {
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
void DNSServer::forwardUdpDNSRequest(DNSSession *session, std::function<void(UdpDNSResponse *)> complete, vector<RemoteDNSServer *> servers, uint32_t pos) {
    if (pos >= servers.size()) {
        complete(nullptr);
        return;
    }
    RemoteDNSServer *server = servers[pos];
    DNSClient::INSTANCE.forwardUdp(session->udpDnsRequest, server->ip, server->port, server->timeout, [=](UdpDNSResponse *response) {
        if (response == nullptr && pos + 1 < servers.size()) {
            forwardUdpDNSRequest(session, complete, servers, pos + 1);
        } else {
            complete(response);
        }
    });
}
void DNSServer::calRemoteDNSServers(const DNSRecord &record, vector<RemoteDNSServer *> &servers) {
    auto host = record.domain;
    //默认更新所有上游server记录
    servers = RemoteDNSServer::calculateQueryServer(host, config.servers);
    //如果当前记录不为空，且满足地区最优，且所有上游都至少成功同步过一次记录，则只更新此server记录
    if (!record.ips.empty() && record.matchArea && DNSCache::INSTANCE.serverCount(host) == servers.size()) {
        servers.clear();
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
    Logger::DEBUG << "begin updateDNSRecord !" << record.domain << END;

    auto host = record.domain;
    vector<RemoteDNSServer *> servers;
    calRemoteDNSServers(record, servers);
    if (servers.empty()) {
        Logger::ERROR << host << "updateDNSRecord calRemoteDNSServers empty!" << END;
        return;
    }
    if (beginQueryRemote(host, nullptr)) {
        if (!servers.empty()) {
            syncDNSRecordFromServer(
                    host, [=](DNSRecord record) {
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

void DNSServer::syncDNSRecordFromServer(const string host, std::function<void(DNSRecord record)> complete,
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
    uint64_t traceId = Logger::traceId;
    std::function<void(vector<uint32_t> ips, bool loadAll)> dnsComplete = [=](vector<uint32_t> ips, bool loadAll) {
        Logger::traceId = traceId;
        vector<uint32_t> oriIps = ips;
        filterIPByArea(host, server, ips);
        if (!ips.empty()) {
            DNSCache::INSTANCE.addCache(host, ips, server->id(),
                                        loadAll ? server->dnsCacheExpire : 60, true);
        } else {
            if (!oriIps.empty()) {
                DNSCache::INSTANCE.addCache(host, oriIps, server->id(),
                                            server->dnsCacheExpire, false);
            }
        }
        DNSRecord record;
        DNSCache::INSTANCE.query(host, record);
        Logger::DEBUG << logTag << "finished! original" << ipv4::ipsToStr(ips) << " final" << ipv4::ipsToStr(ips) << END;
        if (!record.ips.empty() && completedWithAnyRecord) {
            complete(record);
            updateDNSRecord(record);
        } else {
            if (ips.empty() && pos + 1 < servers.size()) {
                syncDNSRecordFromServer(host, complete, servers, pos + 1, completedWithAnyRecord);
            } else {
                complete(record);
            }
        }
    };
    unordered_set<string> areas;
    if (config.areaResolveOptimize) {
        for (auto area : server->areas) {
            if (area[0] != '!') {
                areas.emplace(area);
            }
        }
    }
    Logger::DEBUG << logTag << "start!" << END;
    if (server->type.compare("TCP_SSL") == 0) {
        DNSClient::INSTANCE.tcpTlsDNS(host, server->ip, server->port, server->timeout, areas, dnsComplete);
    } else if (server->type.compare("TCP") == 0) {
        DNSClient::INSTANCE.tcpDNS(host, server->ip, server->port, server->timeout, areas, dnsComplete);
    } else if (server->type.compare("UDP") == 0) {
        DNSClient::INSTANCE.udpDns(host, server->ip, server->port, server->timeout, [=](std::vector<uint32_t> ips) {
            dnsComplete(ips, true);
        });
    }
}

void DNSServer::filterIPByArea(const string host, RemoteDNSServer *server, vector<uint32_t> &ips) {
    if (!ips.empty() && server->whitelist.find(host) == server->whitelist.end() && !server->areas.empty()) {
        for (auto it = ips.begin(); it != ips.end();) {
            auto ip = *it;
            if (!st::areaip::Manager::uniq().isAreaIP(server->areas, ip)) {
                it = ips.erase(it);
                Logger::DEBUG << host << "remove not area ip" << st::utils::ipv4::ipToStr(ip) << "from" << server->ip
                              << st::utils::join(server->areas, "/") << END;
            } else {
                it++;
            }
        }
    }
}
