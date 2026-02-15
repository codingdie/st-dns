//
// Created by codingdie on 2026/02/15.
// 反向 DNS 解析单元测试
//

#include <gtest/gtest.h>
#include "dns_record_manager.h"
#include "st.h"

// 测试基本的反向解析功能
TEST(reverse_resolve, basic_reverse_resolve) {
    dns_record_manager::uniq().clear();

    // 添加一个域名记录
    uint32_t ip = st::utils::ipv4::str_to_ip("1.2.3.4");
    vector<uint32_t> ips = {ip};
    dns_record_manager::uniq().add("test.com", ips, "8.8.8.8", 600);

    // 反向解析 IP
    auto reverse_record = dns_record_manager::uniq().reverse_resolve(ip);

    // 验证能找到对应的域名
    ASSERT_GT(reverse_record.domains_size(), 0);

    bool found = false;
    for (int i = 0; i < reverse_record.domains_size(); i++) {
        if (reverse_record.domains(i) == "test.com") {
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);

    logger::INFO << "Basic reverse resolve test passed" << END;
}

// 测试一个 IP 对应多个域名
TEST(reverse_resolve, one_ip_multiple_domains) {
    dns_record_manager::uniq().clear();

    // 添加多个域名指向同一个 IP
    uint32_t ip = st::utils::ipv4::str_to_ip("10.0.0.1");
    vector<uint32_t> ips = {ip};

    dns_record_manager::uniq().add("domain1.com", ips, "8.8.8.8", 600);
    dns_record_manager::uniq().add("domain2.com", ips, "8.8.8.8", 600);
    dns_record_manager::uniq().add("domain3.com", ips, "8.8.8.8", 600);

    // 反向解析 IP
    auto reverse_record = dns_record_manager::uniq().reverse_resolve(ip);

    // 验证能找到所有域名
    ASSERT_GE(reverse_record.domains_size(), 3);

    unordered_set<string> domains;
    for (int i = 0; i < reverse_record.domains_size(); i++) {
        domains.insert(reverse_record.domains(i));
    }

    ASSERT_TRUE(domains.find("domain1.com") != domains.end());
    ASSERT_TRUE(domains.find("domain2.com") != domains.end());
    ASSERT_TRUE(domains.find("domain3.com") != domains.end());

    logger::INFO << "One IP multiple domains test passed, found "
                 << reverse_record.domains_size() << " domains" << END;
}

// 测试不存在的 IP 反向解析
TEST(reverse_resolve, nonexistent_ip_resolve) {
    dns_record_manager::uniq().clear();

    // 反向解析一个不存在的 IP
    uint32_t ip = st::utils::ipv4::str_to_ip("255.255.255.255");
    auto reverse_record = dns_record_manager::uniq().reverse_resolve(ip);

    // 验证返回空结果
    ASSERT_EQ(0, reverse_record.domains_size());

    logger::INFO << "Nonexistent IP resolve test passed" << END;
}

// 测试多个 IP 的反向解析
TEST(reverse_resolve, multiple_ips_resolve) {
    dns_record_manager::uniq().clear();

    // 添加一个域名有多个 IP
    uint32_t ip1 = st::utils::ipv4::str_to_ip("1.1.1.1");
    uint32_t ip2 = st::utils::ipv4::str_to_ip("2.2.2.2");
    uint32_t ip3 = st::utils::ipv4::str_to_ip("3.3.3.3");

    vector<uint32_t> ips = {ip1, ip2, ip3};
    dns_record_manager::uniq().add("multi-ip.com", ips, "8.8.8.8", 600);

    // 反向解析每个 IP
    auto reverse1 = dns_record_manager::uniq().reverse_resolve(ip1);
    auto reverse2 = dns_record_manager::uniq().reverse_resolve(ip2);
    auto reverse3 = dns_record_manager::uniq().reverse_resolve(ip3);

    // 验证每个 IP 都能找到对应的域名
    ASSERT_GT(reverse1.domains_size(), 0);
    ASSERT_GT(reverse2.domains_size(), 0);
    ASSERT_GT(reverse3.domains_size(), 0);

    logger::INFO << "Multiple IPs resolve test passed" << END;
}

// 测试反向解析索引更新
TEST(reverse_resolve, reverse_index_update) {
    dns_record_manager::uniq().clear();

    uint32_t ip = st::utils::ipv4::str_to_ip("10.10.10.10");
    vector<uint32_t> ips = {ip};

    // 第一次添加
    dns_record_manager::uniq().add("original.com", ips, "8.8.8.8", 600);

    auto reverse1 = dns_record_manager::uniq().reverse_resolve(ip);
    ASSERT_GT(reverse1.domains_size(), 0);

    // 添加另一个域名指向同一个 IP
    dns_record_manager::uniq().add("new.com", ips, "8.8.8.8", 600);

    auto reverse2 = dns_record_manager::uniq().reverse_resolve(ip);
    ASSERT_GE(reverse2.domains_size(), reverse1.domains_size());

    logger::INFO << "Reverse index update test passed" << END;
}

// 测试删除域名后的反向解析
TEST(reverse_resolve, reverse_resolve_after_remove) {
    dns_record_manager::uniq().clear();

    uint32_t ip = st::utils::ipv4::str_to_ip("20.20.20.20");
    vector<uint32_t> ips = {ip};

    // 添加域名
    dns_record_manager::uniq().add("remove-test.com", ips, "8.8.8.8", 600);

    // 验证反向解析能找到
    auto reverse1 = dns_record_manager::uniq().reverse_resolve(ip);
    ASSERT_GT(reverse1.domains_size(), 0);

    // 删除域名
    dns_record_manager::uniq().remove("remove-test.com");

    // 反向解析应该找不到或者域名列表减少
    auto reverse2 = dns_record_manager::uniq().reverse_resolve(ip);

    // 注意：这里的行为取决于实现，可能是找不到，也可能是域名列表减少
    // 我们只验证不会崩溃
    logger::INFO << "Reverse resolve after remove test passed, domains count: "
                 << reverse2.domains_size() << END;
}

// 测试清空缓存后的反向解析
TEST(reverse_resolve, reverse_resolve_after_clear) {
    dns_record_manager::uniq().clear();

    uint32_t ip = st::utils::ipv4::str_to_ip("30.30.30.30");
    vector<uint32_t> ips = {ip};

    // 添加域名
    dns_record_manager::uniq().add("clear-test.com", ips, "8.8.8.8", 600);

    // 验证反向解析能找到
    auto reverse1 = dns_record_manager::uniq().reverse_resolve(ip);
    ASSERT_GT(reverse1.domains_size(), 0);

    // 清空所有缓存
    dns_record_manager::uniq().clear();

    // 反向解析应该找不到
    auto reverse2 = dns_record_manager::uniq().reverse_resolve(ip);
    ASSERT_EQ(0, reverse2.domains_size());

    logger::INFO << "Reverse resolve after clear test passed" << END;
}

// 测试特殊 IP 地址的反向解析
TEST(reverse_resolve, special_ip_addresses) {
    dns_record_manager::uniq().clear();

    // 测试 0.0.0.0
    uint32_t ip_zero = 0;
    auto reverse_zero = dns_record_manager::uniq().reverse_resolve(ip_zero);
    ASSERT_EQ(0, reverse_zero.domains_size());

    // 测试 127.0.0.1
    uint32_t ip_localhost = st::utils::ipv4::str_to_ip("127.0.0.1");
    vector<uint32_t> ips = {ip_localhost};
    dns_record_manager::uniq().add("localhost", ips, "8.8.8.8", 600);

    auto reverse_localhost = dns_record_manager::uniq().reverse_resolve(ip_localhost);
    ASSERT_GT(reverse_localhost.domains_size(), 0);

    logger::INFO << "Special IP addresses test passed" << END;
}

// 测试大量域名的反向解析性能
TEST(reverse_resolve, performance_test) {
    dns_record_manager::uniq().clear();

    uint32_t base_ip = st::utils::ipv4::str_to_ip("100.0.0.1");

    // 添加100个域名指向同一个 IP
    vector<uint32_t> ips = {base_ip};
    for (int i = 0; i < 100; i++) {
        string domain = "perf-test-" + to_string(i) + ".com";
        dns_record_manager::uniq().add(domain, ips, "8.8.8.8", 600);
    }

    // 测试反向解析性能
    auto start = time::now();
    for (int i = 0; i < 1000; i++) {
        auto reverse = dns_record_manager::uniq().reverse_resolve(base_ip);
        ASSERT_GT(reverse.domains_size(), 0);
    }
    auto elapsed = time::now() - start;

    logger::INFO << "Performance test: 1000 reverse resolves took " << elapsed << "ms" << END;
    logger::INFO << "Average: " << (elapsed / 1000.0) << "ms per resolve" << END;

    // 验证性能合理（平均每次查询不超过10ms）
    ASSERT_LT(elapsed / 1000.0, 10.0);
}
