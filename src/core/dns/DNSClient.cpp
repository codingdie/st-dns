//
// Created by codingdie on 2020/5/20.
//

#include "DNSClient.h"
#include "utils/STUtils.h"
#include <sys/socket.h>

DNSClient DNSClient::INSTANCE;


template<typename Result>
bool DNSClient::isTimeoutOrError(boost::system::error_code ec, uint64_t beginTime, uint64_t timeout, std::function<void(Result *)> completeHandler) {
    uint64_t cost = time::now() - beginTime;
    if (!ec && cost <= timeout) {
        return false;
    }
    if (ec) {
        Logger::ERROR << "error!" << ec.message() << END
    } else {
        Logger::ERROR << "timout! cost" << cost << END;
    }
    completeHandler(nullptr);
    return true;
}


void DNSClient::udpDns(const string domain, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(UdpDNSResponse *)> completeHandler) {
    vector<string> domains;
    domains.emplace_back(domain);
    if (domains.empty()) {
        return completeHandler(nullptr);
    }
    uint64_t beginTime = time::now();
    udp::socket *socket = new udp::socket(ioContext, udp::endpoint(udp::v4(), 0));
    UdpDnsRequest dnsRequest(domains);
    unsigned short qid = dnsRequest.dnsHeader->id;
    uint16_t dnsId = dnsRequest.dnsHeader->id;
    string logTag = to_string(dnsId) + " " + dnsServer + " " + domains[0];
    std::function<void(UdpDNSResponse *)> complete = [=](UdpDNSResponse *res) {
        delete socket;
        Logger::DEBUG << logTag << "cost" << time::now() - beginTime << END;
        completeHandler(res);
    };
    socket->async_send_to(buffer(dnsRequest.data, dnsRequest.len),
                          udp::endpoint(make_address_v4(dnsServer), port), [=](boost::system::error_code error, size_t size) {
                              if (!isTimeoutOrError(error, beginTime, timeout, complete)) {
                                  UdpDNSResponse *dnsResponse = new UdpDNSResponse(1024);
                                  udp::endpoint serverEndpoint;
                                  socket->async_receive_from(buffer(dnsResponse->data, sizeof(uint8_t) * 1024), serverEndpoint,
                                                             [=](boost::system::error_code error, size_t size) {
                                                                 if (!isTimeoutOrError(error, beginTime, timeout, complete)) {
                                                                     dnsResponse->parse(size);
                                                                     if (dnsResponse->isValid() && dnsResponse->header->id == qid) {
                                                                         complete(dnsResponse);
                                                                         return;
                                                                     }
                                                                 }
                                                                 delete dnsResponse;
                                                             });
                              }
                          });
}

TcpDNSResponse *parse(uint16_t length, pair<uint8_t *, uint32_t> lengthBytes, pair<uint8_t *, uint32_t> dataBytes, uint16_t dnsId) {
    bool error = false;
    TcpDNSResponse *dnsResponse = nullptr;
    if (length > 0 && length < 2048) {
        dnsResponse = new TcpDNSResponse(2 + length);
        st::utils::copy(lengthBytes.first, dnsResponse->data, 0, 0, 2);
        st::utils::copy(dataBytes.first, dnsResponse->data + 2, 0, 0, length);
        dnsResponse->parse(2 + length);
        bool error = false;
        if (!dnsResponse->isValid()) {
            Logger::ERROR << dnsId << "receive unmarketable data" << END;
            error = true;
        } else {
            if (dnsResponse->udpDnsResponse->header->id != dnsId) {
                Logger::ERROR << dnsId << "receive not valid header id" << END;
                error = true;
            }
            if (dnsResponse->udpDnsResponse->header->responseCode != 0) {
                error = true;
                Logger::ERROR << dnsId << "receive error responseCode"
                              << dnsResponse->udpDnsResponse->header->responseCode << END;
            }
        }
    } else {
        error = true;
        Logger::ERROR << dnsId << "receive unmarketable data" << END;
    }
    if (error) {
        if (dnsResponse != nullptr) {

            delete dnsResponse;
            dnsResponse = nullptr;
        }
    }
    return dnsResponse;
}
void DNSClient::tcpTlsDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(TcpDNSResponse *)> completeHandler) {
    vector<string> domains;
    domains.emplace_back(domain);
    uint64_t beginTime = time::now();
    boost::asio::ssl::context sslCtx(boost::asio::ssl::context::sslv23_client);
    tcp::endpoint serverEndpoint(make_address_v4(dnsServer), port);
    TcpDnsRequest *dnsRequest = new TcpDnsRequest(domains);
    uint16_t dnsId = dnsRequest->dnsHeader->id;
    string logTag = to_string(dnsId) + " " + dnsServer + " " + domains[0];
    boost::asio::ssl::stream<tcp::socket> *socket = new boost::asio::ssl::stream<tcp::socket>(ioContext, sslCtx);
    socket->set_verify_mode(ssl::verify_none);
    pair<uint8_t *, uint32_t> dataBytes = st::mem::pmalloc(1024);
    pair<uint8_t *, uint32_t> lengthBytes = st::mem::pmalloc(2);

    std::function<void(TcpDNSResponse *)> complete =
            [=](TcpDNSResponse *res) {
                delete socket;
                delete dnsRequest;
                st::mem::pfree(dataBytes);
                st::mem::pfree(lengthBytes);
                Logger::DEBUG << logTag << "cost" << time::now() - beginTime << END;
                completeHandler(res);
            };
    socket->lowest_layer().async_connect(
            serverEndpoint,
            [=](boost::system::error_code ec) {
                if (!isTimeoutOrError(ec, beginTime, timeout, complete)) {
                    socket->async_handshake(
                            boost::asio::ssl::stream_base::client,
                            [=](boost::system::error_code ec) {
                                if (!isTimeoutOrError(ec, beginTime, timeout, complete)) {
                                    st::utils::copy(dnsRequest->data, dataBytes.first, 0, 0, dnsRequest->len);
                                    boost::asio::async_write(
                                            *socket,
                                            buffer(dataBytes.first, dnsRequest->len),
                                            [=](boost::system::error_code ec, std::size_t length) {
                                                if (!isTimeoutOrError(ec, beginTime, timeout, complete)) {
                                                    boost::asio::async_read(
                                                            *socket,
                                                            buffer(lengthBytes.first, 2),
                                                            [=](boost::system::error_code ec, std::size_t length) {
                                                                if (!isTimeoutOrError(ec, beginTime, timeout, complete)) {
                                                                    uint16_t dataLen = 0;
                                                                    st::utils::read(lengthBytes.first, dataLen);
                                                                    boost::asio::async_read(
                                                                            *socket,
                                                                            buffer(dataBytes.first, dataLen),
                                                                            [=](boost::system::error_code ec, std::size_t length) {
                                                                                if (!isTimeoutOrError(ec, beginTime, timeout, complete)) {
                                                                                    complete(parse(length, lengthBytes, dataBytes, dnsId));
                                                                                }
                                                                            });
                                                                }
                                                            });
                                                }
                                            });
                                }
                            });
                }
            });
}


void DNSClient::tcpDNS(const string domain, const std::string &dnsServer, uint16_t port, uint64_t timeout, std::function<void(TcpDNSResponse *)> completeHandler) {
    vector<string> domains;
    domains.emplace_back(domain);
    uint64_t beginTime = time::now();
    boost::asio::ssl::context sslCtx(boost::asio::ssl::context::sslv23_client);
    tcp::endpoint serverEndpoint(make_address_v4(dnsServer), port);
    TcpDnsRequest *dnsRequest = new TcpDnsRequest(domains);
    uint16_t dnsId = dnsRequest->dnsHeader->id;
    string logTag = to_string(dnsId) + " " + dnsServer + " " + domains[0];
    tcp::socket *socket = new tcp::socket(ioContext);
    pair<uint8_t *, uint32_t> dataBytes = st::mem::pmalloc(1024);
    pair<uint8_t *, uint32_t> lengthBytes = st::mem::pmalloc(2);
    std::function<void(TcpDNSResponse *)> complete =
            [=](TcpDNSResponse *res) {
                delete socket;
                delete dnsRequest;
                st::mem::pfree(dataBytes);
                st::mem::pfree(lengthBytes);
                Logger::DEBUG << logTag << "cost" << time::now() - beginTime << END;
                completeHandler(res);
            };
    socket->async_connect(
            serverEndpoint,
            [=](boost::system::error_code ec) {
                if (!isTimeoutOrError(ec, beginTime, timeout, complete)) {
                    if (!isTimeoutOrError(ec, beginTime, timeout, complete)) {
                        st::utils::copy(dnsRequest->data, dataBytes.first, 0, 0, dnsRequest->len);
                        boost::asio::async_write(
                                *socket,
                                buffer(dataBytes.first, dnsRequest->len),
                                [=](boost::system::error_code ec, std::size_t length) {
                                    if (!isTimeoutOrError(ec, beginTime, timeout, complete)) {
                                        boost::asio::async_read(
                                                *socket,
                                                buffer(lengthBytes.first, 2),
                                                [=](boost::system::error_code ec, std::size_t length) {
                                                    if (!isTimeoutOrError(ec, beginTime, timeout, complete)) {
                                                        uint16_t dataLen = 0;
                                                        st::utils::read(lengthBytes.first, dataLen);
                                                        boost::asio::async_read(
                                                                *socket,
                                                                buffer(dataBytes.first, dataLen),
                                                                [=](boost::system::error_code ec, std::size_t length) {
                                                                    if (!isTimeoutOrError(ec, beginTime, timeout, complete)) {
                                                                        complete(parse(length, lengthBytes, dataBytes, dnsId));
                                                                    }
                                                                });
                                                    }
                                                });
                                    }
                                });
                    }
                }
            });
}

void DNSClient::forwardUdp(UdpDnsRequest &dnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout, std::function<void(UdpDNSResponse *)> callback) {
    uint64_t beginTime = time::now();
    udp::socket *socket = new udp::socket(ioContext, udp::endpoint(udp::v4(), 0));
    string logTag = "forwardUdp to " + dnsServer;
    auto traceId = Logger::traceId;
    std::function<void(UdpDNSResponse *)> complete = [=](UdpDNSResponse *res) {
        delete socket;
        Logger::INFO << logTag << "finished!"
                     << "cost" << time::now() - beginTime << END;
        callback(res);
    };
    socket->async_send_to(buffer(dnsRequest.data, dnsRequest.len),
                          udp::endpoint(make_address_v4(dnsServer), port),
                          [=](boost::system::error_code error, size_t size) {
                              if (!isTimeoutOrError(error, beginTime, timeout, complete)) {
                                  UdpDNSResponse *dnsResponse = new UdpDNSResponse(1024);
                                  udp::endpoint serverEndpoint;
                                  socket->async_receive_from(
                                          buffer(dnsResponse->data, sizeof(uint8_t) * 1024),
                                          serverEndpoint,
                                          [=](boost::system::error_code error, size_t size) {
                                              if (!isTimeoutOrError(error, beginTime, timeout, complete) && size > 0) {
                                                  dnsResponse->len = size;
                                                  complete(dnsResponse);
                                              } else {
                                                  delete dnsResponse;
                                              }
                                          });
                              }
                          });
}


DNSClient::~DNSClient() {
    ioContext.stop();
    delete ioWoker;
    th->join();
    delete th;
}

DNSClient::DNSClient() {
    ioWoker = new boost::asio::io_context::work(ioContext);
    th = new thread([=]() {
        this->ioContext.run();
    });
}


//
//TcpDNSResponse *DNSClient::queryEDNS(const vector<string> &domains, const string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout) {
//    uint64_t beginTime = time::now();
//    uint64_t restTime = timeout;
////    boost::asio::ssl::context sslCtx(boost::asio::ssl::context::sslv23_client);
//    tcp::endpoint serverEndpoint(make_address_v4(dnsServer), port);
//    TcpDnsRequest dnsRequest(domains, ip);
//    unsigned short qid = dnsRequest.dnsHeader->id;
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
//            Logger::ERROR << "connect error!" << ec.message() << END;
//            return false;
//        }
//        return true;
//    }));
//    ctxThreadLocal.restart();
//    ctxThreadLocal.run();
//    if (isTimeoutOrError(beginTime, timeout, restTime, connectFuture, std::string())) {
//        Logger::ERROR << dnsRequest.dnsHeader->id << "connect timeout!" << END;
//        error = true;
//    } else {
////        auto shakeFuture = socket.async_handshake(boost::asio::ssl::stream_base::client, boost::asio::use_future([](boost::system::error_code error) {
////            if (error) {
////                Logger::ERROR << "ssl handshake error!" << error.message() << END;
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
////            Logger::ERROR << dnsRequest.dnsHeader->id << "ssl handshake timeout!" << END
////            error = true;
////        } else
//        { ;
//            st::utils::copy(dnsRequest.data, dataBytes, 0, 0, dnsRequest.len);
//            auto sendFuture = socket.async_write_some(buffer(dataBytes, dnsRequest.len),
//                                                      boost::asio::use_future([](boost::system::error_code error, std::size_t length) {
//                                                          if (error) {
//                                                              Logger::ERROR << "ssl async_write_some error!" << error.message() << END
//                                                              return false;
//                                                          }
//                                                          return true;
//                                                      }));
//            ctxThreadLocal.restart();
//            ctxThreadLocal.run();
//            if (isTimeoutOrError(beginTime, timeout, restTime, sendFuture, std::string())) {
//                Logger::ERROR << dnsRequest.dnsHeader->id << "send dns request timeout!" << END;
//                error = true;
//            } else {
//                auto receiveLenFuture = boost::asio::async_read(socket, buffer(lengthBytes, 2),
//                                                                boost::asio::use_future([](boost::system::error_code error, std::size_t length) {
//                                                                    if (error) {
//                                                                        Logger::ERROR << "receive len error!" << error.message() << END;
//                                                                        return false;
//                                                                    }
//                                                                    return true;
//                                                                }));
//                ctxThreadLocal.restart();
//                ctxThreadLocal.run();
//                if (isTimeoutOrError(beginTime, timeout, restTime, receiveLenFuture, std::string())) {
//                    Logger::ERROR << dnsRequest.dnsHeader->id << "receive dns response len timeout!" << END;
//                    error = true;
//                } else {
//                    st::utils::read(lengthBytes, length);
//                    auto receiveDataFuture = boost::asio::async_read(socket, buffer(dataBytes, length),
//                                                                     boost::asio::use_future([](boost::system::error_code error, std::size_t length) {
//                                                                         if (error) {
//                                                                             Logger::ERROR << "ssl async_read error!" << error.message() << END;
//                                                                             return false;
//                                                                         }
//                                                                         return true;
//                                                                     }));
//                    ctxThreadLocal.restart();
//                    ctxThreadLocal.run();
//                    if (isTimeoutOrError(beginTime, timeout, restTime, receiveDataFuture, std::string())) {
//                        Logger::ERROR << dnsRequest.dnsHeader->id << "receive dns response data timeout!" << END;
//                        error = true;
//                    }
//                }
//            }
//        }
//
//
//    }
//
//    TcpDNSResponse *dnsResponse = nullptr;
//    if (!error && length > 0) {
//        if (length < 2048) {
//            dnsResponse = new TcpDNSResponse(2 + length);
//            st::utils::copy(lengthBytes, dnsResponse->data, 0, 0, 2);
//            st::utils::copy(dataBytes, dnsResponse->data + 2, 0, 0, length);
//            dnsResponse->parse(2 + length);
//            if (!dnsResponse->isValid()) {
//                Logger::ERROR << dnsRequest.dnsHeader->id << "receive unmarketable data" << END;
//                error = true;
//            } else {
//                if (dnsResponse->udpDnsResponse != nullptr) {
//                    if (dnsResponse->udpDnsResponse->header->id != qid) {
//                        Logger::ERROR << dnsRequest.dnsHeader->id << "receive not valid header id" << END;
//                        error = true;
//                    }
//                    if (dnsResponse->udpDnsResponse->header->responseCode != 0) {
//                        Logger::ERROR << dnsRequest.dnsHeader->id << "receive error responseCode" << dnsResponse->udpDnsResponse->header->responseCode << END;
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
//TcpDNSResponse *DNSClient::EDNS(const std::string &domain, const std::string &dnsServer, uint16_t port, uint32_t ip, uint64_t timeout) {
//    vector<string> domains;
//    domains.emplace_back(domain);
//    return instance.queryEDNS(domains, dnsServer, port, ip, timeout);
//}