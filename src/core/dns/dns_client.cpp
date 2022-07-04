//
// Created by codingdie on 2020/5/20.
//

#include "dns_client.h"
#include "utils/utils.h"
#include <sys/socket.h>

dns_client dns_client::INSTANCE;
using namespace st::dns::protocol;
template<typename Result>
bool dns_client::is_timeout_or_error(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(Result)> complete_handler, Result defaultV) {
    uint64_t cost = time::now() - beginTime;
    if (!ec && cost <= timeout) {
        return false;
    }
    if (ec && ec != boost::asio::error::operation_aborted) {
        logger::ERROR << logTag << "error! cost:" << cost << ec.message() << END
    } else {
        logger::ERROR << logTag << "timout! cost" << cost << END;
    }
    complete_handler(defaultV);
    return true;
}

bool dns_client::is_timeout_or_error(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(std::vector<uint32_t>)> complete_handler) {
    return is_timeout_or_error(logTag, ec, beginTime, timeout, complete_handler, {});
}
bool dns_client::is_timeout_or_error(const string &logTag, boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(protocol::udp_response *res)> complete_handler) {
    udp_response *res = nullptr;
    return is_timeout_or_error(logTag, ec, beginTime, timeout, complete_handler, res);
}
void dns_client::udp_dns(const string &domain, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(std::vector<uint32_t> ips)> complete_handler) {
    vector<string> domains;
    domains.emplace_back(domain);
    if (domains.empty()) {
        return complete_handler({});
    }
    uint64_t beginTime = time::now();
    udp::socket *socket = new udp::socket(ioContext, udp::endpoint(udp::v4(), 0));
    udp_request dnsRequest(domains);
    unsigned short qid = dnsRequest.header->id;
    uint16_t dnsId = dnsRequest.header->id;
    string logTag = to_string(dnsId) + " udp_dns " + dnsServer + " " + domains[0];
    deadline_timer *timeoutTimer = new deadline_timer(ioContext);
    std::function<void(std::vector<uint32_t> ips)> complete = [=](std::vector<uint32_t> ips) {
        delete timeoutTimer;
        if (ips.size() > 0) {
            logger::INFO << logTag << "cost" << time::now() - beginTime << "resolve ips" << st::utils::ipv4::ips_to_str(ips) << END;
        }
        complete_handler(ips);
    };
    timeoutTimer->expires_from_now(boost::posix_time::milliseconds(timeout));
    timeoutTimer->async_wait([=](boost::system::error_code ec) {
        socket->shutdown(boost::asio::socket_base::shutdown_both, ec);
        socket->cancel(ec);
        socket->close(ec);
        ioContext.post([=]() {
            delete socket;
        });
    });
    socket->async_send_to(buffer(dnsRequest.data, dnsRequest.len),
                          udp::endpoint(make_address_v4(dnsServer), port), [=](boost::system::error_code error, size_t size) {
                              if (!is_timeout_or_error(logTag, error, beginTime, timeout, complete)) {
                                  udp_response *dnsResponse = new udp_response(1024);
                                  udp::endpoint *serverEndpoint = new udp::endpoint();
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

void dns_client::tcp_tls_dns(const string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const string &area, std::function<void(std::vector<uint32_t> ips)> complete_handler) {
    if (!area.empty()) {
        port = st::dns::shm::share().set_virtual_port(st::utils::ipv4::str_to_ip(dnsServer), port, area);
    }
    vector<string> domains;
    domains.emplace_back(domain);
    uint64_t beginTime = time::now();
    boost::asio::ssl::context sslCtx(boost::asio::ssl::context::sslv23_client);
    tcp::endpoint serverEndpoint(make_address_v4(dnsServer), port);
    tcp_request *dnsRequest = new tcp_request(domains);
    uint16_t dnsId = dnsRequest->header->id;
    string logTag = to_string(dnsId) + " " + dnsServer + " tcp_tls_dns " + domains[0] + (!area.empty() ? " " + area : "");
    boost::asio::ssl::stream<tcp::socket> *socket = new boost::asio::ssl::stream<tcp::socket>(ioContext, sslCtx);
    socket->set_verify_mode(ssl::verify_none);
    pair<uint8_t *, uint32_t> dataBytes = st::mem::pmalloc(2048);
    pair<uint8_t *, uint32_t> lengthBytes = st::mem::pmalloc(2);
    deadline_timer *timeoutTimer = new deadline_timer(ioContext);
    std::function<void(std::vector<uint32_t> ips)> complete =
            [=](std::vector<uint32_t> ips) {
                delete timeoutTimer;
                if (ips.size() > 0) {
                    logger::INFO << logTag << "cost" << time::now() - beginTime << "resolve ips" << st::utils::ipv4::ips_to_str(ips) << END;
                }
                complete_handler(ips);
            };
    timeoutTimer->expires_from_now(boost::posix_time::milliseconds(timeout));
    timeoutTimer->async_wait([=](boost::system::error_code ec) {
        socket->async_shutdown([=](boost::system::error_code ec) {
            socket->lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, ec);
            socket->lowest_layer().cancel(ec);
            socket->lowest_layer().close(ec);
            ioContext.post([=]() {
                st::mem::pfree(dataBytes);
                st::mem::pfree(lengthBytes);
                delete dnsRequest;
                delete socket;
            });
        });
    });
    socket->lowest_layer().async_connect(
            serverEndpoint,
            [=](boost::system::error_code ec) {
                if (!is_timeout_or_error(logTag, ec, beginTime, timeout, complete)) {
                    socket->async_handshake(
                            boost::asio::ssl::stream_base::client,
                            [=](boost::system::error_code ec) {
                                if (!is_timeout_or_error(logTag, ec, beginTime, timeout, complete)) {
                                    st::utils::copy(dnsRequest->data, dataBytes.first, 0, 0, dnsRequest->len);
                                    boost::asio::async_write(
                                            *socket,
                                            buffer(dataBytes.first, dnsRequest->len),
                                            [=](boost::system::error_code ec, std::size_t length) {
                                                if (!is_timeout_or_error(logTag, ec, beginTime, timeout, complete)) {
                                                    boost::asio::async_read(
                                                            *socket,
                                                            buffer(lengthBytes.first, 2),
                                                            [=](boost::system::error_code ec, std::size_t length) {
                                                                if (!is_timeout_or_error(logTag, ec, beginTime, timeout, complete)) {
                                                                    uint16_t dataLen = 0;
                                                                    st::utils::read(lengthBytes.first, dataLen);
                                                                    if (dataLen > 1024) {
                                                                        complete({});
                                                                    } else {
                                                                        boost::asio::async_read(
                                                                                *socket,
                                                                                buffer(dataBytes.first, dataLen),
                                                                                [=](boost::system::error_code ec, std::size_t length) {
                                                                                    if (!is_timeout_or_error(logTag, ec, beginTime, timeout, complete)) {
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
            });
}


void dns_client::tcp_dns(const string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const string &area, std::function<void(std::vector<uint32_t> ips)> complete_handler) {
    if (!area.empty()) {
        port = st::dns::shm::share().set_virtual_port(st::utils::ipv4::str_to_ip(dnsServer), port, area);
    }
    vector<string> domains;
    domains.emplace_back(domain);
    uint64_t beginTime = time::now();
    boost::asio::ssl::context sslCtx(boost::asio::ssl::context::sslv23_client);
    tcp::endpoint serverEndpoint(make_address_v4(dnsServer), port);
    tcp_request *dnsRequest = new tcp_request(domains);
    uint16_t dnsId = dnsRequest->header->id;
    string logTag = to_string(dnsId) + " tcp_dns " + dnsServer + " " + domains[0];
    tcp::socket *socket = new tcp::socket(ioContext);
    pair<uint8_t *, uint32_t> dataBytes = st::mem::pmalloc(2048);
    pair<uint8_t *, uint32_t> lengthBytes = st::mem::pmalloc(2);
    deadline_timer *timeoutTimer = new deadline_timer(ioContext);
    std::function<void(std::vector<uint32_t> ips)> complete =
            [=](std::vector<uint32_t> ips) {
                delete timeoutTimer;
                if (ips.size() > 0) {
                    logger::INFO << logTag << "cost" << time::now() - beginTime << "resolve ips" << st::utils::ipv4::ips_to_str(ips) << END;
                }
                complete_handler(ips);
            };
    timeoutTimer->expires_from_now(boost::posix_time::milliseconds(timeout));
    timeoutTimer->async_wait([=](boost::system::error_code ec) {
        socket->lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, ec);
        socket->lowest_layer().cancel(ec);
        socket->lowest_layer().close(ec);
        ioContext.post([=]() {
            st::mem::pfree(dataBytes);
            st::mem::pfree(lengthBytes);
            delete dnsRequest;
            delete socket;
        });
    });

    socket->async_connect(
            serverEndpoint,
            [=](boost::system::error_code ec) {
                if (!is_timeout_or_error(logTag, ec, beginTime, timeout, complete)) {
                    st::utils::copy(dnsRequest->data, dataBytes.first, 0, 0, dnsRequest->len);
                    boost::asio::async_write(
                            *socket,
                            buffer(dataBytes.first, dnsRequest->len),
                            [=](boost::system::error_code ec, std::size_t length) {
                                if (!is_timeout_or_error(logTag, ec, beginTime, timeout, complete)) {
                                    boost::asio::async_read(
                                            *socket,
                                            buffer(lengthBytes.first, 2),
                                            [=](boost::system::error_code ec, std::size_t length) {
                                                if (!is_timeout_or_error(logTag, ec, beginTime, timeout, complete)) {
                                                    uint16_t dataLen = 0;
                                                    st::utils::read(lengthBytes.first, dataLen);
                                                    if (dataLen > 1024) {
                                                        complete({});
                                                    } else {
                                                        boost::asio::async_read(
                                                                *socket,
                                                                buffer(dataBytes.first, dataLen),
                                                                [=](boost::system::error_code ec, std::size_t length) {
                                                                    if (!is_timeout_or_error(logTag, ec, beginTime, timeout, complete)) {
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


void dns_client::tcp_tls_dns(const string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const unordered_set<string> &areas, std::function<void(std::vector<uint32_t> ips, bool loadAll)> complete_handler) {
    if (areas.empty()) {
        tcp_tls_dns(domain, dnsServer, port, timeout, [=](std::vector<uint32_t> ips) {
            complete_handler(ips, true);
        });
    } else if (areas.size() == 1) {
        tcp_tls_dns(domain, dnsServer, port, timeout, *areas.begin(), [=](std::vector<uint32_t> ips) {
            complete_handler(ips, true);
        });
    } else {
        atomic_uint16_t *counter = new atomic_uint16_t(0);
        atomic_bool *loadAll = new atomic_bool(true);
        std::unordered_set<uint32_t> *result = new std::unordered_set<uint32_t>();
        std::function<void(std::vector<uint32_t> ips)> eachHandler = [=](std::vector<uint32_t> ips) {
            counter->fetch_add(1);
            if (ips.size() == 0) {
                *loadAll = false;
            }
            for (auto ip : ips) {
                result->emplace(ip);
            }
            if (counter->load() >= areas.size()) {
                std::vector<uint32_t> s(result->begin(), result->end());
                complete_handler(ips, loadAll->load());
                delete result;
                delete counter;
                delete loadAll;
            }
        };
        for (const string area : areas) {
            tcp_tls_dns(domain, dnsServer, port, timeout, area, eachHandler);
        }
    }
}

void dns_client::tcp_dns(const string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, const unordered_set<string> &areas, std::function<void(std::vector<uint32_t> ips, bool loadAll)> complete_handler) {
    if (areas.empty()) {
        tcp_dns(domain, dnsServer, port, timeout, [=](std::vector<uint32_t> ips) {
            complete_handler(ips, true);
        });
    } else if (areas.size() == 1) {
        tcp_dns(domain, dnsServer, port, timeout, *areas.begin(), [=](std::vector<uint32_t> ips) {
            complete_handler(ips, true);
        });
    } else {
        atomic_bool *loadAll = new atomic_bool(true);
        atomic_uint16_t *counter = new atomic_uint16_t(0);
        std::vector<uint32_t> *result = new std::vector<uint32_t>();
        std::function<void(std::vector<uint32_t> ips)> eachHandler = [=](std::vector<uint32_t> ips) {
            counter->fetch_add(1);
            for (auto ip : ips) {
                result->emplace_back(ip);
            }
            if (ips.size() == 0) {
                *loadAll = false;
            }
            if (counter->load() == areas.size()) {
                complete_handler(*result, loadAll->load());
                delete result;
                delete counter;
                delete loadAll;
            }
        };
        for (const string area : areas) {
            tcp_dns(domain, dnsServer, port, timeout, area, eachHandler);
        }
    }
}

void dns_client::tcp_tls_dns(const string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(std::vector<uint32_t> ips)> complete_handler) {
    tcp_tls_dns(domain, dnsServer, port, timeout, "", complete_handler);
}

void dns_client::tcp_dns(const string &domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(std::vector<uint32_t> ips)> complete_handler) {
    tcp_dns(domain, dnsServer, port, timeout, "", complete_handler);
}


void dns_client::forward_udp(udp_request &udpDnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(udp_response *)> callback) {
    uint64_t beginTime = time::now();
    udp::socket *socket = new udp::socket(ioContext, udp::endpoint(udp::v4(), 0));
    string logTag = "forward_udp to " + dnsServer;
    deadline_timer *timeoutTimer = new deadline_timer(ioContext);
    std::function<void(udp_response *)> complete = [=](udp_response *res) {
        delete timeoutTimer;
        if (res != nullptr) {
            logger::INFO << logTag << "sucess!"
                         << "cost" << time::now() - beginTime << END;
        }
        callback(res);
    };
    timeoutTimer->expires_from_now(boost::posix_time::milliseconds(timeout));
    timeoutTimer->async_wait([=](boost::system::error_code ec) {
        socket->shutdown(boost::asio::socket_base::shutdown_both, ec);
        socket->cancel(ec);
        socket->close(ec);
        ioContext.post([=]() {
            delete socket;
        });
    });
    socket->async_send_to(buffer(udpDnsRequest.data, udpDnsRequest.len),
                          udp::endpoint(make_address_v4(dnsServer), port),
                          [=](boost::system::error_code error, size_t size) {
                              if (!is_timeout_or_error(logTag, error, beginTime, timeout, complete)) {
                                  udp_response *dnsResponse = new udp_response(1024);
                                  udp::endpoint *serverEndpoint = new udp::endpoint();
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
    ioContext.stop();
    delete ioWoker;
    th->join();
    delete th;
}

dns_client::dns_client() {
    ioWoker = new boost::asio::io_context::work(ioContext);
    th = new thread([=]() {
        this->ioContext.run();
    });
}


//
//tcp_response *DNSClient::queryEDNS(const vector<string> &domains, const string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout) {
//    uint64_t beginTime = time::now();
//    uint64_t restTime = timeout;
////    boost::asio::ssl::context sslCtx(boost::asio::ssl::context::sslv23_client);
//    tcp::endpoint serverEndpoint(make_address_v4(dnsServer), port);
//    tcp_request dnsRequest(domains, ip);
//    unsigned short qid = dnsRequest.header->id;
//    io_context &ctxThreadLocal = asio::TLIOContext::getIOContext();
////    boost::asio::ssl::stream<tcp::socket> socket(ctxThreadLocal, sslCtx);
////    socket.set_verify_mode(ssl::verify_none);
//    boost::asio::ip::tcp::socket socket(ctxThreadLocal);
//    uint8_t dataBytes[1024];
//    uint8_t lengthBytes[2];
//    uint16_t length = 0;
//    bool error = false;
//    uint64_t begin = time::now();
//    auto connectFuture = socket.lowest_layer().async_connect(serverEndpoint, boost::asio::use_future([](boost::system::error_code ec) {
//        if (ec) {
//            logger::ERROR << "connect error!" << ec.message() << END;
//            return false;
//        }
//        return true;
//    }));
//    ctxThreadLocal.restart();
//    ctxThreadLocal.run();
//    if (is_timeout_or_error(beginTime, timeout, restTime, connectFuture, std::string())) {
//        logger::ERROR << dnsRequest.header->id << "connect timeout!" << END;
//        error = true;
//    } else {
////        auto shakeFuture = socket.async_handshake(boost::asio::ssl::stream_base::client, boost::asio::use_future([](boost::system::error_code error) {
////            if (error) {
////                logger::ERROR << "ssl handshake error!" << error.message() << END;
////                return false;
////            }
////            return true;
////        }));
////        ctxThreadLocal.restart();
////        ctxThreadLocal.run();
////        bool isTimeout = shakeFuture.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::ready;
////        timeout -= (time::now() - begin);
////        begin = time::now();
////        if (isTimeout(begin, timeout) || isTimeout || shakeFuture.get() == false) {
////            logger::ERROR << dnsRequest.header->id << "ssl handshake timeout!" << END
////            error = true;
////        } else
//        { ;
//            st::utils::copy(dnsRequest.data, dataBytes, 0, 0, dnsRequest.len);
//            auto sendFuture = socket.async_write_some(buffer(dataBytes, dnsRequest.len),
//                                                      boost::asio::use_future([](boost::system::error_code error, std::size_t length) {
//                                                          if (error) {
//                                                              logger::ERROR << "ssl async_write_some error!" << error.message() << END
//                                                              return false;
//                                                          }
//                                                          return true;
//                                                      }));
//            ctxThreadLocal.restart();
//            ctxThreadLocal.run();
//            if (is_timeout_or_error(beginTime, timeout, restTime, sendFuture, std::string())) {
//                logger::ERROR << dnsRequest.header->id << "send dns request timeout!" << END;
//                error = true;
//            } else {
//                auto receiveLenFuture = boost::asio::async_read(socket, buffer(lengthBytes, 2),
//                                                                boost::asio::use_future([](boost::system::error_code error, std::size_t length) {
//                                                                    if (error) {
//                                                                        logger::ERROR << "receive len error!" << error.message() << END;
//                                                                        return false;
//                                                                    }
//                                                                    return true;
//                                                                }));
//                ctxThreadLocal.restart();
//                ctxThreadLocal.run();
//                if (is_timeout_or_error(beginTime, timeout, restTime, receiveLenFuture, std::string())) {
//                    logger::ERROR << dnsRequest.header->id << "receive dns response len timeout!" << END;
//                    error = true;
//                } else {
//                    st::utils::read(lengthBytes, length);
//                    auto receiveDataFuture = boost::asio::async_read(socket, buffer(dataBytes, length),
//                                                                     boost::asio::use_future([](boost::system::error_code error, std::size_t length) {
//                                                                         if (error) {
//                                                                             logger::ERROR << "ssl async_read error!" << error.message() << END;
//                                                                             return false;
//                                                                         }
//                                                                         return true;
//                                                                     }));
//                    ctxThreadLocal.restart();
//                    ctxThreadLocal.run();
//                    if (is_timeout_or_error(beginTime, timeout, restTime, receiveDataFuture, std::string())) {
//                        logger::ERROR << dnsRequest.header->id << "receive dns response data timeout!" << END;
//                        error = true;
//                    }
//                }
//            }
//        }
//
//
//    }
//
//    tcp_response *dnsResponse = nullptr;
//    if (!error && length > 0) {
//        if (length < 2048) {
//            dnsResponse = new tcp_response(2 + length);
//            st::utils::copy(lengthBytes, dnsResponse->data, 0, 0, 2);
//            st::utils::copy(dataBytes, dnsResponse->data + 2, 0, 0, length);
//            dnsResponse->parse(2 + length);
//            if (!dnsResponse->is_valid()) {
//                logger::ERROR << dnsRequest.header->id << "receive unmarketable data" << END;
//                error = true;
//            } else {
//                if (dnsResponse->response != nullptr) {
//                    if (dnsResponse->response->header->id != qid) {
//                        logger::ERROR << dnsRequest.header->id << "receive not valid header id" << END;
//                        error = true;
//                    }
//                    if (dnsResponse->response->header->responseCode != 0) {
//                        logger::ERROR << dnsRequest.header->id << "receive error responseCode" << dnsResponse->response->header->responseCode << END;
//                        error = true;
//                    }
//                }
//            }
//        }
//    }
//    if (error) {
//        if (dnsResponse != nullptr) {
//            delete dnsResponse;
//            dnsResponse = nullptr;
//        }
//    }
//    ctxThreadLocal.stop();
//    boost::system::error_code errorCode;
////    socket.shutdown(errorCode);
//    socket.lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, errorCode);
//    socket.lowest_layer().cancel(errorCode);
//    socket.lowest_layer().close(errorCode);
//    return dnsResponse;
//}
//
//tcp_response *DNSClient::EDNS(const std::string &domain, const std::string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout) {
//    vector<string> domains;
//    domains.emplace_back(domain);
//    return instance.queryEDNS(domains, dnsServer, port, ip, timeout);
//}