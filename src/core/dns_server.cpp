//
// Created by codingdie on 2020/5/19.
//

#include "dns_server.h"
#include "console_manager.h"

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "dns_record_manager.h"
#include "dns_client.h"
#include "st.h"
#include "command/proxy_command.h"

using namespace std::placeholders;
using namespace std;
using namespace st::dns;
using namespace st::dns::protocol;
dns_server::dns_server(st::dns::config &config) : rid(time::now()),
                                                  config(config),
                                                  counter(0),
                                                  sync_remote_record_task_queue(
                                                          "st-dns-record-sync-task",
                                                          100,
                                                          100,
                                                          [=](const st::task::priority_task<pair<string, remote_dns_server *>> &task) {
                                                              auto domain = task.get_input().first;
                                                              auto server = task.get_input().second;
                                                              sync_dns_record_from_remote(domain, [=](const dns_record &record) { sync_remote_record_task_queue.complete(task); }, server);
                                                          }) {
    try {
        ss = new udp::socket(ic, udp::endpoint(boost::asio::ip::make_address_v4(config.ip), config.port));
    } catch (const boost::system::system_error &e) {
        logger::ERROR << "bind address error" << config.ip << config.port << e.what() << END;
        exit(1);
    }

}
void dns_server::start_console() {
    console_manager::uniq().init(config.console_ip, config.console_port);
    console_manager::uniq().start();
}

void dns_server::start() {
    dns_record_manager::uniq();
    start_console();
    unsigned int cpu_count = std::thread::hardware_concurrency();
    iw = new boost::asio::io_context::work(ic);
    schedule_iw = new boost::asio::io_context::work(schedule_ic);
    vector<thread> threads;
    threads.reserve(cpu_count);
    for (auto i = 0; i < cpu_count; i++) {
        threads.emplace_back([this]() {
            this->ic.run();
        });
    }
    threads.emplace_back([this]() {
        this->schedule_ic.run();
    });
    logger::INFO << "st-dns start, listen at" << config.ip << config.port << END;
    receive();
    state++;
    schedule_timer = new boost::asio::deadline_timer(schedule_ic);
    schedule();
    for (auto &th : threads) {
        th.join();
    }
    logger::INFO << "st-dns end" << END;
}

void dns_server::shutdown() {
    this->state = 2;
    ss->cancel();
    schedule_timer->cancel();
    ic.stop();
    schedule_ic.stop();
    ss->close();
    delete iw;
    delete schedule_iw;
    delete schedule_timer;
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
                               receive();
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
                                       logger::ERROR << "invalid dns request: parse error" << END;
                                       end_session(session);
                                   }
                               } else {
                                   logger::ERROR << "invalid dns request: net error" << END;
                                   end_session(session);
                               }
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
                              if (writeError) {
                                  logger::ERROR << "send response failed!" << "error:" << writeError.message()
                                               << "code:" << writeError.value() << END;
                              } else {
                                  logger::DEBUG << "send response success!" << "size:" << writeSize
                                               << "expected:" << udpResponse->len << END;
                              }
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

    // 检查强制解析规则
    for (auto rule : config.force_resolve_rules) {
        if (rule->match(host)) {
            dns_record &record = session->record;
            record.server = "st-dns-force";
            record.domain = host;
            record.ips = rule->ips;
            record.expire_time = time::now() / 1000 + 60;
            record.match_area = true;
            session->logger.add_dimension("process_type", "force");
            logger::INFO << "force resolve" << host << st::utils::ipv4::ips_to_str(rule->ips) << END;
            complete(session);
            return;
        }
    }

    if (host == "localhost") {
        dns_record &record = session->record;
        record.server = "st-dns";
        record.domain = host;
        record.expire_time = time::now() / 1000 + 60;
        session->logger.add_dimension("process_type", "local");
        complete(session);
    } else {
        dns_record record = query_record_from_cache(host);
        if (record.ips.empty()) {
            session->logger.add_dimension("process_type", "remote");
            auto *timer = new deadline_timer(ic);
            timer->expires_from_now(boost::posix_time::milliseconds(100));
            timer->async_wait([=](boost::system::error_code ec) {
                dns_record record = query_record_from_cache(host);
                session->record = record;
                complete(session);
                delete timer;
            });
            sync_dns_record_from_remote(host);
        } else {
            session->logger.add_dimension("process_type", "cache");
            session->record = record;
            complete(session);
            if (record.expire || !record.match_area) {
                sync_dns_record_from_remote(host);
            } else {
                sync_loss_dns_record_from_remote(host, record);
            }
        }
    }
}
dns_record dns_server::query_record_from_cache(const string &host) const {
    auto record = dns_record_manager::uniq().resolve(host);
    if (record.ips.empty()) {
        string fiDomain = dns_domain::getFIDomain(host);
        if (fiDomain == "LAN" || fiDomain == "lan") {
            record = dns_record_manager::uniq().resolve(dns_domain::removeFIDomain(host));
            record.domain = host;
        }
    }
    return record;
}

void dns_server::forward_dns_request(session *session, const std::function<void(st::dns::session *session)> &complete_handler) {

    remote_dns_server *server = nullptr;
    for (auto &it : config.servers) {
        if (it->type == "UDP") {
            server = it;
            break;
        }
    }
    if (server != nullptr) {
        dns_client::uniq().forward_udp(session->request, server->ip, server->port, server->timeout, [=](udp_response *response) {
            session->response = response;
            complete_handler(session);
        });
    } else {
        complete_handler(session);
    }
}
void dns_server::sync_dns_record_from_remote(const string &domain) {
    logger::DEBUG << "begin update dns record !" << domain << END;
    vector<remote_dns_server *> servers = remote_dns_server::select_servers(domain, config.servers);
    if (servers.empty()) {
        logger::ERROR << domain << "update dns record cal servers empty!" << END;
        return;
    } else {
        uint32_t priority = MAX_PRIORITY;
        for (auto server : servers) {
            st::task::priority_task<task_queue_param> task(make_pair(domain, server), priority, domain + "/" + server->id());
            sync_remote_record_task_queue.submit(task);
            priority--;
        }
    }
}

void dns_server::sync_loss_dns_record_from_remote(string &host, dns_record &record) {
    vector<remote_dns_server *> servers = remote_dns_server::select_servers(host, config.servers);
    uint32_t priority = MAX_PRIORITY;
    for (auto &server : servers) {
        if (record.servers.find(server->id()) == record.servers.end()) {
            task::priority_task<task_queue_param> task(make_pair(host, server), priority, host + "/" + server->id());
            sync_remote_record_task_queue.submit(task);
            apm_logger::perf("st-dns-sync-loss_record", {{"domain", host}, {"server", server->id()}}, 0);
            priority--;
        }
    }
}

void dns_server::sync_dns_record_from_remote(const string &host, const std::function<void(dns_record record)> &complete, remote_dns_server *server) const {
    auto begin = time::now();
    dns_multi_area_complete multi_area_complete_handler = [=](const vector<uint32_t> &ips, bool load_all) {
        apm_logger::perf("st-dns-sync-record-from-remote", {{"domain", host}, {"server", server->id()}}, time::now() - begin);
        if (!ips.empty()) {
            dns_record_manager::uniq().add(host, ips, server->id(), load_all ? server->dns_cache_expire : server->dns_cache_expire / 2);
        }
        dns_record record = dns_record_manager::uniq().resolve(host);
        complete(record);
    };

    if (server->type == "TCP_SSL") {
        dns_client::uniq().tcp_tls_dns(host, server->ip, server->port, server->timeout, server->resolve_optimize_areas, multi_area_complete_handler);
    } else if (server->type == "TCP") {
        dns_client::uniq().tcp_dns(host, server->ip, server->port, server->timeout, server->resolve_optimize_areas, multi_area_complete_handler);
    } else if (server->type == "UDP") {
        dns_client::uniq().udp_dns(host, server->ip, server->port, server->timeout, [=](const std::vector<uint32_t> &ips) {
            multi_area_complete_handler(ips, true);
        });
    }
}
void dns_server::schedule() {
    for (auto &server : config.servers) {
        if (std::find(server->areas.begin(), server->areas.end(), "LAN") == server->areas.end()) {
            vector<pair<string, uint16_t>> result;
            for (const auto &area : st::command::proxy::get_ip_available_proxy_areas(server->ip)) {
                if (areaip::manager::is_match_areas(server->areas, area)) {
                    uint16_t a_port = st::command::proxy::register_area_port(server->ip, server->port, area);
                    if (a_port > 0) {
                        result.emplace_back(area, a_port);
                        logger::INFO << server->id() << "resolve optimize areas area sync" << area << a_port << END;
                    }
                }
                st::areaip::manager::uniq().load_area_ips(area);
            }
            server->resolve_optimize_areas = result;
        }
    }
    schedule_timer->expires_from_now(boost::posix_time::seconds(5));
    schedule_timer->async_wait([=](boost::system::error_code ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        schedule();
    });
}
