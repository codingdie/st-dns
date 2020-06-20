//
// Created by codingdie on 2020/5/20.
//

#include <iostream>
#include "DNSClient.h"
#include "DNSServer.h"

#include <thread>
#include <vector>
#include <chrono>


void testTcp();

void testUdp(const string &domain, const string &server, const int count, const int parral);

using namespace std;
using namespace std::chrono;
using namespace boost::asio;

long now() {

    auto time_now = std::chrono::system_clock::now();
    auto duration_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now.time_since_epoch());
    return duration_in_ms.count();
}

template<typename FUNC>
void testParallel(FUNC testMethod, int count, int parral) {
    boost::asio::thread_pool threadPool(parral);
    long begin = now();
    atomic_int64_t successNum(0);
    for (int i = 0; i < count; i++) {
        auto task = [&]() {
            if (testMethod()) {
                successNum++;
            }
        };
        boost::asio::post(threadPool, task);
    }
    threadPool.join();
    long end = now();
    long timeToal = (end - begin);
    Logger::INFO << "total:" + to_string(timeToal) << "avg:" + to_string(timeToal * 1.0 / count)
                 << "success:" + to_string(successNum * 1.0 / count * 100) << END;
}

int main(int argc, char *argv[]) {
    testUdp("baidu.com", "192.168.31.164", 1000, 10);
}

void testUdp(const string &domain, const string &server, const int count, const int parral) {
    testParallel([=]() {
        auto dnsResponse = DNSClient::udpDns(domain, server, (uint32_t) 53);
        if (dnsResponse != nullptr) {
            Logger::INFO << "success" << dnsResponse->header->id
                         << dnsResponse->queryZone->querys.front()->domain->domain << dnsResponse->header->id
                         << ipsToStr(dnsResponse->ips) << END;
            delete dnsResponse;
            return true;
        }
        return false;
    }, count, parral);
}

void testTcp() {
    testParallel([]() {
        string domain = "baidu.com";
        string server = "8.8.8.8";
        auto tcpDnsResponse = DNSClient::tcpDns(domain, server);
        if (tcpDnsResponse != nullptr) {
            UdpDNSResponse *dnsResponse = tcpDnsResponse->udpDnsResponse;
            Logger::INFO << "success" << dnsResponse->header->id
                         << dnsResponse->queryZone->querys.front()->domain->domain << dnsResponse->header->id
                         << ipsToStr(dnsResponse->ips) << END;
            delete tcpDnsResponse;
            return true;
        }
        return false;
    }, 10, 5);
}
