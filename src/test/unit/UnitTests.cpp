//
// Created by codingdie on 2020/6/27.
//
#include <gtest/gtest.h>
#include "dns_client.h"
#include "dns_server.h"
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
        dns_client::INSTANCE.tcp_dns(domain, server, port, 5000, areas, complete);
    } else if (type.compare("TCP_SSL") == 0) {
        dns_client::INSTANCE.tcp_tls_dns(domain, server, port, 10000, areas, complete);
    } else {
        dns_client::INSTANCE.udp_dns(domain, server, port, 200, [=](std::vector<uint32_t> ips) {
            complete(ips, true);
        });
    }
    lock.lock();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(result.size() > 0);
    ASSERT_TRUE(resultLoadAll);

    logger::INFO << domain << "ips:" << st::utils::ipv4::ips_to_str(result) << st::utils::join(areas, "/") << END;
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
            logger::traceId = 1000;
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
    auto ns = "TEST";
    st::shm::kv::create(ns, 5 * 1024 * 1024);
    st::shm::kv::share(ns)->clear();
    uint64_t size0 = st::shm::kv::share(ns)->free_size();
    ASSERT_TRUE(size0 > 1024 * 1024 * 3);
    int count = 10000;
    for (int i = 0; i < count; i++) {
        st::shm::kv::share(ns)->put(i, to_string(i) + "baidu.com");
    }
    ASSERT_TRUE(size0 > 1024 * 1024 * 3);

    uint64_t size01 = st::shm::kv::share(ns)->free_size();
    ASSERT_TRUE(size0 > size01);

    for (int i = count; i < count * 2; i++) {
        st::shm::kv::share(ns)->put(i, to_string(i) + "baidu.com");
    }
    uint64_t size02 = st::shm::kv::share(ns)->free_size();
    ASSERT_TRUE(size01 > size02);
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < 100; j++) {
            auto host = st::shm::kv::share(ns)->get(to_string(i));
            ASSERT_STREQ((to_string(i) + "baidu.com").c_str(), host.c_str());
        }
    }

    auto testIp = count * 2 + 1;
    auto host2 = st::dns::shm::share().reverse_resolve(testIp);
    ASSERT_STREQ(st::utils::ipv4::ip_to_str(testIp).c_str(), host2.c_str());
}

TEST(UnitTests, testAreaIP) {
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("TW", "118.163.193.132"));
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("cn", "223.5.5.5"));
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("cn", "220.181.38.148"));
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("cn", "123.117.76.165"));
    ASSERT_TRUE(!st::areaip::manager::uniq().is_area_ip("cn", "172.217.5.110"));
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("us", "172.217.5.110"));
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("jp", "114.48.198.220"));
    ASSERT_TRUE(!st::areaip::manager::uniq().is_area_ip("us", "114.48.198.220"));
    ASSERT_TRUE(!st::areaip::manager::uniq().is_area_ip("cn", "218.146.11.198"));
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("kr", "218.146.11.198"));
}


TEST(UnitTests, testResolveMultiArea) {
    logger::LEVEL = 0;
    logger::INFO << st::mem::malloc_size() << st::mem::free_size() << st::mem::leak_size() << string::npos << END;
    testDNS("www.google.com", "8.8.8.8", 853, "TCP_SSL", {"US", "JP", "HK", "TW"});
    logger::INFO << st::mem::malloc_size() << st::mem::free_size() << st::mem::leak_size() << END;
}


TEST(UnitTests, testIPAreaNetwork) {
    st::areaip::manager::uniq().is_area_ip("JP", "14.0.42.1");
    st::areaip::manager::uniq().is_area_ip("TW", "118.163.193.132");

    std::this_thread::sleep_for(std::chrono::seconds(5));
    bool result = st::areaip::manager::uniq().is_area_ip("JP", "14.0.42.1");
    ASSERT_TRUE(result);
}
