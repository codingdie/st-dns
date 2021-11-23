//
// Created by codingdie on 2020/6/27.
//
#include <gtest/gtest.h>
#include "DNSClient.h"
#include "DNSServer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

void testDNS(const string &domain, const string &server, const uint32_t port, const string &type) {
    mutex lock;
    lock.lock();
    UdpDNSResponse *result = nullptr;
    if (type.compare("TCP") == 0) {
        DNSClient::INSTANCE.tcpDNS(domain, server, port, 1000, [&](TcpDNSResponse *dnsResponse) {
            result = dnsResponse->udpDnsResponse;
            lock.unlock();
        });
    } else if (type.compare("TCP_SSL") == 0) {
        DNSClient::INSTANCE.tcpTlsDNS(domain, server, port, 1000, [&](TcpDNSResponse *dnsResponse) {
            result = dnsResponse->udpDnsResponse;
            lock.unlock();
        });
    } else {
        DNSClient::INSTANCE.udpDns(domain, server, port, 5000, [&](UdpDNSResponse *dnsResponse) {
            result = dnsResponse;
            lock.unlock();
        });
    }
    lock.lock();
    ASSERT_TRUE(result != nullptr);

    if (result != nullptr) {
        string ips = st::utils::ipv4::ipsToStr(result->ips);
        Logger::INFO << domain << "ips:" << ips << END;
        delete result;
        ASSERT_STRNE("", ips.c_str()) << "expect not empty";
    }
    ASSERT_TRUE(result != nullptr);
    lock.unlock();
}


template<typename FUNC>
void testParallel(FUNC testMethod, uint32_t count, uint32_t parral) {
    boost::asio::thread_pool threadPool(parral);
    atomic<uint32_t> taskCount(0);
    for (int i = 0; i < count; i++) {
        auto task = [=, &taskCount]() {
            Logger::traceId = 1000;
            testMethod();
            taskCount++;
        };
        boost::asio::post(threadPool, task);
    }
    while (taskCount != count) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
TEST(UnitTests, testPool) {
    vector<pair<uint8_t *, uint32_t>> datas;
    testParallel([]() {
        auto pair = st::mem::pmalloc(1024);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        st::mem::pfree(pair);
    },
                 1000, 100);
}

TEST(UnitTests, testTcpDNS) {
    for (int i = 0; i < 10; i++) {
        testDNS("baidu.com", "114.114.114.114", 53, "UDP");
    }
}

TEST(UnitTests, testTcpTlsDNS) {
    for (int i = 0; i < 10; i++) {
        testDNS("baidu.com", "223.5.5.5", 853, "TCP_SSL");
    }
}

TEST(UnitTests, testUdpDNS) {
    for (int i = 0; i < 10; i++) {
        testDNS("baidu.com", "114.114.114.114", 53, "TCP");
    }
}

TEST(UnitTests, testBase64) {
    string oriStr = "asdkhb123b";
    string base64Str = st::utils::base64::encode(oriStr);
    ASSERT_STREQ("YXNka2hiMTIzYg==", base64Str.c_str());
    string decodeStr = st::utils::base64::decode(base64Str);
    ASSERT_STREQ(oriStr.c_str(), decodeStr.c_str());
}

TEST(UnitTests, testSHM) {
    int count = 100;
    st::utils::dns::DNSReverseSHM dnsSHM(false);
    st::utils::dns::DNSReverseSHM dnsSHMRead;
    for (int i = 0; i < count; i++) {
        dnsSHM.addOrUpdate(i, to_string(i) + "baidu.com");
    }
    for (int i = 0; i < count; i++) {
        auto host = dnsSHMRead.query(i);
        ASSERT_STREQ((to_string(i) + "baidu.com").c_str(), host.c_str());
    }

    auto testIp = count + 1;
    auto host2 = dnsSHMRead.query(testIp);
    ASSERT_STREQ(st::utils::ipv4::ipToStr(testIp).c_str(), host2.c_str());
}