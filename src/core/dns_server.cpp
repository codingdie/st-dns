//
// Created by codingdie on 2020/5/19.
//

#include "dns_server.h"

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "dns_record_manager.h"
#include "dns_client.h"
#include "st.h"
static mutex rLock;

using namespace std::placeholders;
using namespace std;
using namespace st::dns;
using namespace st::dns::protocol;
dns_server::dns_server(st::dns::config &config) : rid(time::now()), config(config), counter(0), console(config.console_ip, config.console_port) {
    try {
        ss = new udp::socket(ic, udp::endpoint(boost::asio::ip::make_address_v4(config.ip), config.port));
    } catch (const boost::system::system_error &e) {
        logger::ERROR << "bind address error" << config.ip << config.port << e.what() << END;
        exit(1);
    }
    start_console();
}
void dns_server::start_console() {
    console.desc.add_options()("domain", boost::program_options::value<string>()->default_value(""), "domain");
    console.desc.add_options()("ip", boost::program_options::value<string>()->default_value(""), "ip");
    console.impl = [](const vector<string> &commands, const boost::program_options::variables_map &options) {
        auto command = utils::strutils::join(commands, " ");
        std::pair<bool, std::string> result = make_pair(false, "not invalid command");
        string ip = options["ip"].as<string>();
        if (command == "dns resolve") {
            if (options.count("domain")) {
                auto domain = options["domain"].as<string>();
                if (!domain.empty()) {
                    auto record = dns_record_manager::uniq().resolve(domain);
                    result = make_pair(true, record.serialize());
                }
            }
        } else if (command == "dns reverse resolve") {
            if (!ip.empty()) {
                auto record = dns_record_manager::uniq().reverse_resolve(st::utils::ipv4::str_to_ip(ip));
                result = make_pair(true, join(record.domains(), ","));
            }
        } else if (command == "dns record get") {
            if (options.count("domain")) {
                auto domain = options["domain"].as<string>();
                if (!domain.empty()) {
                    auto records = dns_record_manager::uniq().get_dns_record_list(domain);
                    vector<string> strs(records.size());
                    std::transform(records.begin(), records.end(), strs.begin(), [](const dns_record &item) { return item.serialize(); });
                    result = make_pair(true, strutils::join(strs, "\n"));
                }
            }
        } else if (command == "dns record dump") {
            result = make_pair(true, dns_record_manager::uniq().dump());
        } else if (command == "dns record remove") {
            if (options.count("domain")) {
                auto domain = options["domain"].as<string>();
                if (!domain.empty()) {
                    dns_record_manager::uniq().remove(domain);
                    result = make_pair(true, "");
                }
            }
        } else if (command == "dns record clear") {
            dns_record_manager::uniq().clear();
            result = make_pair(true, "");
        } else if (command == "dns record analyse") {
            result = make_pair(true, dns_record_manager::uniq().stats().serialize());
        } else if (command == "ip area") {
            result = make_pair(true, areaip::manager::uniq().get_area(st::utils::ipv4::str_to_ip(ip)));
        }
        return result;
    };
    console.start();
}

void dns_server::start() {
    dns_record_manager::uniq();
    vector<thread> threads;
    unsigned int cpu_count = std::thread::hardware_concurrency();
    iw = new boost::asio::io_context::work(ic);
    for (auto i = 0; i < cpu_count; i++) {
        threads.emplace_back([this]() {
            this->ic.run();
        });
    }
    logger::INFO << "st-dns start, listen at" << config.ip << config.port << END;
    receive();
    state++;
    for (auto &th : threads) {
        th.join();
    }
    logger::INFO << "st-dns end" << END;
}

void dns_server::shutdown() {
    this->state = 2;
    ss->cancel();
    ic.stop();
    delete iw;
    ss->close();
    delete ss;
    apm_logger::disable();
    logger::INFO << "st-dns server stopped, listen at" << config.ip + ":" + to_string(config.port) << END;
}

void dns_server::wait_start() {
    while (state == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}
void dns_server::receive() {
    uint64_t id = rid.fetch_add(1);
    logger::traceId = id;
    auto *session = new st::dns::session(id);
    ss->async_receive_from(buffer(session->request.data, session->request.len),
                           session->client_endpoint,
                           [=](boost::system::error_code errorCode, std::size_t size) {
                               logger::traceId = session->get_id();
                               logger::DEBUG << "dns request" << ++counter << "received!" << END;
                               session->set_time(time::now());
                               session->logger.start();
                               if (!errorCode && size > 0) {
                                   bool parsed = session->request.parse(size);
                                   session->logger.step("parse_request");
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

uint32_t dns_server::cal_expire(dns_record &record) const {
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

    session->logger.add_metric("in_querying_domain_count", waiting_sessions.size());
    session->logger.add_metric("mem_leak_size", st::mem::leak_size());
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

void dns_server::query_dns_record(session *session, const std::function<void(st::dns::session *)> &complete_handler) {
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
        record = dns_record_manager::uniq().resolve(host);
        if (record.ips.empty()) {
            string fiDomain = dns_domain::getFIDomain(host);
            if (fiDomain == "LAN") {
                record = dns_record_manager::uniq().resolve(dns_domain::removeFIDomain(host));
                record.domain = host;
            }
        }
        if (record.ips.empty()) {
            session->logger.add_dimension("process_type", "remote");
            query_dns_record_from_remote(session, complete);
        } else {
            session->logger.add_dimension("process_type", "cache");
            complete(session);
            if (record.expire || !record.match_area) {
                update_dns_record(record.domain);
            }
        }
    }
}

void dns_server::query_dns_record_from_remote(session *session, const std::function<void(st::dns::session *session)> &complete_handler) {
    auto record = session->record;
    auto host = record.domain;
    vector<remote_dns_server *> servers = remote_dns_server::select_servers(host, config.servers);
    if (servers.empty()) {
        logger::ERROR << host << "query_dns_record_from_remote cal_remote_dns_servers empty!" << END;
        complete_handler(session);
        return;
    }
    if (begin_query_remote(host, session)) {
        sync_dns_record_from_remote(
                host, [=](const dns_record &record) {
                    auto sessions = end_query_remote(host);
                    for (auto se : sessions) {
                        se->record = record;
                        complete_handler(se);
                    }
                },
                servers, 0, !dns_record_manager::uniq().has_any_record(host));
    } else {
        logger::DEBUG << host << "is in query!" << END;
    }
}
void dns_server::forward_dns_request(session *session, const std::function<void(st::dns::session *session)> &complete_handler) {
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
void dns_server::forward_dns_request(session *session, const std::function<void(udp_response *)> &complete, vector<remote_dns_server *> servers, uint32_t pos) {
    if (pos >= servers.size()) {
        complete(nullptr);
        return;
    }
    remote_dns_server *server = servers[pos];
    dns_client::uniq().forward_udp(session->request, server->ip, server->port, server->timeout, [=](udp_response *response) {
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

    auto it = waiting_sessions.find(host);
    if (it == waiting_sessions.end()) {
        unordered_set<st::dns::session *> ses;
        waiting_sessions.emplace(host, ses);
        result = true;
        logger::DEBUG << host << "real begin_query_remote" << END;
    }
    if (session != nullptr) {
        waiting_sessions[host].emplace(session);
    }
    rLock.unlock();
    logger::DEBUG << host << "begin_query_remote" << END;
    return result;
}
unordered_set<session *> dns_server::end_query_remote(const string &host) {
    unordered_set<session *> result;
    rLock.lock();
    result = waiting_sessions[host];
    waiting_sessions.erase(host);
    rLock.unlock();
    logger::DEBUG << host << "end_query_remote" << END;
    return result;
}
void dns_server::update_dns_record(const string &domain) {
    logger::DEBUG << "begin update dn record !" << domain << END;

    vector<remote_dns_server *> servers = remote_dns_server::select_servers(domain, config.servers);
    if (servers.empty()) {
        logger::ERROR << domain << "update dns record cal servers empty!" << END;
        return;
    }
    if (begin_query_remote(domain, nullptr)) {
        if (!servers.empty()) {
            sync_dns_record_from_remote(
                    domain, [=](const dns_record &record) {
                        end_query_remote(domain);
                        logger::DEBUG << "update_dns_record finished!" << END;
                    },
                    servers, 0, false);
        } else {
            logger::ERROR << "update_dns_record servers empty!" << END;
            end_query_remote(domain);
        }
    } else {
        logger::DEBUG << "update_dns_record in querying" << domain << END;
    }
}

void dns_server::sync_dns_record_from_remote(const string &host, const std::function<void(dns_record record)> &complete,
                                             vector<remote_dns_server *> servers,
                                             int pos, bool return_resolve_any) {
    if (pos >= servers.size()) {
        logger::ERROR << "not known host" << host << END;
        complete(dns_record_manager::uniq().resolve(host));
        return;
    }
    remote_dns_server *server = servers[pos];
    uint64_t traceId = logger::traceId;
    dns_multi_area_complete complete_handler = [=](const vector<uint32_t> &ips, bool loadAll) {
        logger::traceId = traceId;
        if (!ips.empty()) {
            dns_record_manager::uniq().add(host, ips, server->id(), loadAll ? server->dns_cache_expire : server->dns_cache_expire / 2);
        }
        dns_record record = dns_record_manager::uniq().resolve(host);
        if (pos + 1 >= servers.size()) {
            complete(record);
        } else {
            if (!record.ips.empty() && record.match_area && return_resolve_any) {
                complete(record);
                update_dns_record(record.domain);
            } else {
                sync_dns_record_from_remote(host, complete, servers, pos + 1, return_resolve_any);
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
    areas.emplace("");
    if (server->type == "TCP_SSL") {
        dns_client::uniq().tcp_tls_dns(host, server->ip, server->port, server->timeout, areas, complete_handler);
    } else if (server->type == "TCP") {
        dns_client::uniq().tcp_dns(host, server->ip, server->port, server->timeout, areas, complete_handler);
    } else if (server->type == "UDP") {
        dns_client::uniq().udp_dns(host, server->ip, server->port, server->timeout, [=](const std::vector<uint32_t> &ips) {
            complete_handler(ips, true);
        });
    }
}