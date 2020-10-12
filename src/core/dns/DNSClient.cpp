//
// Created by codingdie on 2020/5/20.
//

#include "DNSClient.h"
#include "asio/STUtils.h"

static mutex rLock;

DNSClient DNSClient::instance;


UdpDNSResponse *DNSClient::udpDns(const string &domain, const string &dnsServer, uint32_t port, uint64_t timeout) {
    vector<string> domains;
    domains.emplace_back(domain);
    return udpDns(domains, dnsServer, port, timeout);
}

UdpDNSResponse *DNSClient::udpDns(const vector<string> &domains, const string &dnsServer, uint32_t port, uint64_t timeout) {
    return instance.queryUdp(domains, dnsServer, port, timeout);
}

TcpDNSResponse *DNSClient::tcpDns(const string &domain, const string &dnsServer, uint16_t port, uint64_t timeout) {
    vector<string> domains;
    domains.emplace_back(domain);
    return tcpDns(domains, dnsServer, port, timeout);
}

TcpDNSResponse *DNSClient::tcpDns(const vector<string> &domains, const string &dnsServer, uint16_t port, uint64_t timeout) {
    return instance.queryTcpOverTls(domains, dnsServer, port, timeout);
}

UdpDNSResponse *DNSClient::queryUdp(const std::vector<string> &domains, const std::string &dnsServer, uint32_t port, uint64_t timeout) {
    udp::socket socket(ioContext, udp::endpoint(udp::v4(), 0));
    UdpDnsRequest dnsRequest(domains);
    udp::endpoint serverEndpoint;
    unsigned short qid = dnsRequest.dnsHeader->id;
    UdpDNSResponse *dnsResponse = nullptr;
    long begin = time::now();
    std::future<size_t> future = socket.async_send_to(buffer(dnsRequest.data, dnsRequest.len), udp::endpoint(make_address_v4(dnsServer), port),
                                                      boost::asio::use_future);
    future_status status = future.wait_for(std::chrono::milliseconds(timeout));
    if (status != std::future_status::ready) {
        Logger::ERROR << dnsRequest.dnsHeader->id << "connect timeout!" << END;
    } else {
        timeout -= (time::now() - begin);
        begin = time::now();
        dnsResponse = new UdpDNSResponse(1024);

        std::future<size_t> receiveFuture = socket.async_receive_from(buffer(dnsResponse->data, sizeof(byte) * 1024), serverEndpoint,
                                                                      boost::asio::use_future);
        bool receiveStatus = receiveFuture.wait_for(std::chrono::milliseconds(timeout)) == std::future_status::ready;
        if (!receiveStatus) {
            Logger::ERROR << dnsRequest.dnsHeader->id << "receive timeout!" << END;
            delete dnsResponse;
            dnsResponse = nullptr;
        } else {
            size_t len = receiveFuture.get();
            dnsResponse->parse(len);
            if (!dnsResponse->isValid() || dnsResponse->header->id != qid) {
                delete dnsResponse;
                dnsResponse = nullptr;
            }
        }

    }
    try {
        socket.cancel();
        socket.close();
    } catch (boost::wrapexcept<boost::system::system_error> ex) {
        Logger::ERROR << dnsRequest.dnsHeader->id << ex.what() << END;
    }

    return dnsResponse;
}

DNSClient::~DNSClient() {
    delete ioContextWork;
    for (auto &it:ioThreads) {
        it.join();
    }
}

DNSClient::DNSClient() {
    ioContextWork = new boost::asio::io_context::work(ioContext);
    for (int i = 0; i < 10; i++) {
        ioThreads.emplace_back([&]() {
            ioContext.run();
        });
    }

}

TcpDNSResponse *DNSClient::queryTcpOverTls(const vector<string> &domains, const string &dnsServer, uint16_t port, uint64_t timeout) {
    {
        lock_guard<mutex> lockGuard(rLock);
        if (hostsInQuery.find(domains[0]) != hostsInQuery.end()) {
            return nullptr;
        } else {
            hostsInQuery.emplace(domains[0]);
        }
    }
    boost::asio::ssl::context sslCtx(boost::asio::ssl::context::sslv23_client);
    tcp::endpoint serverEndpoint(make_address_v4(dnsServer), port);
    TcpDnsRequest dnsRequest(domains);
    unsigned short qid = dnsRequest.dnsHeader->id;
    io_context &ctxThreadLocal = asio::TLIOContext::getIOContext();
    boost::asio::ssl::stream<tcp::socket> socket(ctxThreadLocal, sslCtx);
    socket.set_verify_mode(ssl::verify_none);
    byte dataBytes[1024];
    byte lengthBytes[2];
    uint16_t length = 0;
    bool error = false;
    long begin = time::now();
    auto connectFuture = socket.lowest_layer().async_connect(serverEndpoint, boost::asio::use_future([](boost::system::error_code ec) {
        if (ec.failed()) {
            Logger::ERROR << "connect error!" << ec.message() << END;
            return false;
        }
        return true;
    }));
    ctxThreadLocal.restart();
    ctxThreadLocal.run();
    future_status connectStatus = connectFuture.wait_for(std::chrono::milliseconds(timeout));
    timeout -= (time::now() - begin);
    begin = time::now();
    if (timeout < 0 || connectStatus != std::future_status::ready || connectFuture.get() == false) {
        Logger::ERROR << dnsRequest.dnsHeader->id << "connect timeout!" << END;
        error = true;
    } else {
        auto shakeFuture = socket.async_handshake(boost::asio::ssl::stream_base::client, boost::asio::use_future([](boost::system::error_code error) {
            if (error.failed()) {
                Logger::ERROR << "ssl handshake error!" << error.message() << END;
                return false;
            }
            return true;
        }));
        ctxThreadLocal.restart();
        ctxThreadLocal.run();
        bool isTimeout = shakeFuture.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::ready;
        timeout -= (time::now() - begin);
        begin = time::now();
        if (timeout < 0 || isTimeout || shakeFuture.get() == false) {
            Logger::ERROR << dnsRequest.dnsHeader->id << "ssl handshake timeout!" << END
            error = true;
        } else { ;
            st::utils::copy(dnsRequest.data, dataBytes, 0, 0, dnsRequest.len);
            auto sendFuture = socket.async_write_some(buffer(dataBytes, dnsRequest.len),
                                                      boost::asio::use_future([](boost::system::error_code error, std::size_t length) {
                                                          if (error.failed()) {
                                                              Logger::ERROR << "ssl async_write_some error!" << error.message() << END
                                                              return false;
                                                          }
                                                          return true;
                                                      }));
            ctxThreadLocal.restart();
            ctxThreadLocal.run();
            isTimeout = sendFuture.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::ready;
            timeout -= (time::now() - begin);
            begin = time::now();
            if (timeout < 0 || isTimeout || sendFuture.get() == false) {
                Logger::ERROR << dnsRequest.dnsHeader->id << "send dns request timeout!" << END;
                error = true;
            } else {
                auto receiveLenFuture = boost::asio::async_read(socket, buffer(lengthBytes, 2),
                                                                boost::asio::use_future([](boost::system::error_code error, std::size_t length) {
                                                                    if (error.failed()) {
                                                                        Logger::ERROR << "ssl async_read error!" << END;
                                                                        return false;
                                                                    }
                                                                    return true;
                                                                }));
                ctxThreadLocal.restart();
                ctxThreadLocal.run();
                isTimeout = receiveLenFuture.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::ready;
                timeout -= (time::now() - begin);
                begin = time::now();
                if (timeout < 0 || isTimeout || receiveLenFuture.get() == false) {
                    Logger::ERROR << dnsRequest.dnsHeader->id << "receive dns response len timeout!" << END;
                    error = true;
                } else {
                    st::utils::read(lengthBytes, length);
                    auto receiveDataFuture = boost::asio::async_read(socket, buffer(dataBytes, length),
                                                                     boost::asio::use_future([](boost::system::error_code error, std::size_t length) {
                                                                         if (error.failed()) {
                                                                             Logger::ERROR << "ssl async_read error!" << END;
                                                                             return false;
                                                                         }
                                                                         return true;
                                                                     }));
                    ctxThreadLocal.restart();
                    ctxThreadLocal.run();
                    if (receiveDataFuture.wait_for(
                            std::chrono::milliseconds(timeout)) != std::future_status::ready || receiveDataFuture.get() == false) {
                        Logger::ERROR << dnsRequest.dnsHeader->id << "receive dns response data timeout!" << END;
                        error = true;
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
                Logger::ERROR << dnsRequest.dnsHeader->id << "receive unmarketable data" << END;
                error = true;
            } else {
                if (dnsResponse->udpDnsResponse != nullptr) {
                    if (dnsResponse->udpDnsResponse->header->id != qid) {
                        Logger::ERROR << dnsRequest.dnsHeader->id << "receive not valid header id" << END;
                        error = true;
                    }
                    if (dnsResponse->udpDnsResponse->header->responseCode != 0) {
                        Logger::ERROR << dnsRequest.dnsHeader->id << "receive error responseCode" << dnsResponse->udpDnsResponse->header->responseCode << END;
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
    boost::system::error_code errorCode;
    socket.shutdown(errorCode);
    socket.lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, errorCode);
    socket.lowest_layer().cancel(errorCode);
    socket.lowest_layer().close(errorCode);
    hostsInQuery.erase(domains[0]);
    return dnsResponse;
}

