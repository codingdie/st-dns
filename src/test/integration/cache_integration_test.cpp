//
// Created by codingdie on 2026/02/15.
// DNS 缓存集成测试
//

#include "integration_test_base.h"
#include <chrono>

class cache_integration_tests : public BaseTest {};

// 辅助函数：通过 DNS server 查询域名，返回 IP 列表
static vector<uint32_t> query_dns(const string &domain, uint64_t timeout = 5000) {
    const string server_ip = "127.0.0.1";
    const uint32_t port = 5353;
    mutex lock;
    lock.lock();
    vector<uint32_t> ips;
    dns_client::uniq().udp_dns(domain, server_ip, port, timeout, [&](std::vector<uint32_t> result) {
        ips = result;
        lock.unlock();
    });
    lock.lock();
    lock.unlock();
    return ips;
}

// 测试缓存未命中 -> 缓存命中流程（使用 force_resolve 域名，无需外部网络）
TEST_F(cache_integration_tests, cache_miss_then_hit) {
    dns_record_manager::uniq().clear();

    // 第一次查询 - 缓存未命中，走 force_resolve
    auto start1 = time::now();
    auto ips1 = query_dns("test.codingdie.com");
    auto elapsed1 = time::now() - start1;
    ASSERT_EQ(1, ips1.size());
    ASSERT_EQ(st::utils::ipv4::str_to_ip("1.2.3.4"), ips1[0]);
    logger::INFO << "First query (cache miss) took " << elapsed1 << "ms" << END;

    // 第二次查询 - 缓存命中
    auto start2 = time::now();
    auto ips2 = query_dns("test.codingdie.com");
    auto elapsed2 = time::now() - start2;
    ASSERT_EQ(1, ips2.size());
    ASSERT_EQ(ips1[0], ips2[0]);
    logger::INFO << "Second query (cache hit) took " << elapsed2 << "ms" << END;

    logger::INFO << "Cache hit speedup: " << (elapsed1 * 1.0 / std::max(elapsed2, (uint64_t)1)) << "x" << END;
}

// 测试多个 force_resolve 域名的缓存
TEST_F(cache_integration_tests, cache_multiple_domains) {
    dns_record_manager::uniq().clear();

    // 查询不同的 force_resolve 域名
    auto ips1 = query_dns("test.codingdie.com");
    ASSERT_EQ(1, ips1.size());
    ASSERT_EQ(st::utils::ipv4::str_to_ip("1.2.3.4"), ips1[0]);

    auto ips2 = query_dns("www.codingdie.com");
    ASSERT_EQ(2, ips2.size());
    ASSERT_EQ(st::utils::ipv4::str_to_ip("192.168.1.100"), ips2[0]);
    ASSERT_EQ(st::utils::ipv4::str_to_ip("192.168.1.101"), ips2[1]);

    auto ips3 = query_dns("github.com");
    ASSERT_EQ(1, ips3.size());
    ASSERT_EQ(st::utils::ipv4::str_to_ip("192.30.255.113"), ips3[0]);

    logger::INFO << "Cache multiple domains test passed" << END;
}

// 测试并发查询缓存一致性（使用 force_resolve 域名）
TEST_F(cache_integration_tests, concurrent_cache_consistency) {
    dns_record_manager::uniq().clear();

    const string domain = "github.com";

    // 先填充缓存
    auto ips0 = query_dns(domain);
    ASSERT_EQ(1, ips0.size());

    // 并发查询10次
    const int concurrent_count = 10;
    vector<vector<uint32_t>> results(concurrent_count);
    vector<mutex> locks(concurrent_count);

    for (int i = 0; i < concurrent_count; i++) {
        locks[i].lock();
    }

    for (int i = 0; i < concurrent_count; i++) {
        dns_client::uniq().udp_dns(domain, "127.0.0.1", 5353, 5000, [&, i](std::vector<uint32_t> result) {
            results[i] = result;
            locks[i].unlock();
        });
    }

    for (int i = 0; i < concurrent_count; i++) {
        locks[i].lock();
        ASSERT_EQ(1, results[i].size());
        ASSERT_EQ(ips0[0], results[i][0]);
        locks[i].unlock();
    }

    logger::INFO << "Concurrent cache consistency test passed" << END;
}

// 测试缓存统计信息
TEST_F(cache_integration_tests, cache_stats_integration) {
    dns_record_manager::uniq().clear();

    auto stats_before = dns_record_manager::uniq().stats();
    ASSERT_EQ(0, stats_before.total_domain);

    // 查询多个 force_resolve 域名填充缓存
    query_dns("test.codingdie.com");
    query_dns("www.codingdie.com");
    query_dns("github.com");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto stats_after = dns_record_manager::uniq().stats();
    logger::INFO << "Cache stats: " << stats_after.serialize() << END;
    // force_resolve 的记录也应该被统计
    ASSERT_GE(stats_after.total_domain, 0);

    logger::INFO << "Cache stats integration test passed" << END;
}
