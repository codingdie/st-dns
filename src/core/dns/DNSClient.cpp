//
// Created by codingdie on 2020/5/20.
//

#include "DNSClient.h"

DNSClient DNSClient::instance;


UdpDNSResponse *DNSClient::udpDns(string &domain, string &dnsServer) {
    vector<string> domains;
    domains.emplace_back(domain);
    return instance.queryUdp(domains, dnsServer);
}

UdpDNSResponse *DNSClient::udpDns(vector<string> &domains, string &dnsServer) {
    return instance.queryUdp(domains, dnsServer);
}


UdpDNSResponse *DNSClient::tcpDns(string &domain, string &dnsServer) {
    vector<string> domains;
    domains.emplace_back(domain);
    return instance.queryTcp(domains, dnsServer);
}

UdpDNSResponse *DNSClient::tcpDns(vector<string> &domains, string &dnsServer) {
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

void connect_handler(const boost::system::error_code &error) {
    if (!error) {
        // Connect succeeded.
    }
}

UdpDNSResponse *DNSClient::queryTcp(vector<string> &domains, string &dnsServer) {
    tcp::endpoint serverEndpoint(make_address_v4(dnsServer), 53);
    tcp::socket socket(ioContext, tcp::endpoint(tcp::v4(), 0));
    TcpDnsRequest dnsRequest(domains);
    UdpDnsRequest dnsRequest2(domains);
    unsigned short qid = dnsRequest.dnsHeader->id;
    UdpDNSResponse *dnsResponse = nullptr;
    std::promise<int> promiseObj;
    future<int> future = promiseObj.get_future();
    auto connectFuture = socket.async_connect(serverEndpoint, boost::asio::use_future([](boost::system::error_code ec) {
        return true;
    }));

    future_status status = connectFuture.wait_for(std::chrono::milliseconds(150));
    if (status != std::future_status::ready) {
        Logger::ERROR << dnsRequest.dnsHeader->id << "write timeout!" << END;
    } else {
        boost::asio::streambuf b(2);
        dnsResponse = new UdpDNSResponse(1024);
        byte lengthBytes[2];
//        size_t receive = socket.async_receive(buffer(lengthBytes, 2), boost::asio::use_future);


//        byte buffers[1024];
//        size_t readSize = 0;
//        size_t i = socket.read_some(buffer(buffers, sizeof(byte) * 1024), error);
//        copy(buffers, dnsResponse->data, (size_t) 0, readSize, i);
//        dnsResponse->parse(readSize);
//        if (!dnsResponse->isValid() || dnsResponse->header->id != qid) {
//            delete dnsResponse;
//            return nullptr;
//        }
    }
    return dnsResponse;
}


