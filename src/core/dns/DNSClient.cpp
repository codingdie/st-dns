//
// Created by codingdie on 2020/5/20.
//

#include "DNSClient.h"
#include <boost/asio/ssl.hpp>

DNSClient DNSClient::instance;


UdpDNSResponse *DNSClient::udpDns(string &domain, string &dnsServer) {
    vector<string> domains;
    domains.emplace_back(domain);
    return instance.queryUdp(domains, dnsServer);
}

UdpDNSResponse *DNSClient::udpDns(vector<string> &domains, string &dnsServer) {
    return instance.queryUdp(domains, dnsServer);
}


TcpDNSResponse *DNSClient::tcpDns(string &domain, string &dnsServer) {
    vector<string> domains;
    domains.emplace_back(domain);
    return instance.queryTcp(domains, dnsServer);
}

TcpDNSResponse *DNSClient::tcpDns(vector<string> &domains, string &dnsServer) {
    return instance.queryTcp(domains, dnsServer);
}


DNSClient::DNSClient() {
    ioContextWork = new boost::asio::io_context::work(ioContext);
    ioThread = new thread([this]() {
        ioContext.run();
    });
}

UdpDNSResponse *DNSClient::queryUdp(vector<string> &domains, string &dnsServer) {
    udp::socket socket(ioContext, udp::endpoint(udp::v4(), 0));
    UdpDnsRequest dnsRequest(domains);
    udp::endpoint serverEndpoint;
    deadline_timer timeout(ioContext);
    unsigned short qid = dnsRequest.dnsHeader->id;
    UdpDNSResponse *dnsResponse = nullptr;
    std::future<size_t> future = socket.async_send_to(buffer(dnsRequest.data, dnsRequest.len),
                                                      udp::endpoint(make_address_v4(dnsServer), 53),
                                                      boost::asio::use_future);
    future_status status = future.wait_for(std::chrono::milliseconds(1500));
    if (status != std::future_status::ready) {
        Logger::ERROR << dnsRequest.dnsHeader->id << "connect timeout!" << END;
    } else {
        dnsResponse = new UdpDNSResponse(1024);
        std::future<size_t> receiveFuture = socket.async_receive_from(buffer(dnsResponse->data, sizeof(byte) * 1024),
                                                                      serverEndpoint, boost::asio::use_future);
        bool receiveStatus = receiveFuture.wait_for(std::chrono::milliseconds(50)) == std::future_status::ready;
        if (!receiveStatus) {
            Logger::ERROR << dnsRequest.dnsHeader->id << "receive timeout!" << END;
            delete dnsResponse;
            dnsResponse = nullptr;
        }
        size_t len = receiveFuture.get();
        dnsResponse->parse(len);
        if (!dnsResponse->isValid() || dnsResponse->header->id != qid) {
            delete dnsResponse;
            dnsResponse = nullptr;

        }
    }

    return dnsResponse;
}

DNSClient::~DNSClient() {
    delete ioContextWork;
    ioThread->join();
    delete ioThread;
}


TcpDNSResponse *DNSClient::queryTcp(vector<string> &domains, string &dnsServer) {
    const basic_endpoint<tcp> &localEndpoint = tcp::endpoint(tcp::v4(), 0);
    tcp::endpoint serverEndpoint(make_address_v4(dnsServer), 853);
    TcpDnsRequest dnsRequest(domains);
    unsigned short qid = dnsRequest.dnsHeader->id;
    TcpDNSResponse *dnsResponse = nullptr;

    ssl::context sslCtx(ssl::context::sslv23);
    sslCtx.set_default_verify_paths();

    boost::asio::ssl::stream<tcp::socket> socket(ioContext, sslCtx);
    socket.set_verify_mode(ssl::verify_none);
    auto connectFuture = socket.lowest_layer().async_connect(serverEndpoint,
                                                             boost::asio::use_future([](boost::system::error_code ec) {
                                                                 return true;
                                                             }));
    future_status connectStatus = connectFuture.wait_for(std::chrono::milliseconds(150));
    if (connectStatus != std::future_status::ready) {
        Logger::ERROR << dnsRequest.dnsHeader->id << "connect timeout!" << END;
    } else {
        auto value = connectFuture.get();
        auto shakeFuture = socket.async_handshake(boost::asio::ssl::stream_base::client,
                                                  boost::asio::use_future([](boost::system::error_code error) {
                                                      return error;
                                                  }));
        if (shakeFuture.wait_for(std::chrono::milliseconds(500)) != std::future_status::ready) {
            Logger::ERROR << dnsRequest.dnsHeader->id << "ssl handshake timeout!" << END;
        } else {
            auto sendFuture = boost::asio::async_write(socket, buffer(dnsRequest.data, dnsRequest.len),
                                                       boost::asio::use_future);
            if (sendFuture.wait_for(std::chrono::milliseconds(200)) != std::future_status::ready) {
                Logger::ERROR << dnsRequest.dnsHeader->id << "send dns request timeout!" << END;
            } else {
                byte lengthBytes[2];
                auto receiveLenFuture = boost::asio::async_read(socket, buffer(lengthBytes, 2),
                                                                boost::asio::use_future);
                if (receiveLenFuture.wait_for(std::chrono::milliseconds(200)) != std::future_status::ready) {
                    Logger::ERROR << dnsRequest.dnsHeader->id << "receive dns response len timeout!" << END;
                } else {
                    uint16_t length;
                    read(lengthBytes, length);
                    dnsResponse = new TcpDNSResponse(2 + length);
                    auto receiveDataFuture = boost::asio::async_read(socket, buffer(dnsResponse->data + 2, length),
                                                                     boost::asio::use_future);
                    copy(lengthBytes, dnsResponse->data, 0, 0, 2);
                    if (receiveDataFuture.wait_for(std::chrono::milliseconds(200)) != std::future_status::ready) {
                        Logger::ERROR << dnsRequest.dnsHeader->id << "receive dns response data timeout!" << END;
                    }
                    auto get1 = receiveDataFuture.get();
                    dnsResponse->parse(2 + length);
                    bool result = true;
                    if (!dnsResponse->isValid()) {
                        result = false;
                        Logger::ERROR << dnsRequest.dnsHeader->id << "receive unmarketable data" << END;
                    } else {
                        if (dnsResponse->udpDnsResponse != nullptr) {
                            if (dnsResponse->udpDnsResponse->header->id != qid) {
                                result = false;
                                Logger::ERROR << dnsRequest.dnsHeader->id << "receive not valid header id" << END;
                            }
                            if (dnsResponse->udpDnsResponse->header->responseCode != 0) {
                                result = false;
                                Logger::ERROR << dnsRequest.dnsHeader->id << "receive error responseCode"
                                              << dnsResponse->udpDnsResponse->header->responseCode << END;
                            }
                        }
                    }


                    if (!result) {
                        delete dnsResponse;
                        dnsResponse = nullptr;
                    }
                }
            }
        }

    }
    boost::system::error_code ec;
    socket.shutdown(ec);
    return dnsResponse;
}


