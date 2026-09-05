//
// Created by codingdie on 2020/5/20.
//

#include "dns_client.h"
#include "st.h"
#include <array>
#include <memory>
#include <sys/socket.h>
#include <openssl/ssl.h>
using namespace st::dns::protocol;

namespace {
    struct udp_dns_state {
        std::shared_ptr<udp::socket> socket;
        std::shared_ptr<deadline_timer> timer;
        std::shared_ptr<udp_request> request;
        std::shared_ptr<udp_response> response;
        udp::endpoint server_endpoint;
        std::atomic_bool completed;

        udp_dns_state() : completed(false) {}
    };

    struct tcp_dns_state {
        std::shared_ptr<tcp::socket> socket;
        std::shared_ptr<deadline_timer> timer;
        std::shared_ptr<tcp_request> request;
        std::array<uint8_t, 2048> data_bytes{};
        std::array<uint8_t, 2> length_bytes{};
        std::atomic_bool completed;

        tcp_dns_state() : completed(false) {}
    };

    struct tcp_tls_dns_state {
        std::shared_ptr<boost::asio::ssl::stream<tcp::socket>> socket;
        std::shared_ptr<deadline_timer> timer;
        std::shared_ptr<tcp_request> request;
        std::array<uint8_t, 2048> data_bytes{};
        std::array<uint8_t, 2> length_bytes{};
        std::atomic_bool completed;

        tcp_tls_dns_state() : completed(false) {}
    };

    struct forward_udp_state {
        std::shared_ptr<udp::socket> socket;
        std::shared_ptr<deadline_timer> timer;
        std::vector<uint8_t> request_data;
        std::unique_ptr<udp_response> response;
        udp::endpoint server_endpoint;
        std::atomic_bool completed;

        forward_udp_state() : completed(false) {}
    };
}

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
    auto state = std::make_shared<udp_dns_state>();
    state->socket = std::make_shared<udp::socket>(ic, udp::endpoint(udp::v4(), 0));
    state->timer = std::make_shared<deadline_timer>(ic);
    state->request = std::make_shared<udp_request>(domains);
    unsigned short qid = state->request->header->id;
    uint16_t dnsId = state->request->header->id;
    string logTag = to_string(dnsId) + " udp_dns " + dns_server + " " + domains[0];
    std::function<void(const std::vector<uint32_t> &)> complete = [state, complete_handler, logTag, beginTime](const std::vector<uint32_t> &ips) {
        if (state->completed.exchange(true)) {
            return;
        }
        boost::system::error_code ignored_ec;
        state->timer->cancel(ignored_ec);
        state->socket->cancel(ignored_ec);
        state->socket->close(ignored_ec);
        if (!ips.empty()) {
            logger::DEBUG << logTag << "cost" << time::now() - beginTime << "resolve ips" << st::utils::ipv4::ips_to_str(ips) << END;
        }
        complete_handler(ips);
    };
    state->timer->expires_from_now(boost::posix_time::milliseconds(timeout));
    state->timer->async_wait([complete](boost::system::error_code ec) {
        if (!ec) {
            complete({});
        }
    });
    state->socket->async_send_to(buffer(state->request->data, state->request->len),
                          udp::endpoint(make_address_v4(dns_server), port), [this, state, complete, logTag, beginTime, timeout, qid](boost::system::error_code error, size_t size) {
                              if (!is_timeout_or_error(logTag, error, beginTime, timeout, complete)) {
                                  state->response = std::make_shared<udp_response>(1024);
                                  state->socket->async_receive_from(buffer(state->response->data, sizeof(uint8_t) * 1024), state->server_endpoint,
                                                             [this, state, complete, logTag, beginTime, timeout, qid](boost::system::error_code error, size_t size) {
                                                                 if (!is_timeout_or_error(logTag, error, beginTime, timeout, complete)) {
                                                                     state->response->parse(size);
                                                                     if (state->response->is_valid() && state->response->header->id == qid) {
                                                                         complete(state->response->ips);
                                                                     } else {
                                                                         complete({});
                                                                     }
                                                                 }
                                                             });
                              }
                          });
}

void dns_client::tcp_tls_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, pair<string, uint16_t> area, const dns_complete &complete_handler) {
    auto o_port = port;
    if (area.second > 0) {
        port = area.second;
    }
    vector<string> domains;
    domains.emplace_back(domain);
    uint64_t beginTime = time::now();
    tcp::endpoint server_endpoint(make_address_v4(dns_server), port);
    auto state = std::make_shared<tcp_tls_dns_state>();
    state->request = std::make_shared<tcp_request>(domains);
    uint16_t dnsId = state->request->header->id;
    string log_tag = to_string(dnsId) + " tcp_tls_dns " + dns_server + ":" + to_string(o_port) + " " + domains[0] + (area.second == 0 ? "" : " " + area.first + "/" + to_string(area.second));

    state->socket = std::make_shared<boost::asio::ssl::stream<tcp::socket>>(ic, *ssl_ctx);
    state->timer = std::make_shared<deadline_timer>(ic);
    state->socket->set_verify_mode(ssl::verify_none);
    boost::system::error_code ec;
    state->socket->next_layer().open(server_endpoint.protocol(), ec);
#ifdef TCP_FASTOPEN_CONNECT
    using fastopen_connect = boost::asio::detail::socket_option::boolean<IPPROTO_TCP, TCP_FASTOPEN_CONNECT>;
    state->socket->next_layer().set_option(fastopen_connect(true), ec);
#endif// TCP_FASTOPEN_CONNECT
    state->socket->next_layer().set_option(tcp::no_delay(true));
    state->socket->next_layer().set_option(boost::asio::socket_base::keep_alive(true));
    std::function<void(const std::vector<uint32_t> &)> complete =
            [state, complete_handler, log_tag, beginTime](const std::vector<uint32_t> &ips) {
                if (state->completed.exchange(true)) {
                    return;
                }
                boost::system::error_code ignored_ec;
                state->timer->cancel(ignored_ec);
                state->socket->lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, ignored_ec);
                state->socket->lowest_layer().cancel(ignored_ec);
                state->socket->lowest_layer().close(ignored_ec);
                if (!ips.empty()) {
                    logger::DEBUG << log_tag << "cost" << time::now() - beginTime << "resolve ips" << st::utils::ipv4::ips_to_str(ips) << END;
                }
                complete_handler(ips);
            };
    state->timer->expires_from_now(boost::posix_time::milliseconds(timeout));
    state->timer->async_wait([complete](boost::system::error_code ec) {
        if (!ec) {
            complete({});
        }
    });
    state->socket->lowest_layer().async_connect(
            server_endpoint,
            [this, state, complete, log_tag, beginTime, timeout, dnsId](boost::system::error_code ec) {
                if (!is_timeout_or_error(log_tag, ec, beginTime, timeout, complete)) {
                    //                    init_ssl_session(dns_server, socket);
                    state->socket->async_handshake(
                            boost::asio::ssl::stream_base::client,
                            [this, state, complete, log_tag, beginTime, timeout, dnsId](boost::system::error_code ec) {
                                if (!is_timeout_or_error(log_tag, ec, beginTime, timeout, complete)) {
                                    //                                    if (save_ssl_session(dns_server, socket)) {
                                    //                                        logger::DEBUG << log_tag << "not reused ssl!" << END;
                                    //                                    } else {
                                    //                                        logger::DEBUG << log_tag << "reused ssl!" << END;
                                    //                                    }
                                    st::utils::copy(state->request->data, state->data_bytes.data(), 0, 0, state->request->len);
                                    boost::asio::async_write(
                                            *state->socket,
                                            buffer(state->data_bytes.data(), state->request->len),
                                            [this, state, complete, log_tag, beginTime, timeout, dnsId](boost::system::error_code ec, std::size_t length) {
                                                if (!is_timeout_or_error(log_tag, ec, beginTime, timeout, complete)) {
                                                    boost::asio::async_read(
                                                            *state->socket,
                                                            buffer(state->length_bytes.data(), state->length_bytes.size()),
                                                            [this, state, complete, log_tag, beginTime, timeout, dnsId](boost::system::error_code ec, std::size_t length) {
                                                                if (!is_timeout_or_error(log_tag, ec, beginTime, timeout, complete)) {
                                                                    uint16_t dataLen = 0;
                                                                    st::utils::read(state->length_bytes.data(), dataLen);
                                                                    if (dataLen > 1024) {
                                                                        complete({});
                                                                    } else {
                                                                        boost::asio::async_read(
                                                                                *state->socket,
                                                                                buffer(state->data_bytes.data(), dataLen),
                                                                                [this, state, complete, log_tag, beginTime, timeout, dnsId](boost::system::error_code ec, std::size_t length) {
                                                                                    if (!is_timeout_or_error(log_tag, ec, beginTime, timeout, complete)) {
                                                                                        complete(parse(length,
                                                                                                       make_pair(state->length_bytes.data(), static_cast<uint32_t>(state->length_bytes.size())),
                                                                                                       make_pair(state->data_bytes.data(), static_cast<uint32_t>(state->data_bytes.size())),
                                                                                                       dnsId));
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


void dns_client::tcp_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, pair<string, uint16_t> area, const dns_complete &complete_handler) {
    auto o_port = port;
    if (area.second > 0) {
        port = area.second;
    }
    vector<string> domains;
    domains.emplace_back(domain);
    uint64_t begin = time::now();
    tcp::endpoint server_endpoint(make_address_v4(dns_server), port);
    auto state = std::make_shared<tcp_dns_state>();
    state->request = std::make_shared<tcp_request>(domains);
    uint16_t dnsId = state->request->header->id;
    string logTag = to_string(dnsId) + " tcp_dns " + dns_server + ":" + to_string(o_port) + " " + domains[0] + (area.second == 0 ? "" : " " + area.first + "/" + to_string(area.second));
    state->socket = std::make_shared<tcp::socket>(ic);
    state->timer = std::make_shared<deadline_timer>(ic);
    boost::system::error_code ec;
    state->socket->open(server_endpoint.protocol(), ec);
#ifdef TCP_FASTOPEN_CONNECT
    using fastopen_connect = boost::asio::detail::socket_option::boolean<IPPROTO_TCP, TCP_FASTOPEN_CONNECT>;
    state->socket->set_option(fastopen_connect(true), ec);
#endif// TCP_FASTOPEN_CONNECT
    state->socket->set_option(tcp::no_delay(true));
    state->socket->set_option(boost::asio::socket_base::keep_alive(true));
    std::function<void(const std::vector<uint32_t> &)> complete =
            [state, complete_handler, logTag, begin](const std::vector<uint32_t> &ips) {
                if (state->completed.exchange(true)) {
                    return;
                }
                boost::system::error_code ignored_ec;
                state->timer->cancel(ignored_ec);
                state->socket->shutdown(boost::asio::socket_base::shutdown_both, ignored_ec);
                state->socket->cancel(ignored_ec);
                state->socket->close(ignored_ec);
                if (!ips.empty()) {
                    logger::DEBUG << logTag << "cost" << time::now() - begin << "resolve ips" << st::utils::ipv4::ips_to_str(ips) << END;
                }
                complete_handler(ips);
            };
    state->timer->expires_from_now(boost::posix_time::milliseconds(timeout));
    state->timer->async_wait([complete](boost::system::error_code ec) {
        if (!ec) {
            complete({});
        }
    });

    state->socket->async_connect(
            server_endpoint,
            [this, state, complete, logTag, begin, timeout, dnsId](boost::system::error_code ec) {
                if (!is_timeout_or_error(logTag, ec, begin, timeout, complete)) {
                    st::utils::copy(state->request->data, state->data_bytes.data(), 0, 0, state->request->len);
                    boost::asio::async_write(
                            *state->socket,
                            buffer(state->data_bytes.data(), state->request->len),
                            [this, state, complete, logTag, begin, timeout, dnsId](boost::system::error_code ec, std::size_t length) {
                                if (!is_timeout_or_error(logTag, ec, begin, timeout, complete)) {
                                    boost::asio::async_read(
                                            *state->socket,
                                            buffer(state->length_bytes.data(), state->length_bytes.size()),
                                            [this, state, complete, logTag, begin, timeout, dnsId](boost::system::error_code ec, std::size_t length) {
                                                if (!is_timeout_or_error(logTag, ec, begin, timeout, complete)) {
                                                    uint16_t dataLen = 0;
                                                    st::utils::read(state->length_bytes.data(), dataLen);
                                                    if (dataLen > 1024) {
                                                        complete({});
                                                    } else {
                                                        boost::asio::async_read(
                                                                *state->socket,
                                                                buffer(state->data_bytes.data(), dataLen),
                                                                [this, state, complete, logTag, begin, timeout, dnsId](boost::system::error_code ec, std::size_t length) {
                                                                    if (!is_timeout_or_error(logTag, ec, begin, timeout, complete)) {
                                                                        complete(parse(length,
                                                                                       make_pair(state->length_bytes.data(), static_cast<uint32_t>(state->length_bytes.size())),
                                                                                       make_pair(state->data_bytes.data(), static_cast<uint32_t>(state->data_bytes.size())),
                                                                                       dnsId));
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


void dns_client::tcp_tls_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const vector<pair<string, uint16_t>> &areas, const dns_multi_area_complete &complete_handler) {
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

void dns_client::tcp_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const vector<pair<string, uint16_t>> &areas, const dns_multi_area_complete &complete_handler) {
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
        auto *result = new std::set<uint32_t>();
        std::function<void(std::vector<uint32_t> ips)> each_handler = [=](const std::vector<uint32_t> &ips) {
            counter->fetch_add(1);
            for (auto ip : ips) {
                result->emplace(ip);
            }
            if (ips.size() == 0) {
                *load_all = false;
            }
            if (counter->load() == areas.size()) {
                vector<uint32_t> ips(result->begin(), result->end());
                complete_handler(ips, load_all->load());
                delete result;
                delete counter;
                delete load_all;
            }
        };
        for (const auto &area : areas) {
            tcp_dns(domain, dns_server, port, timeout, area, each_handler);
        }
    }
}

void dns_client::tcp_tls_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const dns_complete &complete_handler) {
    tcp_tls_dns(domain, dns_server, port, timeout, make_pair("", 0), complete_handler);
}

void dns_client::tcp_dns(const string &domain, const std::string &dns_server, uint16_t port, uint64_t timeout, const dns_complete &complete_handler) {
    tcp_dns(domain, dns_server, port, timeout, make_pair("", 0), complete_handler);
}


void dns_client::forward_udp(udp_request &udpdns_request, const std::string &dns_server, uint32_t port, uint64_t timeout, const std::function<void(udp_response *)>& callback) {
    uint64_t beginTime = time::now();
    auto state = std::make_shared<forward_udp_state>();
    state->socket = std::make_shared<udp::socket>(ic, udp::endpoint(udp::v4(), 0));
    state->timer = std::make_shared<deadline_timer>(ic);
    state->request_data.assign(udpdns_request.data, udpdns_request.data + udpdns_request.len);
    string logTag = "forward_udp to " + dns_server;
    std::function<void(udp_response *)> complete = [state, callback, logTag, beginTime](udp_response *res) {
        if (state->completed.exchange(true)) {
            return;
        }
        boost::system::error_code ignored_ec;
        state->timer->cancel(ignored_ec);
        state->socket->cancel(ignored_ec);
        state->socket->close(ignored_ec);
        udp_response *response = res == state->response.get() ? state->response.release() : nullptr;
        if (response != nullptr) {
            logger::INFO << logTag << "sucess!"
                         << "cost" << time::now() - beginTime << END;
        }
        callback(response);
    };
    state->timer->expires_from_now(boost::posix_time::milliseconds(timeout));
    state->timer->async_wait([complete](boost::system::error_code ec) {
        if (!ec) {
            complete(nullptr);
        }
    });
    state->socket->async_send_to(buffer(state->request_data.data(), state->request_data.size()),
                          udp::endpoint(make_address_v4(dns_server), port),
                          [this, state, complete, logTag, beginTime, timeout](boost::system::error_code error, size_t size) {
                              if (!is_timeout_or_error(logTag, error, beginTime, timeout, complete)) {
                                  state->response.reset(new udp_response(1024));
                                  state->socket->async_receive_from(
                                          buffer(state->response->data, sizeof(uint8_t) * 1024),
                                          state->server_endpoint,
                                          [this, state, complete, logTag, beginTime, timeout](boost::system::error_code error, size_t size) {
                                                  if (!is_timeout_or_error(logTag, error, beginTime, timeout, complete) && size > 0) {
                                                  state->response->len = size;
                                                  complete(state->response.get());
                                              } else {
                                                  complete(nullptr);
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
    delete iw;
    iw = nullptr;
    if (th && th->joinable()) {
        th->join();
    }
    delete th;
    // ssl_ctx 必须在线程退出后、OPENSSL_cleanup 之前销毁
    delete ssl_ctx;
    ssl_ctx = nullptr;
    // 主动清理 OpenSSL，避免 atexit 阶段与 tcmalloc 冲突导致 SEGFAULT
    OPENSSL_cleanup();
}

dns_client::dns_client() : ic() {
    ssl_ctx = new boost::asio::ssl::context(boost::asio::ssl::context::sslv23_client);
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
