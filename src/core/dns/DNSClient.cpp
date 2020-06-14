//
// Created by codingdie on 2020/5/20.
//

#include "DNSClient.h"

DNSClient DNSClient::instance;


UdpDNSResponse *DNSClient::udpDns(const std::string &domain, const string &dnsServer) {
    vector<string> domains;
    domains.emplace_back(domain);
    return instance.udpDns(domains, dnsServer, DEFAULT_DNS_PORT);
}

UdpDNSResponse *DNSClient::udpDns(vector<string> &domains, const string &dnsServer) {
    return instance.udpDns(domains, dnsServer, DEFAULT_DNS_PORT);
}

UdpDNSResponse *DNSClient::udpDns(const std::string &domain, const string &dnsServer, uint32_t port) {
    vector<string> domains;
    domains.emplace_back(domain);
    return instance.queryUdp(domains, dnsServer, port);
}

UdpDNSResponse *DNSClient::udpDns(vector<string> &domains, const string &dnsServer, uint32_t port) {
    return instance.queryUdp(domains, dnsServer, port);
}


TcpDNSResponse *DNSClient::tcpDns(const string &domain, const string &dnsServer) {
    return tcpDns(domain, dnsServer, (uint32_t) 5000);
}

TcpDNSResponse *DNSClient::tcpDns(const string &domain, const string &dnsServer, uint32_t timeout) {
    vector<string> domains;
    domains.emplace_back(domain);
    return tcpDns(domains, dnsServer, timeout);
}

TcpDNSResponse *DNSClient::tcpDns(vector<string> &domains, const string &dnsServer) {
    return tcpDns(domains, dnsServer, (uint32_t) 5000);
}


TcpDNSResponse *DNSClient::tcpDns(vector<string> &domains, const string &dnsServer, uint32_t timout) {
    return instance.queryTcp(domains, dnsServer, timout);
}


UdpDNSResponse *DNSClient::queryUdp(vector<string> &domains, const string &dnsServer, uint32_t port) {
    udp::socket socket(ioContext, udp::endpoint(udp::v4(), 0));
    UdpDnsRequest dnsRequest(domains);
    udp::endpoint serverEndpoint;
    deadline_timer timeout(ioContext);
    unsigned short qid = dnsRequest.dnsHeader->id;
    UdpDNSResponse *dnsResponse = nullptr;
    std::future<size_t> future = socket.async_send_to(buffer(dnsRequest.data, dnsRequest.len),
                                                      udp::endpoint(make_address_v4(dnsServer), port),
                                                      boost::asio::use_future);
    future_status status = future.wait_for(std::chrono::milliseconds(1500));
    if (status != std::future_status::ready) {
        Logger::ERROR << dnsRequest.dnsHeader->id << "connect timeout!" << END;
    } else {
        dnsResponse = new UdpDNSResponse(1024);
        std::future<size_t> receiveFuture = socket.async_receive_from(buffer(dnsResponse->data, sizeof(byte) * 1024),
                                                                      serverEndpoint, boost::asio::use_future);
        bool receiveStatus = receiveFuture.wait_for(std::chrono::milliseconds(5000)) == std::future_status::ready;
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
    ioThread->join();
    delete ioThread;
    delete sslCtx;
}

DNSClient::DNSClient() {
    sslCtx = new boost::asio::ssl::context(boost::asio::ssl::context::sslv23);
    ioContextWork = new boost::asio::io_context::work(ioContext);
    ioThread = new thread([this]() {
        ioContext.run();
    });
}

TcpDNSResponse *DNSClient::queryTcp(vector<string> &domains, const string &dnsServer) {
    return queryTcp(domains, dnsServer, 5000);
}

TcpDNSResponse *DNSClient::queryTcp(vector<string> &domains, const string &dnsServer, uint32_t timeout) {
    const basic_endpoint<tcp> localEndpoint = tcp::endpoint(tcp::v4(), 0);
    tcp::endpoint serverEndpoint(make_address_v4(dnsServer), 853);
    TcpDnsRequest dnsRequest(domains);
    unsigned short qid = dnsRequest.dnsHeader->id;
    boost::asio::ssl::stream<tcp::socket> socket(ioContext, *sslCtx);
    socket.set_verify_mode(ssl::verify_none);
    byte dataBytes[1024];
    byte lengthBytes[2];
    uint16_t length = 0;
    bool error = false;
    long begin = time::now();
    auto connectFuture = socket.lowest_layer().async_connect(serverEndpoint,
                                                             boost::asio::use_future([](boost::system::error_code ec) {
                                                                 return true;
                                                             }));
    future_status connectStatus = connectFuture.wait_for(std::chrono::milliseconds(timeout));
    if (connectStatus != std::future_status::ready) {
        Logger::ERROR << dnsRequest.dnsHeader->id << "connect timeout!" << END;
        error = true;
    } else {
        timeout -= (time::now() - begin);
        begin = time::now();
        auto value = connectFuture.get();
        auto shakeFuture = socket.async_handshake(boost::asio::ssl::stream_base::client,
                                                  boost::asio::use_future([](boost::system::error_code error) {
                                                      return error;
                                                  }));
        if (shakeFuture.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::ready) {
            Logger::ERROR << dnsRequest.dnsHeader->id << "ssl handshake timeout!" << END;
            error = true;
        } else {
            timeout -= (time::now() - begin);
            begin = time::now();
            auto sendFuture = boost::asio::async_write(socket, buffer(dnsRequest.data, dnsRequest.len),
                                                       boost::asio::use_future);
            if (sendFuture.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::ready) {
                Logger::ERROR << dnsRequest.dnsHeader->id << "send dns request timeout!" << END;
                error = true;
            } else {
                timeout -= (time::now() - begin);
                begin = time::now();
                auto receiveLenFuture = boost::asio::async_read(socket, buffer(lengthBytes, 2),
                                                                boost::asio::use_future);
                if (receiveLenFuture.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::ready) {
                    Logger::ERROR << dnsRequest.dnsHeader->id << "receive dns response len timeout!" << END;
                    error = true;
                } else {
                    timeout -= (time::now() - begin);
                    begin = time::now();
                    st::utils::read(lengthBytes, length);
                    auto receiveDataFuture = boost::asio::async_read(socket, buffer(dataBytes, length),
                                                                     boost::asio::use_future);
                    if (receiveDataFuture.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::ready) {
                        Logger::ERROR << dnsRequest.dnsHeader->id << "receive dns response data timeout!" << END;
                        error = true;
                    }
                }
            }
        }


    }

    try {
        socket.lowest_layer().cancel();
        socket.lowest_layer().shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
        socket.lowest_layer().close();
    } catch (boost::wrapexcept<boost::system::system_error> ex) {
        Logger::ERROR << dnsRequest.dnsHeader->id << ex.what() << END;
    }

    TcpDNSResponse *dnsResponse = nullptr;
    if (!error && length > 0) {
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
                    Logger::ERROR << dnsRequest.dnsHeader->id << "receive error responseCode"
                                  << dnsResponse->udpDnsResponse->header->responseCode << END;
                    error = true;
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
    return dnsResponse;
}


