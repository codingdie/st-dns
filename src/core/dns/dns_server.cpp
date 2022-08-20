//
// Created by codingdie on 2020/5/19.
//

#include "dns_server.h"

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "dns_cache.h"
#include "dns_client.h"
#include "utils/utils.h"
static mutex rLock;

using namespace std::placeholders;
using namespace std;
using namespace st::dns;
using namespace st::dns::protocol;
dns_server::dns_server(st::dns::config &config) : rid(time::now()), config(config), counter(0) {
    try {
        ss = new udp::socket(ic, udp::endpoint(boost::asio::ip::make_address_v4(config.ip), config.port));
    } catch (const boost::system::system_error &e) {
        logger::ERROR << "bind address error" << config.ip << config.port << e.what() << END;
        exit(1);
    }
}

void dns_server::start() {
    iw = new boost::asio::io_context::work(ic);
    dns_cache::INSTANCE.load_from_file();
    logger::INFO << "st-dns start, listen at" << config.ip << config.port << END;
    receive();
    state++;
    ic.run();
    logger::INFO << "st-dns end" << END;
}

void dns_server::shutdown() {
    this->state = 2;
    ss->cancel();
    ic.stop();
    delete iw;
    ss->close();
    logger::INFO << "st-dns server stoped, listen at" << config.ip + ":" + to_string(config.port) << END;
}

void dns_server::wait_start() {
    while (state == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}
void dns_server::receive() {
    uint64_t id = rid.fetch_add(1);
    logger::traceId = id;
    session *session = new st::dns::session(id);
    ss->async_receive_from(buffer(session->request.data, session->request.len),
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
                                       process_session(session);
                                   } else {
                                       logger::ERROR << "invalid dns request" << END;
                                       end_session(session);
                                   }
                               } else {
                                   end_session(session);
                               }
                               receive();
                           });
}

uint32_t dns_server::cal_expire(dns_record &record) {
    uint32_t expire = config.dns_cache_expire;
    if (!record.match_area) {
        expire = 1;
    }
    expire = max(expire, (uint32_t) 1 * 10);
    expire = min(expire, (uint32_t) 10 * 60);
    return expire;
}

void dns_server::process_session(session *session) {
    auto complete = [=](st::dns::session *se) {
        se->logger.step("buildResponse");
        udp::endpoint &clientEndpoint = se->client_endpoint;
        udp_response *udpResponse = se->response;
        if (udpResponse == nullptr) {
            udpResponse = new udp_response(se->request, se->record, cal_expire(se->record));
            se->response = udpResponse;
        }
        ss->async_send_to(buffer(udpResponse->data, udpResponse->len), clientEndpoint,
                          [=](boost::system::error_code writeError, size_t writeSize) {
                              logger::traceId = se->get_id();
                              se->logger.step("response");
                              end_session(se);
                          });
    };
    session->record.domain = session->get_host();
    session->logger.add_dimension("query_type", session->get_query_type_value());
    session->logger.add_dimension("domain", session->get_host());
    if (session->get_query_type() == dns_query::A) {
        query_dns_record(session, complete);
    } else if (session->get_query_type() == dns_query::AAAA) {
        session->process_type = session::process_type::DROP;
        session->logger.add_dimension("process_type", "drop");
        complete(session);
    } else {
        session->process_type = session::process_type::FORWARD;
        session->logger.add_dimension("process_type", "forward");
        forward_dns_request(session, complete);
    }
}

void dns_server::end_session(session *session) {
    bool success = session->response != nullptr;
    if (session->process_type == session::process_type::QUERY) {
        success &= !session->record.ips.empty();
    }

    session->logger.add_metric("trusted_domain_count", dns_cache::INSTANCE.get_trusted_domain_count());
    session->logger.add_metric("in_querying_domain_count", wating_sessions.size());
    session->logger.add_metric("mem_leak_size", st::mem::leak_size());
    session->logger.add_metric("dns_reverse_shm_free_size", st::dns::shm::share().free_size());
    session->logger.add_dimension("success", to_string(success));
    auto firstIPArea = session->response != nullptr ? session->response->fist_ip_area() : "";
    auto areas = session->response != nullptr ? session->response->ip_areas() : vector<string>({});
    session->logger.add_dimension("first_ip_area", firstIPArea);
    session->logger.end();

    logger::INFO << "dns request process" << session->process_type << (success ? "success!" : "failed!");
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

void dns_server::query_dns_record(session *session, std::function<void(st::dns::session *)> complete_handler) {
    auto complete = [=](st::dns::session *se) {
        se->response = new udp_response(se->request, se->record, cal_expire(se->record));
        complete_handler(se);
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
            string fiDomain = dns_domain::getFIDomain(host);
            if (fiDomain == "LAN") {
                dns_cache::INSTANCE.query(dns_domain::removeFIDomain(host), record);
                record.domain = host;
            }
        }
        if (record.ips.empty()) {
            session->logger.add_dimension("process_type", "remote");
            query_dns_record_from_remote(session, complete);
        } else {
            session->logger.add_dimension("process_type", "cache");
            if (record.expire || !record.match_area) {
                update_dns_record(record.domain);
            }
            complete(session);
        }
    }
}

void dns_server::query_dns_record_from_remote(session *session, std::function<void(st::dns::session *session)> complete_handler) {
    auto record = session->record;
    auto host = record.domain;
    vector<remote_dns_server *> servers = remote_dns_server::calculateQueryServer(host, config.servers);
    if (servers.empty()) {
        logger::ERROR << host << "query_dns_record_from_remote cal_remote_dns_servers empty!" << END;
        complete_handler(session);
        return;
    }
    if (begin_query_remote(host, session)) {
        sync_dns_record_from_remote(
                host, [=](dns_record record) {
                    auto sessions = end_query_remote(host);
                    for (auto it = sessions.begin(); it != sessions.end(); it++) {
                        st::dns::session *se = *it;
                        se->record = record;
                        complete_handler(se);
                    }
                },
                servers, 0, dns_cache::INSTANCE.has_any_record(host));
    } else {
        logger::DEBUG << host << "is in query!" << END;
    }
}
void dns_server::forward_dns_request(session *session, std::function<void(st::dns::session *session)> complete_handler) {
    vector<remote_dns_server *> servers;
    for (auto &it : config.servers) {
        if (it->type == "UDP") {
            servers.emplace_back(it);
        }
    }
    if (!servers.empty()) {
        forward_dns_request(
                session, [=](udp_response *res) {
                    session->response = res;
                    complete_handler(session);
                },
                servers, 0);
    } else {
        complete_handler(session);
    }
}
void dns_server::forward_dns_request(session *session, std::function<void(udp_response *)> complete, vector<remote_dns_server *> servers, uint32_t pos) {
    if (pos >= servers.size()) {
        complete(nullptr);
        return;
    }
    remote_dns_server *server = servers[pos];
    dns_client::INSTANCE.forward_udp(session->request, server->ip, server->port, server->timeout, [=](udp_response *response) {
        if (response == nullptr && pos + 1 < servers.size()) {
            forward_dns_request(session, complete, servers, pos + 1);
        } else {
            complete(response);
        }
    });
}
bool dns_server::begin_query_remote(const string &host, session *session) {
    bool result = false;
    rLock.lock();

    auto it = wating_sessions.find(host);
    if (it == wating_sessions.end()) {
        unordered_set<st::dns::session *> ses;
        wating_sessions.emplace(make_pair(host, ses));
        result = true;
        logger::DEBUG << host << "real begin_query_remote" << END;
    }
    if (session != nullptr) {
        wating_sessions[host].emplace(session);
    }
    rLock.unlock();
    logger::DEBUG << host << "begin_query_remote" << END;
    return result;
}
unordered_set<session *> dns_server::end_query_remote(const string &host) {
    unordered_set<session *> result;
    rLock.lock();
    result = wating_sessions[host];
    wating_sessions.erase(host);
    rLock.unlock();
    logger::DEBUG << host << "end_query_remote" << END;
    return result;
}
void dns_server::update_dns_record(const string &domain) {
    logger::DEBUG << "begin update_dns_record !" << domain << END;

    vector<remote_dns_server *> servers = remote_dns_server::calculateQueryServer(domain, config.servers);
    if (servers.empty()) {
        logger::ERROR << domain << "update_dns_record cal_remote_dns_servers empty!" << END;
        return;
    }
    if (begin_query_remote(domain, nullptr)) {
        if (!servers.empty()) {
            sync_dns_record_from_remote(
                    domain, [=](dns_record record) {
                        end_query_remote(domain);
                        logger::DEBUG << "update_dns_record finished!" << END;
                    },
                    servers, 0, false);
        } else {
            logger::ERROR << "update_dns_record servers empty!" << END;
            end_query_remote(domain);
        }
    } else {
        logger::DEBUG << "update_dns_record inQuerying" << domain << END;
    }
}

void dns_server::sync_dns_record_from_remote(const string &host, std::function<void(dns_record record)> complete,
                                             vector<remote_dns_server *> servers,
                                             int pos, bool completed) {
    if (pos >= servers.size()) {
        logger::ERROR << "not known host" << host << END;
        dns_record record;
        dns_cache::INSTANCE.query(host, record);
        complete(record);
        return;
    }
    remote_dns_server *server = servers[pos];
    string logTag = host + " sync_dns_record_from_remote " + server->id();
    uint64_t traceId = logger::traceId;
    std::function<void(vector<uint32_t> ips, bool loadAll)> dnsComplete = [=](vector<uint32_t> ips, bool loadAll) {
        logger::traceId = traceId;
        vector<uint32_t> oriIps = ips;
        filter_ip_by_area(host, server, ips);
        if (!ips.empty()) {
            dns_cache::INSTANCE.add(host, ips, server->id(),
                                    loadAll ? server->dns_cache_expire : 60, true);
        } else {
            if (!oriIps.empty()) {
                dns_cache::INSTANCE.add(host, oriIps, server->id(), server->dns_cache_expire, false);
            }
        }
        dns_record record;
        dns_cache::INSTANCE.query(host, record);
        logger::DEBUG << logTag << "finished! original" << ipv4::ips_to_str(ips) << " final" << ipv4::ips_to_str(ips) << END;
        if (!record.ips.empty() && completed) {
            complete(record);
            update_dns_record(record.domain);
        } else {
            if (ips.empty() && pos + 1 < servers.size()) {
                sync_dns_record_from_remote(host, complete, servers, pos + 1, completed);
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
        dns_client::INSTANCE.tcp_tls_dns(host, server->ip, server->port, server->timeout, areas, dnsComplete);
    } else if (server->type.compare("TCP") == 0) {
        dns_client::INSTANCE.tcp_dns(host, server->ip, server->port, server->timeout, areas, dnsComplete);
    } else if (server->type.compare("UDP") == 0) {
        dns_client::INSTANCE.udp_dns(host, server->ip, server->port, server->timeout, [=](std::vector<uint32_t> ips) {
            dnsComplete(ips, true);
        });
    }
}

void dns_server::filter_ip_by_area(const string &host, remote_dns_server *server, vector<uint32_t> &ips) {
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
