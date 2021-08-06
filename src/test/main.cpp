//
// Created by codingdie on 2020/5/20.
//

#include "DNSClient.h"
#include "DNSServer.h"
#include "utils/STUtils.h"
#include <iostream>

#include "DNS.h"
#include "WhoisClient.h"
#include <boost/pool/pool.hpp>
#include <chrono>
#include <thread>
#include <vector>

void testTcp(const char *const domain, const char *const string1);

void testUdp(const string &domain, const string &server, const int count, const int parral);

void testEDNS(const char *const domain, const char *const server);


using namespace std;
using namespace std::chrono;
using namespace boost::asio;

uint64_t now() {

    auto time_now = std::chrono::system_clock::now();
    auto duration_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now.time_since_epoch());
    return duration_in_ms.count();
}

template<typename FUNC>
void testParallel(FUNC testMethod, int count, int parral) {
    boost::asio::thread_pool threadPool(parral);
    uint64_t begin = now();
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
    uint64_t end = now();
    uint64_t timeToal = (end - begin);
    Logger::INFO << "total:" + to_string(timeToal) << "avg:" + to_string(timeToal * 1.0 / count)
                 << "success:" + to_string(
                                         successNum * 1.0 / count * 100)
                 << END;
}

int main(int argc, char *argv[]) {

    // for (int i = 0; i < 100; i++) {
    //     // UDPLogger::INSTANCE.log("127.0.0.1", 30500, to_string(time::now()) + to_string(i) + "\n");
    //     UDPLogger::INSTANCE.log("127.0.0.1", 30500, "123123");
    // }
    // sleep(5000);
}


void testUdp(const string &domain, const string &server, const int count, const int parral) {
    testParallel([=]() {
        auto dnsResponse = DNSClient::udpDns(domain, server, (uint32_t) 53, 5000);
        if (dnsResponse != nullptr) {
            Logger::INFO << "success" << dnsResponse->header->id << dnsResponse->queryZone->querys.front()->domain->domain << dnsResponse->header->id << st::utils::ipv4::ipsToStr(dnsResponse->ips) << END;
            delete dnsResponse;
            return true;
        }
        return false;
    },
                 count, parral);
}

void testTcp(const char *const domain, const char *const server) {
    testParallel([=]() {
        auto tcpDnsResponse = DNSClient::tcpDns(domain, server, 53, 20000);
        if (tcpDnsResponse != nullptr) {
            UdpDNSResponse *dnsResponse = tcpDnsResponse->udpDnsResponse;
            Logger::INFO << "success" << dnsResponse->header->id
                         << dnsResponse->queryZone->querys.front()->domain->domain << dnsResponse->header->id
                         << st::utils::ipv4::ipsToStr(
                                    dnsResponse->ips)
                         << END;
            delete tcpDnsResponse;
            return true;
        }
        return false;
    },
                 300, 8);
}
//
//void testEDNS(const char *const domain, const char *const server) {
//    testParallel([=]() {
//        int cn = 1032296584;
//        int us = 96217087;
//        auto tcpDnsResponse = DNSClient::EDNS(domain, server, 53, cn, 20000);
//        if (tcpDnsResponse != nullptr) {
//            UdpDNSResponse *dnsResponse = tcpDnsResponse->udpDnsResponse;
//            Logger::INFO << "success" << dnsResponse->header->id << dnsResponse->queryZone->querys.front()->domain->domain << dnsResponse->header->id << st::utils::ipv4::ipsToStr(
//                    dnsResponse->ips) << END;
//            delete tcpDnsResponse;
//            return true;
//        }
//        return false;
//    }, 1, 1);
//}
