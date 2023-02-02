//
// Created by codingdie on 2020/5/20.
//

#include "dns_client.h"
#include "st.h"
#include <sys/socket.h>
#include <openssl/ssl.h>
#include "command/proxy_command.h"
using namespace st::dns::protocol;
template<typename Result>
bool dns_client::is_timeout_or_error(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(Result)> complete_handler, Result defaultV) {
    uint64_t cost = time::now() - beginTime;
    if (!ec && cost <= timeout) {
        return false;
    }
    if (ec && ec != boost::asio::error::operation_aborted) {
        logger::ERROR << logTag << "error! cost:" << cost << ec.message() << END;
    } else {
        logger::ERROR << logTag << "timout! cost" << cost << END;
    }
    complete_handler(defaultV);
    return true;
}

bool dns_client::is_timeout_or_error(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, const dns_complete &complete_handler) {
    return is_timeout_or_error(logTag, ec, beginTime, timeout, complete_handler, {});
}
bool dns_client::is_timeout_or_error(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, const std::function<void(protocol::udp_response *res)> &complete_handler) {
    udp_response *res = nullptr;
    return is_timeout_or_error(logTag, ec, beginTime, timeout, complete_handler, res);
}
void dns_client::udp_dns(const string &domain, const std::string &dns_server, uint32_t port, uint64_t timeout, const dns_complete &complete_handler) {
    vector<string> domains;
    domains.emplace_back(domain);
    if (domains.empty()) {
        return complete_handler({});
    }
    uint64_t beginTime = time::now();
    auto *socket = new udp::socket(ic, udp::endpoint(udp::v4(), 0));
    udp_request dns_request(domains);
    unsigned short qid = dns_request.header->id;
    uint16_t dnsId = dns_request.header->id;
    string logTag = to_string(dnsId) + " udp_dns " + dns_server + " " + domains[0];
    auto *timeoutTimer = new deadline_timer(ic);
    std::function<void(std::vector<uint32_t> ips)> complete = [=](std::vector<uint32_t> ips) {
        delete timeoutTimer;
        if (!ips.empty()) {
            logger::DEBUG << logTag << "cost" << time::now() - beginTime << "resolve ips" << st::utils::ipv4::ips_to_str(ips) << END;
        }
        complete_handler(ips);
    };
    timeoutTimer->expires_from_now(boost::posix_time::milliseconds(timeout));
    timeoutTimer->async_wait([=](boost::system::error_code ec) {
        socket->shutdown(boost::asio::socket_base::shutdown_both, ec);
        socket->cancel(ec);
        socket->close(ec);
        ic.post([=]() {
            delete socket;
        });
    });
    socket->async_send_to(buffer(dns_request.data, dns_request.len),
                          udp::endpoint(make_address_v4(dns_server), port), [=](boost::system::error_code error, size_t size) {
                              if (!is_timeout_or_error(logTag, error, beginTime, timeout, complete)) {
                                  auto *dnsResponse = new udp_response(1024);
                                  auto *serverEndpoint = new udp::endpoint();
                                  socket->async_receive_from(buffer(dnsResponse->data, sizeof(uint8_t) * 1024), *serverEndpoint,
                                                             [=](boost::system::error_code error, size_t size) {
                                                                 if (!is_timeout_or_error(logTag, error, beginTime, timeout, complete)) {
                                                                     dnsResponse->parse(size);
                                                                     if (dnsResponse->is_valid() && dnsResponse->header->id == qid) {
                                                                         complete(dnsResponse->ips);
                                                                     } else {
                                                                         complete({});
                                                                     }
                                                                 }
                                                                 delete dnsResponse;
                                                                 delete serverEndpoint;
                                                             });
                              }
                          });
}

void dns_client::tcp_tls_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const string &area, const dns_complete &complete_handler) {
    if (!area.empty()) {
        auto area_port = st::command::proxy::register_area_port(st::utils::ipv4::str_to_ip(dns_server), port, area);
        if (area_port != 0) {
            port = area_port;
        }
    }
    vector<string> domains;
    domains.emplace_back(domain);
    uint64_t beginTime = time::now();
    tcp::endpoint server_endpoint(make_address_v4(dns_server), port);
    auto *dns_request = new tcp_request(domains);
    uint16_t dnsId = dns_request->header->id;
    string log_tag = to_string(dnsId) + " " + dns_server + " tcp_tls_dns " + domains[0] + (!area.empty() ? " " + area : "");
    auto *socket = new boost::asio::ssl::stream<tcp::socket>(ic, ssl_ctx);
    socket->set_verify_mode(ssl::verify_none);
    boost::system::error_code ec;
    socket->next_layer().open(server_endpoint.protocol(), ec);
#ifdef TCP_FASTOPEN_CONNECT
    using fastopen_connect = boost::asio::detail::socket_option::boolean<IPPROTO_TCP, TCP_FASTOPEN_CONNECT>;
    socket->next_layer().set_option(fastopen_connect(true), ec);
#endif// TCP_FASTOPEN_CONNECT
    socket->next_layer().set_option(tcp::no_delay(true));
    socket->next_layer().set_option(boost::asio::socket_base::keep_alive(true));
    pair<uint8_t *, uint32_t> data_bytes = st::mem::pmalloc(2048);
    pair<uint8_t *, uint32_t> length_bytes = st::mem::pmalloc(2);
    auto *timer = new deadline_timer(ic);
    std::function<void(std::vector<uint32_t> ips)> complete =
            [=](const std::vector<uint32_t> &ips) {
                timer->cancel();
                delete timer;
                if (!ips.empty()) {
                    logger::DEBUG << log_tag << "cost" << time::now() - beginTime << "resolve ips" << st::utils::ipv4::ips_to_str(ips) << END;
                }
                complete_handler(ips);
            };
    timer->expires_from_now(boost::posix_time::milliseconds(timeout));
    timer->async_wait([=](boost::system::error_code ec) {
        socket->async_shutdown([=](boost::system::error_code ec) {
            socket->lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, ec);
            socket->lowest_layer().cancel(ec);
            socket->lowest_layer().close(ec);
            ic.post([=]() {
                st::mem::pfree(data_bytes);
                st::mem::pfree(length_bytes);
                delete dns_request;
                delete socket;
            });
        });
    });
    socket->lowest_layer().async_connect(
            server_endpoint,
            [=](boost::system::error_code ec) {
                if (!is_timeout_or_error(log_tag, ec, beginTime, timeout, complete)) {
                    //                    init_ssl_session(dns_server, socket);
                    socket->async_handshake(
                            boost::asio::ssl::stream_base::client,
                            [=](boost::system::error_code ec) {
                                if (!is_timeout_or_error(log_tag, ec, beginTime, timeout, complete)) {
                                    //                                    if (save_ssl_session(dns_server, socket)) {
                                    //                                        logger::DEBUG << log_tag << "not reused ssl!" << END;
                                    //                                    } else {
                                    //                                        logger::DEBUG << log_tag << "reused ssl!" << END;
                                    //                                    }
                                    st::utils::copy(dns_request->data, data_bytes.first, 0, 0, dns_request->len);
                                    boost::asio::async_write(
                                            *socket,
                                            buffer(data_bytes.first, dns_request->len),
                                            [=](boost::system::error_code ec, std::size_t length) {
                                                if (!is_timeout_or_error(log_tag, ec, beginTime, timeout, complete)) {
                                                    boost::asio::async_read(
                                                            *socket,
                                                            buffer(length_bytes.first, 2),
                                                            [=](boost::system::error_code ec, std::size_t length) {
                                                                if (!is_timeout_or_error(log_tag, ec, beginTime, timeout, complete)) {
                                                                    uint16_t dataLen = 0;
                                                                    st::utils::read(length_bytes.first, dataLen);
                                                                    if (dataLen > 1024) {
                                                                        complete({});
                                                                    } else {
                                                                        boost::asio::async_read(
                                                                                *socket,
                                                                                buffer(data_bytes.first, dataLen),
                                                                                [=](boost::system::error_code ec, std::size_t length) {
                                                                                    if (!is_timeout_or_error(log_tag, ec, beginTime, timeout, complete)) {
                                                                                        complete(parse(length, length_bytes, data_bytes, dnsId));
                                                                                    }
                                                                                });
                                                                    }
                                                                }
                                                            });
                                                }
                                            });
                                }
                            });
                }
            });
}


void dns_client::tcp_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const string &area, const dns_complete &complete_handler) {
    if (!area.empty()) {
        auto area_port = st::command::proxy::register_area_port(st::utils::ipv4::str_to_ip(dns_server), port, area);
        if (area_port != 0) {
            port = area_port;
        }
    }
    vector<string> domains;
    domains.emplace_back(domain);
    uint64_t begin = time::now();
    tcp::endpoint server_endpoint(make_address_v4(dns_server), port);
    auto *dns_request = new tcp_request(domains);
    uint16_t dnsId = dns_request->header->id;
    string logTag = to_string(dnsId) + " tcp_dns " + dns_server + " " + domains[0];
    auto *socket = new tcp::socket(ic);
    boost::system::error_code ec;
    socket->open(server_endpoint.protocol(), ec);
#ifdef TCP_FASTOPEN_CONNECT
    using fastopen_connect = boost::asio::detail::socket_option::boolean<IPPROTO_TCP, TCP_FASTOPEN_CONNECT>;
    socket->set_option(fastopen_connect(true), ec);
#endif// TCP_FASTOPEN_CONNECT
    socket->set_option(tcp::no_delay(true));
    socket->set_option(boost::asio::socket_base::keep_alive(true));
    pair<uint8_t *, uint32_t> dataBytes = st::mem::pmalloc(2048);
    pair<uint8_t *, uint32_t> lengthBytes = st::mem::pmalloc(2);
    auto *timer = new deadline_timer(ic);
    std::function<void(std::vector<uint32_t> ips)> complete =
            [=](const std::vector<uint32_t> &ips) {
                delete timer;
                if (!ips.empty()) {
                    logger::DEBUG << logTag << "cost" << time::now() - begin << "resolve ips" << st::utils::ipv4::ips_to_str(ips) << END;
                }
                complete_handler(ips);
            };
    timer->expires_from_now(boost::posix_time::milliseconds(timeout));
    timer->async_wait([=](boost::system::error_code ec) {
        socket->lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, ec);
        socket->lowest_layer().cancel(ec);
        socket->lowest_layer().close(ec);
        ic.post([=]() {
            st::mem::pfree(dataBytes);
            st::mem::pfree(lengthBytes);
            delete dns_request;
            delete socket;
        });
    });

    socket->async_connect(
            server_endpoint,
            [=](boost::system::error_code ec) {
                if (!is_timeout_or_error(logTag, ec, begin, timeout, complete)) {
                    st::utils::copy(dns_request->data, dataBytes.first, 0, 0, dns_request->len);
                    boost::asio::async_write(
                            *socket,
                            buffer(dataBytes.first, dns_request->len),
                            [=](boost::system::error_code ec, std::size_t length) {
                                if (!is_timeout_or_error(logTag, ec, begin, timeout, complete)) {
                                    boost::asio::async_read(
                                            *socket,
                                            buffer(lengthBytes.first, 2),
                                            [=](boost::system::error_code ec, std::size_t length) {
                                                if (!is_timeout_or_error(logTag, ec, begin, timeout, complete)) {
                                                    uint16_t dataLen = 0;
                                                    st::utils::read(lengthBytes.first, dataLen);
                                                    if (dataLen > 1024) {
                                                        complete({});
                                                    } else {
                                                        boost::asio::async_read(
                                                                *socket,
                                                                buffer(dataBytes.first, dataLen),
                                                                [=](boost::system::error_code ec, std::size_t length) {
                                                                    if (!is_timeout_or_error(logTag, ec, begin, timeout, complete)) {
                                                                        complete(parse(length, lengthBytes, dataBytes, dnsId));
                                                                    }
                                                                });
                                                    }
                                                }
                                            });
                                }
                            });
                }
            });
}


void dns_client::tcp_tls_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const unordered_set<string> &areas, const dns_multi_area_complete &complete_handler) {
    if (areas.empty()) {
        tcp_tls_dns(domain, dns_server, port, timeout, [=](const std::vector<uint32_t> &ips) {
            complete_handler(ips, true);
        });
    } else if (areas.size() == 1) {
        tcp_tls_dns(domain, dns_server, port, timeout, *areas.begin(), [=](const std::vector<uint32_t> &ips) {
            complete_handler(ips, true);
        });
    } else {
        auto *counter = new atomic_uint16_t(0);
        auto *load_all = new atomic_bool(true);
        auto *result = new std::unordered_set<uint32_t>();
        std::function<void(std::vector<uint32_t> ips)> eachHandler = [=](std::vector<uint32_t> ips) {
            counter->fetch_add(1);
            if (ips.size() == 0) {
                *load_all = false;
            }
            for (auto ip : ips) {
                result->emplace(ip);
            }
            if (counter->load() >= areas.size()) {
                std::vector<uint32_t> s(result->begin(), result->end());
                complete_handler(s, load_all->load());
                delete result;
                delete counter;
                delete load_all;
            }
        };
        for (const auto &area : areas) {
            tcp_tls_dns(domain, dns_server, port, timeout, area, eachHandler);
        }
    }
}

void dns_client::tcp_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const unordered_set<string> &areas, const dns_multi_area_complete &complete_handler) {
    if (areas.empty()) {
        tcp_dns(domain, dns_server, port, timeout, [=](const std::vector<uint32_t> &ips) {
            complete_handler(ips, true);
        });
    } else if (areas.size() == 1) {
        tcp_dns(domain, dns_server, port, timeout, *areas.begin(), [=](const std::vector<uint32_t> &ips) {
            complete_handler(ips, true);
        });
    } else {
        auto *load_all = new atomic_bool(true);
        auto *counter = new atomic_uint16_t(0);
        auto *result = new std::vector<uint32_t>();
        std::function<void(std::vector<uint32_t> ips)> each_handler = [=](const std::vector<uint32_t> &ips) {
            counter->fetch_add(1);
            for (auto ip : ips) {
                result->emplace_back(ip);
            }
            if (ips.size() == 0) {
                *load_all = false;
            }
            if (counter->load() == areas.size()) {
                complete_handler(*result, load_all->load());
                delete result;
                delete counter;
                delete load_all;
            }
        };
        for (const string &area : areas) {
            tcp_dns(domain, dns_server, port, timeout, area, each_handler);
        }
    }
}

void dns_client::tcp_tls_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const dns_complete &complete_handler) {
    tcp_tls_dns(domain, dns_server, port, timeout, "", complete_handler);
}

void dns_client::tcp_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const dns_complete &complete_handler) {
    tcp_dns(domain, dns_server, port, timeout, "", complete_handler);
}


void dns_client::forward_udp(udp_request &udpdns_request, const std::string &dns_server, uint32_t port, uint64_t timeout, std::function<void(udp_response *)> callback) {
    uint64_t beginTime = time::now();
    auto *socket = new udp::socket(ic, udp::endpoint(udp::v4(), 0));
    string logTag = "forward_udp to " + dns_server;
    auto *timer = new deadline_timer(ic);
    std::function<void(udp_response *)> complete = [=](udp_response *res) {
        delete timer;
        if (res != nullptr) {
            logger::INFO << logTag << "sucess!"
                         << "cost" << time::now() - beginTime << END;
        }
        callback(res);
    };
    timer->expires_from_now(boost::posix_time::milliseconds(timeout));
    timer->async_wait([=](boost::system::error_code ec) {
        socket->shutdown(boost::asio::socket_base::shutdown_both, ec);
        socket->cancel(ec);
        socket->close(ec);
        ic.post([=]() {
            delete socket;
        });
    });
    socket->async_send_to(buffer(udpdns_request.data, udpdns_request.len),
                          udp::endpoint(make_address_v4(dns_server), port),
                          [=](boost::system::error_code error, size_t size) {
                              if (!is_timeout_or_error(logTag, error, beginTime, timeout, complete)) {
                                  auto *dnsResponse = new udp_response(1024);
                                  auto *serverEndpoint = new udp::endpoint();
                                  socket->async_receive_from(
                                          buffer(dnsResponse->data, sizeof(uint8_t) * 1024),
                                          *serverEndpoint,
                                          [=](boost::system::error_code error, size_t size) {
                                              delete serverEndpoint;
                                              if (!is_timeout_or_error(logTag, error, beginTime, timeout, complete) && size > 0) {
                                                  dnsResponse->len = size;
                                                  complete(dnsResponse);
                                              } else {
                                                  delete dnsResponse;
                                              }
                                          });
                              }
                          });
}


std::vector<uint32_t> dns_client::parse(uint16_t length, pair<uint8_t *, uint32_t> lengthBytes, pair<uint8_t *, uint32_t> dataBytes, uint16_t dnsId) {
    udp_response *dnsResponse = nullptr;
    if (length > 0 && length <= 1024) {
        dnsResponse = new udp_response(dataBytes.first, length);
        dnsResponse->parse(length);
        if (!dnsResponse->is_valid()) {
            logger::ERROR << dnsId << "receive unmarketable data" << END;
        } else {
            if (dnsResponse->header->id != dnsId) {
                logger::ERROR << dnsId << "receive not valid header id" << END;
                dnsResponse->mark_invalid();
            }
            if (dnsResponse->header->responseCode != 0) {
                logger::ERROR << dnsId << "receive error responseCode"
                              << dnsResponse->header->responseCode << END;
                dnsResponse->mark_invalid();
            }
        }
    } else {
        logger::ERROR << dnsId << "receive unmarketable data" << END;
    }

    vector<uint32_t> ips;
    if (dnsResponse != nullptr && dnsResponse->is_valid()) {
        ips = dnsResponse->ips;
    }
    delete dnsResponse;
    return ips;
}


dns_client::~dns_client() {
    ic.stop();
    delete iw;
    th->join();
    delete th;
}

dns_client::dns_client() : ic(), ssl_ctx(boost::asio::ssl::context::sslv23_client) {
    iw = new boost::asio::io_context::work(ic);
    th = new thread([=]() {
        this->ic.run();
    });
}
dns_client &dns_client::uniq() {
    static dns_client ds;
    return ds;
}
//boost::asio::ssl::context &dns_client::get_context(const string &server) {
//    boost::asio::ssl::context *ctx = nullptr;
//
//    if (contexts.find(server) != contexts.end()) {
//        ctx = contexts.at(server);
//    } else {
//        ctx = new boost::asio::ssl::context(boost::asio::ssl::context::sslv23_client);
//        SSL_CTX_set_session_cache_mode(ctx->native_handle(), SSL_SESS_CACHE_CLIENT);
//        SSL_CTX_set_options(ctx->native_handle(), SSL_OP_NO_TICKET);
//        contexts[server] = ctx;
//    }
//    return *ctx;
//}
//void dns_client::init_ssl_session(const string &server, ssl::stream<tcp::socket> *socket) {
//    if (sessions.find(server) != sessions.end()) {
//        SSL_get1_session(socket->native_handle());
//        auto session = sessions.at(server);
//        if (session) {
//            SSL_set_session(socket->native_handle(), session);
//        }
//    }
//}
//bool dns_client::save_ssl_session(const string &server, boost::asio::ssl::stream<tcp::socket> *socket) {
//    if (!SSL_session_reused(socket->native_handle())) {
//        if (sessions.find(server) != sessions.end()) {
//            auto session = sessions.at(server);
//            SSL_SESSION_free(session);
//            sessions[server] = nullptr;
//        }
//        sessions[server] = SSL_get1_session(socket->native_handle());
//        return true;
//    } else {
//        return false;
//    }
//}
