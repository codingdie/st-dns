//
// Created by codingdie on 2020/6/27.
//
#include <gtest/gtest.h>
#include "dns_client.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>


void test_dns(const string &domain, const string &server, const uint32_t port, const string &type, const unordered_set<string> areas) {
    mutex lock;
    lock.lock();
    std::vector<uint32_t> result;
    bool resultLoadAll = false;
    auto complete = [&](std::vector<uint32_t> ips, bool loadAll) {
        result = ips;
        resultLoadAll = loadAll;
        lock.unlock();
    };
    if (type == "TCP") {
        dns_client::uniq().tcp_dns(domain, server, port, 10000, areas, complete);
    } else if (type == "TCP_SSL") {
        dns_client::uniq().tcp_tls_dns(domain, server, port, 10000, areas, complete);
    } else {
        dns_client::uniq().udp_dns(domain, server, port, 200, [=](std::vector<uint32_t> ips) {
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
    test_dns(domain, server, port, type, {});
}


TEST(UnitTests, test_udp_dns) {
    auto begin = time::now();
    for (auto i = 0; i < 10; i++) {
        testDNS("baidu.com", "114.114.114.114", 53, "UDP");
    }
    logger::INFO << "test_udp_dns total cost" << time::now() - begin << END;
}


TEST(UnitTests, test_tcp_dns) {
    auto begin = time::now();
    for (auto i = 0; i < 10; i++) {
        testDNS("www.google.com", "8.8.8.8", 53, "TCP");
    }
    logger::INFO << "test_tcp_dns total cost" << time::now() - begin << END;
}

TEST(UnitTests, test_tcp_tls_dns) {
    logger::LEVEL = 0;
    auto begin = time::now();
    for (auto i = 0; i < 10; i++) {
        testDNS("www.google.com", "8.8.8.8", 853, "TCP_SSL");
    }
    logger::INFO << "test_tcp_dns total cost" << time::now() - begin << END;
}


TEST(UnitTests, test_tcp_tls_dns_resolve_multi_area) {
    logger::LEVEL = 0;
    logger::INFO << st::mem::malloc_size() << st::mem::free_size() << st::mem::leak_size() << string::npos << END;
    test_dns("www.google.com", "8.8.8.8", 853, "TCP_SSL", {"US", "JP", "HK", "TW"});
    logger::INFO << st::mem::malloc_size() << st::mem::free_size() << st::mem::leak_size() << END;
}
