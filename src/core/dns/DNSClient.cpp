//
// Created by codingdie on 2020/5/20.
//

#include "DNSClient.h"
#include "asio/STUtils.h"
#include <sys/socket.h>

DNSClient DNSClient::instance;
UdpDNSResponse *
DNSClient::forwardUdpDNS(UdpDnsRequest &udpDnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout) {
    return instance.forwardUdp(udpDnsRequest, dnsServer, port, timeout);
}


UdpDNSResponse *DNSClient::udpDns(const string &domain, const string &dnsServer, uint32_t port, uint64_t timeout) {
    vector<string> domains;
    domains.emplace_back(domain);
    return udpDns(domains, dnsServer, port, timeout);
}

UdpDNSResponse *
DNSClient::udpDns(const vector<string> &domains, const string &dnsServer, uint32_t port, uint64_t timeout) {
    return instance.queryUdp(domains, dnsServer, port, timeout);
}

TcpDNSResponse *DNSClient::tcpTlsDns(const string &domain, const string &dnsServer, uint16_t port, uint64_t timeout) {
    vector<string> domains;
    domains.emplace_back(domain);
    return tcpTlsDns(domains, dnsServer, port, timeout);
}

TcpDNSResponse *
DNSClient::tcpTlsDns(const vector<string> &domains, const string &dnsServer, uint16_t port, uint64_t timeout) {
    return instance.queryTcpOverTls(domains, dnsServer, port, timeout);
}


TcpDNSResponse *DNSClient::tcpDns(const string &domain, const string &dnsServer, uint16_t port, uint64_t timeout) {
    vector<string> domains;
    domains.emplace_back(domain);
    return tcpDns(domains, dnsServer, port, timeout);
}

TcpDNSResponse *
DNSClient::tcpDns(const vector<string> &domains, const string &dnsServer, uint16_t port, uint64_t timeout) {
    return instance.queryTcp(domains, dnsServer, port, timeout);
}

UdpDNSResponse *
DNSClient::queryUdp(const std::vector<string> &domains, const std::string &dnsServer, uint32_t port, uint64_t timeout) {
    if (domains.empty()) {
        return nullptr;
    }
    io_context &ctxThreadLocal = asio::TLIOContext::getIOContext();
    ctxThreadLocal.stop();
    ctxThreadLocal.reset();
    uint64_t beginTime = time::now();
    uint64_t restTime = timeout;
    udp::socket socket(ctxThreadLocal, udp::endpoint(udp::v4(), 0));
    UdpDnsRequest dnsRequest(domains);
    udp::endpoint serverEndpoint;
    unsigned short qid = dnsRequest.dnsHeader->id;
    UdpDNSResponse *dnsResponse = nullptr;
    uint16_t dnsId = dnsRequest.dnsHeader->id;
    string logTag = to_string(dnsId) + " " + dnsServer + " " + domains[0];
    auto completeHandler = [&]() {
        boost::system::error_code errorCode;
        socket.shutdown(boost::asio::socket_base::shutdown_both, errorCode);
        socket.cancel(errorCode);
        socket.close(errorCode);
        ctxThreadLocal.stop();
        ctxThreadLocal.reset();

        Logger::DEBUG << logTag << "cost" << time::now() - beginTime << END;
    };
    auto sendFuture = socket.async_send_to(buffer(dnsRequest.data, dnsRequest.len),
                                           udp::endpoint(make_address_v4(dnsServer), port),
                                           boost::asio::use_future([&](boost::system::error_code error, size_t size) {
                                               return error;
                                           }));
    bool error = true;
    if (!isTimeoutOrError(completeHandler, sendFuture, beginTime, timeout, restTime, logTag, "send")) {
        dnsResponse = new UdpDNSResponse(1024);
        size_t len = 0;
        auto receiveFuture = socket.async_receive_from(buffer(dnsResponse->data, sizeof(uint8_t) * 1024), serverEndpoint,
                                                       boost::asio::use_future(
                                                               [&](boost::system::error_code error, size_t size) {
                                                                   len = size;
                                                                   return error;
                                                               }));
        if (!isTimeoutOrError(completeHandler, receiveFuture, beginTime, timeout, restTime, logTag, "receive")) {
            dnsResponse->parse(len);
            if (dnsResponse->isValid() && dnsResponse->header->id == qid) {
                error = false;
            } else {
                Logger::ERROR << logTag << "not valid dnsResponse!" << END
            }
        }
    }
    if (error) {
        delete dnsResponse;
        dnsResponse = nullptr;
    }
    completeHandler();
    return dnsResponse;
}

UdpDNSResponse *
DNSClient::forwardUdp(UdpDnsRequest &dnsRequest, const std::string &dnsServer, uint32_t port, uint64_t timeout) {
    io_context &ctxThreadLocal = asio::TLIOContext::getIOContext();
    ctxThreadLocal.stop();
    ctxThreadLocal.reset();
    uint64_t beginTime = time::now();
    uint64_t restTime = timeout;
    udp::socket socket(ctxThreadLocal, udp::endpoint(udp::v4(), 0));
    udp::endpoint serverEndpoint;
    UdpDNSResponse *dnsResponse = nullptr;
    string logTag = "forward to " + dnsServer;
    auto completeHandler = [&]() {
        boost::system::error_code errorCode;
        socket.shutdown(boost::asio::socket_base::shutdown_both, errorCode);
        socket.cancel(errorCode);
        socket.close(errorCode);
        ctxThreadLocal.stop();
        ctxThreadLocal.reset();

        Logger::DEBUG << logTag << "cost" << time::now() - beginTime << END;
    };
    auto sendFuture = socket.async_send_to(buffer(dnsRequest.data, dnsRequest.len),
                                           udp::endpoint(make_address_v4(dnsServer), port),
                                           boost::asio::use_future([&](boost::system::error_code error, size_t size) {
                                               return error;
                                           }));
    bool error = true;
    if (!isTimeoutOrError(completeHandler, sendFuture, beginTime, timeout, restTime, logTag, "send")) {
        dnsResponse = new UdpDNSResponse(1024);
        auto receiveFuture = socket.async_receive_from(buffer(dnsResponse->data, sizeof(uint8_t) * 1024), serverEndpoint,
                                                       boost::asio::use_future(
                                                               [&](boost::system::error_code error, size_t size) {
                                                                   dnsResponse->len = size;
                                                                   return error;
                                                               }));
        if (!isTimeoutOrError(completeHandler, receiveFuture, beginTime, timeout, restTime, logTag, "receive")) {
            //todo judge dns header id
            error = false;
        }
    }
    if (error) {
        delete dnsResponse;
        dnsResponse = nullptr;
    }
    completeHandler();
    return dnsResponse;
}

DNSClient::~DNSClient() {
}

DNSClient::DNSClient() {
}


TcpDNSResponse *
DNSClient::queryTcp(const vector<string> &domains, const string &dnsServer, uint16_t port, uint64_t timeout) {
    uint64_t beginTime = time::now();
    uint64_t restTime = timeout;
    tcp::endpoint serverEndpoint(make_address_v4(dnsServer), port);
    TcpDnsRequest dnsRequest(domains);
    uint16_t dnsId = dnsRequest.dnsHeader->id;
    string logTag = to_string(dnsId) + " " + dnsServer + " " + domains[0];
    unsigned short qid = dnsId;
    io_context &ctxThreadLocal = asio::TLIOContext::getIOContext();
    ctxThreadLocal.stop();
    ctxThreadLocal.reset();
    tcp::socket socket(ctxThreadLocal);
    uint8_t dataBytes[1024];
    uint8_t lengthBytes[2];
    uint16_t length = 0;
    bool error = true;
    auto connectFuture = socket.async_connect(serverEndpoint,
                                              boost::asio::use_future([](boost::system::error_code ec) {
                                                  return ec;
                                              }));
    auto completeHandler = [&]() {
        boost::system::error_code errorCode;
        socket.shutdown(boost::asio::socket_base::shutdown_both, errorCode);
        socket.cancel(errorCode);
        socket.close(errorCode);
        ctxThreadLocal.stop();
        Logger::DEBUG << logTag << "cost" << time::now() - beginTime << END;
    };
    if (!isTimeoutOrError(completeHandler, connectFuture, beginTime, timeout, restTime, logTag, "connect")) {

        st::utils::copy(dnsRequest.data, dataBytes, 0, 0, dnsRequest.len);
        auto sendFuture = socket.async_write_some(buffer(dataBytes, dnsRequest.len),
                                                  boost::asio::use_future(
                                                          [](boost::system::error_code ec, std::size_t length) {
                                                              return ec;
                                                          }));
        if (!isTimeoutOrError(
                    completeHandler, sendFuture, beginTime, timeout, restTime, logTag, "send dns request")) {
            auto receiveLenFuture = boost::asio::async_read(socket, buffer(lengthBytes, 2),
                                                            boost::asio::use_future(
                                                                    [&](boost::system::error_code ec,
                                                                        std::size_t length) {
                                                                        return ec;
                                                                    }));
            if (!isTimeoutOrError(
                        completeHandler, receiveLenFuture, beginTime, timeout, restTime, logTag, "receive dns len")) {
                st::utils::read(lengthBytes, length);
                auto receiveDataFuture = boost::asio::async_read(socket, buffer(dataBytes, length),
                                                                 boost::asio::use_future(
                                                                         [&](boost::system::error_code ec,
                                                                             std::size_t length) {
                                                                             return ec;
                                                                         }));

                if (!isTimeoutOrError(completeHandler, receiveDataFuture, beginTime, timeout, restTime, logTag,
                                      "receive dns data")) {
                    error = false;
                }
            }
        }
    }

    TcpDNSResponse *dnsResponse = nullptr;
    if (!error && length > 0) {
        if (length < 2048) {
            dnsResponse = new TcpDNSResponse(2 + length);
            st::utils::copy(lengthBytes, dnsResponse->data, 0, 0, 2);
            st::utils::copy(dataBytes, dnsResponse->data + 2, 0, 0, length);
            dnsResponse->parse(2 + length);
            if (!dnsResponse->isValid()) {
                Logger::ERROR << dnsId << "receive unmarketable data" << END;
                error = true;
            } else {
                if (dnsResponse->udpDnsResponse != nullptr) {
                    if (dnsResponse->udpDnsResponse->header->id != qid) {
                        Logger::ERROR << dnsId << "receive not valid header id" << END;
                        error = true;
                    }
                    if (dnsResponse->udpDnsResponse->header->responseCode != 0) {
                        Logger::ERROR << dnsId << "receive error responseCode"
                                      << dnsResponse->udpDnsResponse->header->responseCode << END;
                        error = true;
                    }
                }
            }
        }
    }
    if (error) {
        if (dnsResponse != nullptr) {
            delete dnsResponse;
            dnsResponse = nullptr;
        }
    }
    completeHandler();

    return dnsResponse;
}

TcpDNSResponse *
DNSClient::queryTcpOverTls(const vector<string> &domains, const string &dnsServer, uint16_t port, uint64_t timeout) {
    uint64_t beginTime = time::now();
    uint64_t restTime = timeout;
    boost::asio::ssl::context sslCtx(boost::asio::ssl::context::sslv23_client);
    tcp::endpoint serverEndpoint(make_address_v4(dnsServer), port);
    TcpDnsRequest dnsRequest(domains);
    uint16_t dnsId = dnsRequest.dnsHeader->id;
    string logTag = to_string(dnsId) + " " + dnsServer + " " + domains[0];
    unsigned short qid = dnsId;
    io_context &ctxThreadLocal = asio::TLIOContext::getIOContext();
    ctxThreadLocal.stop();
    ctxThreadLocal.reset();
    boost::asio::ssl::stream<tcp::socket> socket(ctxThreadLocal, sslCtx);
    socket.set_verify_mode(ssl::verify_none);
    uint8_t dataBytes[1024];
    uint8_t lengthBytes[2];
    uint16_t length = 0;
    bool error = true;
    auto connectFuture = socket.lowest_layer().async_connect(serverEndpoint,
                                                             boost::asio::use_future([](boost::system::error_code ec) {
                                                                 return ec;
                                                             }));
    auto completeHandler = [&]() {
        boost::system::error_code errorCode;
        socket.lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, errorCode);
        socket.lowest_layer().cancel(errorCode);
        socket.lowest_layer().close(errorCode);
        ctxThreadLocal.stop();
        Logger::DEBUG << logTag << "cost" << time::now() - beginTime << END;
    };
    if (!isTimeoutOrError(completeHandler, connectFuture, beginTime, timeout, restTime, logTag, "connect")) {
        setSSLSocketTimeoutOpts(logTag, socket, 0);
        auto shakeFuture = socket.async_handshake(boost::asio::ssl::stream_base::client,
                                                  boost::asio::use_future([&](boost::system::error_code ec) {
                                                      return ec;
                                                  }));
        if (!isTimeoutOrError(completeHandler, shakeFuture, beginTime, timeout, restTime, logTag, "ssl handshake")) {
            st::utils::copy(dnsRequest.data, dataBytes, 0, 0, dnsRequest.len);
            auto sendFuture = socket.async_write_some(buffer(dataBytes, dnsRequest.len),
                                                      boost::asio::use_future(
                                                              [](boost::system::error_code ec, std::size_t length) {
                                                                  return ec;
                                                              }));
            if (!isTimeoutOrError(
                        completeHandler, sendFuture, beginTime, timeout, restTime, logTag, "send dns request")) {
                auto receiveLenFuture = boost::asio::async_read(socket, buffer(lengthBytes, 2),
                                                                boost::asio::use_future(
                                                                        [&](boost::system::error_code ec,
                                                                            std::size_t length) {
                                                                            return ec;
                                                                        }));
                if (!isTimeoutOrError(
                            completeHandler, receiveLenFuture, beginTime, timeout, restTime, logTag, "receive dns len")) {
                    st::utils::read(lengthBytes, length);
                    auto receiveDataFuture = boost::asio::async_read(socket, buffer(dataBytes, length),
                                                                     boost::asio::use_future(
                                                                             [&](boost::system::error_code ec,
                                                                                 std::size_t length) {
                                                                                 return ec;
                                                                             }));

                    if (!isTimeoutOrError(completeHandler, receiveDataFuture, beginTime, timeout, restTime, logTag,
                                          "receive dns data")) {
                        error = false;
                    }
                }
            }
        }
    }

    TcpDNSResponse *dnsResponse = nullptr;
    if (!error && length > 0) {
        if (length < 2048) {
            dnsResponse = new TcpDNSResponse(2 + length);
            st::utils::copy(lengthBytes, dnsResponse->data, 0, 0, 2);
            st::utils::copy(dataBytes, dnsResponse->data + 2, 0, 0, length);
            dnsResponse->parse(2 + length);
            if (!dnsResponse->isValid()) {
                Logger::ERROR << dnsId << "receive unmarketable data" << END;
                error = true;
            } else {
                if (dnsResponse->udpDnsResponse != nullptr) {
                    if (dnsResponse->udpDnsResponse->header->id != qid) {
                        Logger::ERROR << dnsId << "receive not valid header id" << END;
                        error = true;
                    }
                    if (dnsResponse->udpDnsResponse->header->responseCode != 0) {
                        Logger::ERROR << dnsId << "receive error responseCode"
                                      << dnsResponse->udpDnsResponse->header->responseCode << END;
                        error = true;
                    }
                }
            }
        }
    }
    if (error) {
        if (dnsResponse != nullptr) {
            delete dnsResponse;
            dnsResponse = nullptr;
        }
    }
    completeHandler();

    return dnsResponse;
}

void DNSClient::setSSLSocketTimeoutOpts(const string &logTag, ssl::stream<tcp::socket> &socket, uint64_t timeout) const {
    socklen_t optLen = sizeof(struct timeval);
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = timeout % 1000;
    int fd = socket.lowest_layer().native_handle();
    int error = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, optLen);
    ;
    if (error == -1) {
        Logger::ERROR << logTag << "setSSLSocketTimeoutOpts SO_SNDTIMEO error" << strerror(errno) << Logger::ENDL;
    }
    error = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, optLen);
    ;
    if (error == -1) {
        Logger::ERROR << logTag << "setSSLSocketTimeoutOpts SO_RCVTIMEO error" << strerror(errno) << Logger::ENDL;
    }
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

bool DNSClient::isTimeoutOrError(std::function<void()> completeHandler, future<boost::system::error_code> &future,
                                 uint64_t begin, uint64_t timeout, uint64_t &restTime, string logTag,
                                 const string step) {
    logTag = logTag + " " + step;
    Logger::DEBUG << logTag << "begin!" << restTime << END;
    io_context &ctxThreadLocal = asio::TLIOContext::getIOContext();
    //todo change to thread pool for not create/destroy to frequncy;
    u_int64_t traceId = Logger::traceId;
    std::thread th([=, &ctxThreadLocal]() {
        Logger::traceId = traceId;
        boost::system::error_code ec;
        ctxThreadLocal.restart();
        ctxThreadLocal.run(ec);
        if (ec) {
            Logger::ERROR << logTag << "context finish run!" << ec.message() << END;
        } else {
            Logger::DEBUG << logTag << "context finish run!" << END;
        }
    });
    future_status status = future.wait_for(std::chrono::milliseconds(restTime));
    restTime = timeout - (time::now() - begin);
    bool failed = true;
    if (restTime <= 0 || status != std::future_status::ready) {
        completeHandler();
        Logger::ERROR << logTag << "timeout!"
                      << "cost" << (time::now() - begin) << END;
    } else {
        error_code error = future.get();
        if (error) {
            completeHandler();
            Logger::ERROR << logTag << "error!" << error.message() << END
        } else {
            Logger::DEBUG << logTag << "finished!" << END;
            failed = false;
        }
    }
    th.join();
    Logger::DEBUG << logTag << "finished test!" << END;
    return failed;
}
