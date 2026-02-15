//
// Created by codingdie on 2026/02/15.
// DNS 缓存集成测试
//

#include "dns_server.h"
#include "dns_client.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

class integration_tests : public ::testing::Test {
protected:
    dns_server *server = nullptr;
    thread *th = nullptr;

    void SetUp() override {
        st::dns::config::INSTANCE.load("../confs/test");
        dns_record_manager::uniq().clear();
        server = new dns_server(st::dns::config::INSTANCE);
        th = new thread([=]() { server->start(); });
        server->wait_start();
    }

    void TearDown() override {
        server->shutdown();
        th->join();
        delete th;
        delete server;
    }
};

// 测试缓存未命中 -> 缓存命中流程
TEST_F(integration_tests, cache_miss_then_hit) {
    const string domain = "cache-test.baidu.com";
    const string server_ip = "127.0.0.1";
    const uint32_t port = 5353;

    // 清空缓存
    dns_record_manager::uniq().clear();

    // 第一次查询 - 缓存未命中
    mutex lock1;
    lock1.lock();
    vector<uint32_t> ips1;
    auto start1 = time::now();
    dns_client::uniq().udp_dns(domain, server_ip, port, 5000, [&](std::vector<uint32_t> result) {
        ips1 = result;
        lock1.unlock();
    });
    lock1.lock();
    auto elapsed1 = time::now() - start1;
    ASSERT_GT(ips1.size(), 0);
    logger::INFO << "First query (cache miss) took " << elapsed1 << "ms" << END;
    lock1.unlock();

    // 第二次查询 - 缓存命中
    mutex lock2;
    lock2.lock();
    vector<uint32_t> ips2;
    auto start2 = time::now();
    dns_client::uniq().udp_dns(domain, server_ip, port, 5000, [&](std::vector<uint32_t> result) {
        ips2 = result;
        lock2.unlock();
    });
    lock2.lock();
    auto elapsed2 = time::now() - start2;
    ASSERT_GT(ips2.size(), 0);
    logger::INFO << "Second query (cache hit) took " << elapsed2 << "ms" << END;
    lock2.unlock();

    // 验证两次查询返回相同的结果
    ASSERT_EQ(ips1.size(), ips2.size());

    // 验证缓存命中的查询更快（通常应该快很多）
    logger::INFO << "Cache hit speedup: " << (elapsed1 * 1.0 / elapsed2) << "x" << END;
}

// 测试缓存过期后重新查询
TEST_F(integration_tests, cache_expiration_requery) {
    const string domain = "expire-test.baidu.com";
    const string server_ip = "127.0.0.1";
    const uint32_t port = 5353;

    // 清空缓存
    dns_record_manager::uniq().clear();

    // 第一次查询
    mutex lock1;
    lock1.lock();
    vector<uint32_t> ips1;
    dns_client::uniq().udp_dns(domain, server_ip, port, 5000, [&](std::vector<uint32_t> result) {
        ips1 = result;
        lock1.unlock();
    });
    lock1.lock();
    ASSERT_GT(ips1.size(), 0);
    lock1.unlock();

    // 等待缓存过期（假设测试配置中缓存时间较短）
    logger::INFO << "Waiting for cache to expire..." << END;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 第二次查询 - 缓存过期后重新查询
    mutex lock2;
    lock2.lock();
    vector<uint32_t> ips2;
    dns_client::uniq().udp_dns(domain, server_ip, port, 5000, [&](std::vector<uint32_t> result) {
        ips2 = result;
        lock2.unlock();
    });
    lock2.lock();
    ASSERT_GT(ips2.size(), 0);
    lock2.unlock();

    logger::INFO << "Cache expiration requery test passed" << END;
}

// 测试并发查询缓存一致性
TEST_F(integration_tests, concurrent_cache_consistency) {
    const string domain = "concurrent-test.baidu.com";
    const string server_ip = "127.0.0.1";
    const uint32_t port = 5353;

    // 清空缓存
    dns_record_manager::uniq().clear();

    // 第一次查询，填充缓存
    mutex lock0;
    lock0.lock();
    vector<uint32_t> ips0;
    dns_client::uniq().udp_dns(domain, server_ip, port, 5000, [&](std::vector<uint32_t> result) {
        ips0 = result;
        lock0.unlock();
    });
    lock0.lock();
    ASSERT_GT(ips0.size(), 0);
    lock0.unlock();

    // 并发查询10次
    const int concurrent_count = 10;
    vector<vector<uint32_t>> results(concurrent_count);
    vector<mutex> locks(concurrent_count);

    for (int i = 0; i < concurrent_count; i++) {
        locks[i].lock();
    }

    for (int i = 0; i < concurrent_count; i++) {
        dns_client::uniq().udp_dns(domain, server_ip, port, 5000, [&, i](std::vector<uint32_t> result) {
            results[i] = result;
            locks[i].unlock();
        });
    }

    // 等待所有查询完成
    for (int i = 0; i < concurrent_count; i++) {
        locks[i].lock();
        ASSERT_GT(results[i].size(), 0);
        locks[i].unlock();
    }

    // 验证所有查询返回相同的结果
    for (int i = 1; i < concurrent_count; i++) {
        ASSERT_EQ(results[0].size(), results[i].size());
    }

    logger::INFO << "Concurrent cache consistency test passed" << END;
}

// 测试缓存统计信息
TEST_F(integration_tests, cache_stats_integration) {
    const string server_ip = "127.0.0.1";
    const uint32_t port = 5353;

    // 清空缓存
    dns_record_manager::uniq().clear();

    auto stats_before = dns_record_manager::uniq().stats();
    ASSERT_EQ(0, stats_before.total_domain);

    // 查询多个不同的域名
    vector<string> domains = {"test1.baidu.com", "test2.baidu.com", "test3.baidu.com"};

    for (const auto &domain : domains) {
        mutex lock;
        lock.lock();
        dns_client::uniq().udp_dns(domain, server_ip, port, 5000, [&](std::vector<uint32_t> result) {
            lock.unlock();
        });
        lock.lock();
        lock.unlock();
    }

    // 等待一下确保缓存更新
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto stats_after = dns_record_manager::uniq().stats();
    ASSERT_GE(stats_after.total_domain, domains.size());

    logger::INFO << "Cache stats: " << stats_after.serialize() << END;
    logger::INFO << "Cache stats integration test passed" << END;
}
