//
// Created by codingdie on 2020/5/19.
//

#include "DNSServer.h"

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "dns_cache.h"
#include "DNSClient.h"
#include "utils/utils.h"
static mutex rLock;

using namespace std::placeholders;
using namespace std;
using namespace st::dns;
DNSServer::DNSServer(st::dns::config &config) : rid(time::now()), config(config), counter(0) {
    try {
        socketS = new udp::socket(ioContext, udp::endpoint(boost::asio::ip::make_address_v4(config.ip), config.port));
    } catch (const boost::system::system_error &e) {
        logger::ERROR << "bind address error" << config.ip << config.port << e.what() << END;
        exit(1);
    }
}

void DNSServer::start() {
    ioWoker = new boost::asio::io_context::work(ioContext);
    dns_cache::INSTANCE.load_from_file();
    logger::INFO << "st-dns start, listen at" << config.ip << config.port << END;
    receive();
    state++;
    ioContext.run();
    logger::INFO << "st-dns end" << END;
}

void DNSServer::shutdown() {
    this->state = 2;
    socketS->cancel();
    ioContext.stop();
    delete ioWoker;
    socketS->close();
    logger::INFO << "st-dns server stoped, listen at" << config.ip + ":" + to_string(config.port) << END;
}

void DNSServer::waitStart() {
    while (state == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}
void DNSServer::receive() {
    uint64_t id = rid.fetch_add(1);
    logger::traceId = id;
    session *session = new st::dns::session(id);
    socketS->async_receive_from(buffer(session->request.data, session->request.len),
                                session->client_endpoint,
                                [=](boost::system::error_code errorCode, std::size_t size) {
                                    logger::traceId = session->get_id();
                                    logger::DEBUG << "dns request" << ++counter << "received!" << END;
                                    session->set_time(time::now());
                                    session->logger.start();
                                    if (!errorCode && size > 0) {
                                        bool parsed = session->request.parse(size);
                                        session->logger.step("parseRequest");
                                        if (parsed) {
                                            processSession(session);
                                        } else {
                                            logger::ERROR << "invalid dns request" << END;
                                            endDNSSession(session);
                                        }
                                    } else {
                                        endDNSSession(session);
                                    }
                                    receive();
                                });
}

uint32_t DNSServer::getExpire(dns_record &record) {
    uint32_t expire = config.dns_cache_expire;
    if (!record.match_area) {
        expire = 1;
    }
    expire = max(expire, (uint32_t) 1 * 10);
    expire = min(expire, (uint32_t) 10 * 60);
    return expire;
}

void DNSServer::processSession(session *session) {
    auto complete = [=](st::dns::session *se) {
        se->logger.step("buildResponse");
        udp::endpoint &clientEndpoint = se->client_endpoint;
        udp_response *udpResponse = se->response;
        if (udpResponse == nullptr) {
            udpResponse = new udp_response(se->request, se->record, getExpire(se->record));
            se->response = udpResponse;
        }
        socketS->async_send_to(buffer(udpResponse->data, udpResponse->len), clientEndpoint,
                               [=](boost::system::error_code writeError, size_t writeSize) {
                                   logger::traceId = se->get_id();
                                   se->logger.step("response");
                                   endDNSSession(se);
                               });
    };
    session->record.domain = session->get_host();
    session->logger.add_dimension("queryType", session->get_query_type_value());
    session->logger.add_dimension("domain", session->get_host());
    if (session->get_query_type() == DNSQuery::A) {
        queryDNSRecord(session, complete);
    } else if (session->get_query_type() == DNSQuery::AAAA) {
        session->process_type = session::process_type::DROP;
        session->logger.add_dimension("process_type", "drop");
        complete(session);
    } else {
        session->process_type = session::process_type::FORWARD;
        session->logger.add_dimension("process_type", "forward");
        forwardUdpDNSRequest(session, complete);
    }
}

void DNSServer::endDNSSession(session *session) {
    bool success = session->response != nullptr;
    if (session->process_type == session::process_type::QUERY) {
        success &= !session->record.ips.empty();
    }

    session->logger.add_metric("trusted_domain_count", dns_cache::INSTANCE.get_trusted_domain_count());
    session->logger.add_metric("inQueryingDomainCount", watingSessions.size());
    session->logger.add_metric("memLeakSize", st::mem::leak_size());
    session->logger.add_metric("dnsReverseSHMFreeSize", st::dns::shm::share().free_size());
    session->logger.add_dimension("success", to_string(success));
    auto firstIPArea = session->response != nullptr ? session->response->fist_ip_area() : "";
    auto areas = session->response != nullptr ? session->response->ip_areas() : vector<string>({});
    session->logger.add_dimension("firstIPArea", firstIPArea);
    session->logger.end();

    logger::INFO << "dns request prcocess" << session->process_type << (success ? "success!" : "failed!");
    logger::INFO << "type" << session->get_query_type();
    if (!session->request.get_host().empty()) {
        logger::INFO << "domain" << session->request.get_host();
    }
    if (!session->record.ips.empty()) {
        logger::INFO << st::utils::ipv4::ips_to_str(session->record.ips) << session->record.server;
    }
    logger::INFO << "cost" << time::now() - session->get_time() << END;
    delete session;
}

void DNSServer::queryDNSRecord(session *session, std::function<void(st::dns::session *)> finalComplete) {
    auto complete = [=](st::dns::session *se) {
        se->response = new udp_response(se->request, se->record, getExpire(se->record));
        finalComplete(se);
    };
    auto host = session->get_host();
    dns_record &record = session->record;
    if (host == "localhost") {
        record.server = "st-dns";
        record.domain = host;
        record.expire_time = std::numeric_limits<uint64_t>::max();
        session->logger.add_dimension("process_type", "local");
        complete(session);
    } else {
        dns_cache::INSTANCE.query(host, record);
        if (record.ips.empty()) {
            string fiDomain = DNSDomain::getFIDomain(host);
            if (fiDomain == "LAN") {
                dns_cache::INSTANCE.query(DNSDomain::removeFIDomain(host), record);
                record.domain = host;
            }
        }
        if (record.ips.empty()) {
            session->logger.add_dimension("process_type", "remote");
            queryDNSRecordFromServer(session, complete);
        } else {
            session->logger.add_dimension("process_type", "cache");
            if (record.expire || !record.match_area) {
                updateDNSRecord(record);
            }
            complete(session);
        }
    }
}

void DNSServer::queryDNSRecordFromServer(session *session, std::function<void(st::dns::session *session)> completeHandler) {
    auto record = session->record;
    auto host = record.domain;
    vector<remote_dns_server *> &servers = session->servers;
    calRemoteDNSServers(record, servers);
    if (servers.empty()) {
        logger::ERROR << host << "queryDNSRecordFromServer calRemoteDNSServers empty!" << END;
        completeHandler(session);
        return;
    }
    if (beginQueryRemote(host, session)) {
        syncDNSRecordFromServer(
                host, [=](dns_record record) {
                    auto sessions = endQueryRemote(host);
                    for (auto it = sessions.begin(); it != sessions.end(); it++) {
                        st::dns::session *se = *it;
                        se->record = record;
                        completeHandler(se);
                    }
                },
                servers, 0, dns_cache::INSTANCE.has_any_record(host));
    } else {
        logger::DEBUG << host << "is in query!" << END;
    }
}
void DNSServer::forwardUdpDNSRequest(session *session, std::function<void(st::dns::session *session)> completeHandler) {
    vector<remote_dns_server *> &servers = session->servers;
    for (auto &it : config.servers) {
        if (it->type == "UDP") {
            servers.emplace_back(it);
        }
    }
    if (!servers.empty()) {
        forwardUdpDNSRequest(
                session, [=](udp_response *res) {
                    session->response = res;
                    completeHandler(session);
                },
                servers, 0);
    } else {
        completeHandler(session);
    }
}
void DNSServer::forwardUdpDNSRequest(session *session, std::function<void(udp_response *)> complete, vector<remote_dns_server *> servers, uint32_t pos) {
    if (pos >= servers.size()) {
        complete(nullptr);
        return;
    }
    remote_dns_server *server = servers[pos];
    DNSClient::INSTANCE.forwardUdp(session->request, server->ip, server->port, server->timeout, [=](udp_response *response) {
        if (response == nullptr && pos + 1 < servers.size()) {
            forwardUdpDNSRequest(session, complete, servers, pos + 1);
        } else {
            complete(response);
        }
    });
}
void DNSServer::calRemoteDNSServers(const dns_record &record, vector<remote_dns_server *> &servers) {
    auto host = record.domain;
    //默认更新所有上游server记录
    servers = remote_dns_server::calculateQueryServer(host, config.servers);
    //如果当前记录不为空，且满足地区最优，且所有上游都至少成功同步过一次记录，则只更新此server记录
    if (!record.ips.empty() && record.match_area && dns_cache::INSTANCE.server_count(host) == servers.size()) {
        servers.clear();
        remote_dns_server *server = this->config.get_dns_server_by_id(record.server);
        if (server != nullptr) {
            servers.emplace_back(server);
        } else {
            servers = remote_dns_server::calculateQueryServer(host, config.servers);
        }
    }
}
bool DNSServer::beginQueryRemote(const string host, session *session) {
    bool result = false;
    rLock.lock();

    auto it = watingSessions.find(host);
    if (it == watingSessions.end()) {
        unordered_set<st::dns::session *> ses;
        watingSessions.emplace(make_pair(host, ses));
        result = true;
        logger::DEBUG << host << "real beginQueryRemote" << END;
    }
    if (session != nullptr) {
        watingSessions[host].emplace(session);
    }
    rLock.unlock();
    logger::DEBUG << host << "beginQueryRemote" << END;
    return result;
}
unordered_set<session *> DNSServer::endQueryRemote(const string host) {
    unordered_set<session *> result;
    rLock.lock();
    result = watingSessions[host];
    watingSessions.erase(host);
    rLock.unlock();
    logger::DEBUG << host << "endQueryRemote" << END;
    return result;
}
void DNSServer::updateDNSRecord(dns_record record) {
    logger::DEBUG << "begin updateDNSRecord !" << record.domain << END;

    auto host = record.domain;
    vector<remote_dns_server *> servers;
    calRemoteDNSServers(record, servers);
    if (servers.empty()) {
        logger::ERROR << host << "updateDNSRecord calRemoteDNSServers empty!" << END;
        return;
    }
    if (beginQueryRemote(host, nullptr)) {
        if (!servers.empty()) {
            syncDNSRecordFromServer(
                    host, [=](dns_record record) {
                        endQueryRemote(host);
                        logger::DEBUG << "updateDNSRecord finished!" << END;
                    },
                    servers, 0, false);
        } else {
            logger::ERROR << "updateDNSRecord servers empty!" << END;
            endQueryRemote(host);
        }
    } else {
        logger::DEBUG << "updateDNSRecord inQuerying" << host << END;
    }
}

void DNSServer::syncDNSRecordFromServer(const string host, std::function<void(dns_record record)> complete,
                                        vector<remote_dns_server *> servers,
                                        int pos, bool completedWithAnyRecord) {
    if (pos >= servers.size()) {
        logger::ERROR << "not known host" << host << END;
        dns_record record;
        dns_cache::INSTANCE.query(host, record);
        complete(record);
        return;
    }
    remote_dns_server *server = servers[pos];
    string logTag = host + " syncDNSRecordFromServer " + server->id();
    uint64_t traceId = logger::traceId;
    std::function<void(vector<uint32_t> ips, bool loadAll)> dnsComplete = [=](vector<uint32_t> ips, bool loadAll) {
        logger::traceId = traceId;
        vector<uint32_t> oriIps = ips;
        filterIPByArea(host, server, ips);
        if (!ips.empty()) {
            dns_cache::INSTANCE.add(host, ips, server->id(),
                                        loadAll ? server->dns_cache_expire : 60, true);
        } else {
            if (!oriIps.empty()) {
                dns_cache::INSTANCE.add(host, oriIps, server->id(),
                                            server->dns_cache_expire, false);
            }
        }
        dns_record record;
        dns_cache::INSTANCE.query(host, record);
        logger::DEBUG << logTag << "finished! original" << ipv4::ips_to_str(ips) << " final" << ipv4::ips_to_str(ips) << END;
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
    if (config.area_resolve_optimize) {
        for (auto area : server->areas) {
            if (area[0] != '!') {
                areas.emplace(area);
            }
        }
    }
    logger::DEBUG << logTag << "start!" << END;
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

void DNSServer::filterIPByArea(const string host, remote_dns_server *server, vector<uint32_t> &ips) {
    if (!ips.empty() && server->whitelist.find(host) == server->whitelist.end() && !server->areas.empty()) {
        for (auto it = ips.begin(); it != ips.end();) {
            auto ip = *it;
            if (!st::areaip::manager::uniq().is_area_ip(server->areas, ip)) {
                it = ips.erase(it);
                logger::DEBUG << host << "remove not area ip" << st::utils::ipv4::ip_to_str(ip) << "from" << server->ip
                              << st::utils::join(server->areas, "/") << END;
            } else {
                it++;
            }
        }
    }
}
