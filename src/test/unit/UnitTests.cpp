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


void testDNS(const string &domain, const string &server, const uint32_t port, const string &type, const unordered_set<string> areas) {
    mutex lock;
    lock.lock();
    std::vector<uint32_t> result;
    bool resultLoadAll = false;
    auto complete = [&](std::vector<uint32_t> ips, bool loadAll) {
        result = ips;
        resultLoadAll = loadAll;
        lock.unlock();
    };
    if (type.compare("TCP") == 0) {
        DNSClient::INSTANCE.tcpDNS(domain, server, port, 5000, areas, complete);
    } else if (type.compare("TCP_SSL") == 0) {
        DNSClient::INSTANCE.tcpTlsDNS(domain, server, port, 10000, areas, complete);
    } else {
        DNSClient::INSTANCE.udpDns(domain, server, port, 200, [=](std::vector<uint32_t> ips) {
            complete(ips, true);
        });
    }
    lock.lock();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(result.size() > 0);
    ASSERT_TRUE(resultLoadAll);

    Logger::INFO << domain << "ips:" << st::utils::ipv4::ipsToStr(result) << st::utils::join(areas, "/") << END;
    lock.unlock();
}

void testDNS(const string &domain, const string &server, const uint32_t port, const string &type) {
    testDNS(domain, server, port, type, {});
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

TEST(UnitTests, testUdpDNS) {
    for (int i = 0; i < 3; i++) {
        testDNS("baidu.com", "114.114.114.114", 53, "UDP");
    }
}

TEST(UnitTests, testTcpTlsDNS) {
    for (int i = 0; i < 3; i++) {
        testDNS("baidu.com", "223.5.5.5", 853, "TCP_SSL");
    }
}

TEST(UnitTests, testTcpDNS) {
    for (int i = 0; i < 3; i++) {
        testDNS("baidu.com", "223.5.5.5", 53, "TCP");
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
    st::SHM::share().clear();
    uint64_t size0 = st::SHM::share().freeSize();
    ASSERT_TRUE(size0 > 1024 * 1024 * 3);
    uint64_t count = 10000;
    for (int i = 0; i < count; i++) {
        st::SHM::share().put(i, to_string(i) + "baidu.com");
    }
    ASSERT_TRUE(size0 > 1024 * 1024 * 3);

    uint64_t size01 = st::SHM::share().freeSize();
    ASSERT_TRUE(size0 > size01);

    for (int i = count; i < count * 2; i++) {
        st::SHM::share().put(i, to_string(i) + "baidu.com");
    }
    uint64_t size02 = st::SHM::share().freeSize();
    ASSERT_TRUE(size01 > size02);
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < 100; j++) {
            auto host = st::dns::SHM::share().reverse(i);
            ASSERT_STREQ((to_string(i) + "baidu.com").c_str(), host.c_str());
        }
    }

    auto testIp = count * 2 + 1;
    auto host2 = st::dns::SHM::share().reverse(testIp);
    ASSERT_STREQ(st::utils::ipv4::ipToStr(testIp).c_str(), host2.c_str());
}

TEST(UnitTests, testAreaIP) {
    ASSERT_TRUE(st::areaip::Manager::uniq().isAreaIP("TW", "118.163.193.132"));
    ASSERT_TRUE(st::areaip::Manager::uniq().isAreaIP("cn", "223.5.5.5"));
    ASSERT_TRUE(st::areaip::Manager::uniq().isAreaIP("cn", "220.181.38.148"));
    ASSERT_TRUE(st::areaip::Manager::uniq().isAreaIP("cn", "123.117.76.165"));
    ASSERT_TRUE(!st::areaip::Manager::uniq().isAreaIP("cn", "172.217.5.110"));
    ASSERT_TRUE(st::areaip::Manager::uniq().isAreaIP("us", "172.217.5.110"));
    ASSERT_TRUE(st::areaip::Manager::uniq().isAreaIP("jp", "114.48.198.220"));
    ASSERT_TRUE(!st::areaip::Manager::uniq().isAreaIP("us", "114.48.198.220"));
    ASSERT_TRUE(!st::areaip::Manager::uniq().isAreaIP("cn", "218.146.11.198"));
    ASSERT_TRUE(st::areaip::Manager::uniq().isAreaIP("kr", "218.146.11.198"));
}


TEST(UnitTests, testResolveMultiArea) {
    Logger::LEVEL = 0;
    Logger::INFO << st::mem::mallocSize() << st::mem::freeSize() << st::mem::leakSize() << string::npos << END;
    testDNS("www.google.com", "8.8.8.8", 853, "TCP_SSL", {"US", "JP", "HK", "TW"});
    Logger::INFO << st::mem::mallocSize() << st::mem::freeSize() << st::mem::leakSize() << END;
}


TEST(UnitTests, testIPAreaNetwork) {
    st::areaip::Manager::uniq().isAreaIP("JP", "14.0.42.1");
    st::areaip::Manager::uniq().isAreaIP("TW", "118.163.193.132");

    std::this_thread::sleep_for(std::chrono::seconds(5));
    bool result = st::areaip::Manager::uniq().isAreaIP("JP", "14.0.42.1");
    ASSERT_TRUE(result);
}
