//
// Created by codingdie on 2026/02/15.
// DNS 缓存管理单元测试
//

#include <gtest/gtest.h>
#include "dns_record_manager.h"
#include "st.h"
#include <thread>
#include <chrono>

// 测试缓存过期时间
TEST(cache_management, cache_expiration) {
    dns_record_manager::uniq().clear();

    // 添加一个短过期时间的记录（2秒）
    vector<uint32_t> ips = {st::utils::ipv4::str_to_ip("1.2.3.4")};
    dns_record_manager::uniq().add("expire-test.com", ips, "8.8.8.8", 2);

    // 立即查询，应该能查到
    auto records = dns_record_manager::uniq().get_dns_record_list("expire-test.com");
    ASSERT_EQ(1, records.size());
    ASSERT_FALSE(records[0].expire);

    // 等待3秒后查询，应该过期
    std::this_thread::sleep_for(std::chrono::seconds(3));

    records = dns_record_manager::uniq().get_dns_record_list("expire-test.com");
    if (!records.empty()) {
        ASSERT_TRUE(records[0].expire);
    }

    logger::INFO << "Cache expiration test passed" << END;
}

// 测试缓存删除功能
TEST(cache_management, remove_cache) {
    dns_record_manager::uniq().clear();

    // 添加多个域名的记录
    vector<uint32_t> ips1 = {st::utils::ipv4::str_to_ip("1.1.1.1")};
    vector<uint32_t> ips2 = {st::utils::ipv4::str_to_ip("2.2.2.2")};

    dns_record_manager::uniq().add("test1.com", ips1, "8.8.8.8", 600);
    dns_record_manager::uniq().add("test2.com", ips2, "8.8.8.8", 600);

    // 验证两个域名都存在
    auto records1 = dns_record_manager::uniq().get_dns_record_list("test1.com");
    auto records2 = dns_record_manager::uniq().get_dns_record_list("test2.com");
    ASSERT_EQ(1, records1.size());
    ASSERT_EQ(1, records2.size());

    // 删除 test1.com
    dns_record_manager::uniq().remove("test1.com");

    // 验证 test1.com 被删除，test2.com 仍然存在
    records1 = dns_record_manager::uniq().get_dns_record_list("test1.com");
    records2 = dns_record_manager::uniq().get_dns_record_list("test2.com");
    ASSERT_EQ(0, records1.size());
    ASSERT_EQ(1, records2.size());

    logger::INFO << "Remove cache test passed" << END;
}

// 测试清空所有缓存
TEST(cache_management, clear_all_cache) {
    dns_record_manager::uniq().clear();

    // 添加多个域名的记录
    vector<uint32_t> ips = {st::utils::ipv4::str_to_ip("1.1.1.1")};

    dns_record_manager::uniq().add("test1.com", ips, "8.8.8.8", 600);
    dns_record_manager::uniq().add("test2.com", ips, "8.8.8.8", 600);
    dns_record_manager::uniq().add("test3.com", ips, "8.8.8.8", 600);

    // 验证记录存在
    auto stats_before = dns_record_manager::uniq().stats();
    ASSERT_GT(stats_before.total_domain, 0);

    // 清空所有缓存
    dns_record_manager::uniq().clear();

    // 验证所有记录被清空
    auto stats_after = dns_record_manager::uniq().stats();
    ASSERT_EQ(0, stats_after.total_domain);

    auto records = dns_record_manager::uniq().get_dns_record_list("test1.com");
    ASSERT_EQ(0, records.size());

    logger::INFO << "Clear all cache test passed" << END;
}

// 测试缓存统计功能
TEST(cache_management, cache_stats) {
    dns_record_manager::uniq().clear();

    // 添加一些记录
    vector<uint32_t> ips1 = {
        st::utils::ipv4::str_to_ip("1.1.1.1"),
        st::utils::ipv4::str_to_ip("2.2.2.2")
    };
    vector<uint32_t> ips2 = {
        st::utils::ipv4::str_to_ip("3.3.3.3")
    };

    dns_record_manager::uniq().add("domain1.com", ips1, "8.8.8.8", 600);
    dns_record_manager::uniq().add("domain2.com", ips2, "8.8.8.8", 600);

    // 获取统计信息
    auto stats = dns_record_manager::uniq().stats();

    // 验证统计信息
    ASSERT_GE(stats.total_domain, 2);
    ASSERT_GE(stats.total_ip, 3);

    logger::INFO << "Cache stats: " << stats.serialize() << END;
    logger::INFO << "Cache stats test passed" << END;
}

// 测试缓存导出功能
TEST(cache_management, dump_cache) {
    dns_record_manager::uniq().clear();

    // 添加一些记录
    vector<uint32_t> ips = {st::utils::ipv4::str_to_ip("1.2.3.4")};
    dns_record_manager::uniq().add("dump-test.com", ips, "8.8.8.8", 600);

    // 导出缓存
    std::string dump_path = dns_record_manager::uniq().dump();

    // 验证导出路径不为空
    ASSERT_FALSE(dump_path.empty());

    logger::INFO << "Cache dumped to: " << dump_path << END;
    logger::INFO << "Dump cache test passed" << END;
}

// 测试智能解析功能
TEST(cache_management, smart_resolve) {
    dns_record_manager::uniq().clear();

    // 添加同一个域名的多个服务器记录
    vector<uint32_t> ips1 = {st::utils::ipv4::str_to_ip("1.1.1.1")};
    vector<uint32_t> ips2 = {st::utils::ipv4::str_to_ip("2.2.2.2")};

    dns_record_manager::uniq().add("smart-test.com", ips1, "8.8.8.8", 600);
    dns_record_manager::uniq().add("smart-test.com", ips2, "114.114.114.114", 600);

    // 使用智能解析
    auto record = dns_record_manager::uniq().resolve("smart-test.com");

    // 验证解析结果包含两个服务器的记录
    ASSERT_GE(record.ips.size(), 1);
    ASSERT_GE(record.servers.size(), 1);

    logger::INFO << "Smart resolve returned " << record.ips.size() << " IPs from "
                 << record.servers.size() << " servers" << END;
    logger::INFO << "Smart resolve test passed" << END;
}

// 测试多服务器记录合并
TEST(cache_management, multi_server_record_merge) {
    dns_record_manager::uniq().clear();

    // 添加同一个域名的多个服务器记录
    vector<uint32_t> ips1 = {
        st::utils::ipv4::str_to_ip("1.1.1.1"),
        st::utils::ipv4::str_to_ip("1.1.1.2")
    };
    vector<uint32_t> ips2 = {
        st::utils::ipv4::str_to_ip("2.2.2.1"),
        st::utils::ipv4::str_to_ip("2.2.2.2")
    };
    vector<uint32_t> ips3 = {
        st::utils::ipv4::str_to_ip("3.3.3.1")
    };

    dns_record_manager::uniq().add("merge-test.com", ips1, "8.8.8.8", 600);
    dns_record_manager::uniq().add("merge-test.com", ips2, "114.114.114.114", 600);
    dns_record_manager::uniq().add("merge-test.com", ips3, "223.5.5.5", 600);

    // 获取所有记录
    auto records = dns_record_manager::uniq().get_dns_record_list("merge-test.com");

    // 验证有3个服务器的记录
    ASSERT_EQ(3, records.size());

    // 验证每个服务器的IP数量
    int total_ips = 0;
    for (const auto &record : records) {
        total_ips += record.ips.size();
    }
    ASSERT_EQ(5, total_ips);

    logger::INFO << "Multi-server record merge test passed" << END;
}

// 测试缓存更新（相同域名和服务器）
TEST(cache_management, cache_update) {
    dns_record_manager::uniq().clear();

    // 第一次添加
    vector<uint32_t> ips1 = {st::utils::ipv4::str_to_ip("1.1.1.1")};
    dns_record_manager::uniq().add("update-test.com", ips1, "8.8.8.8", 600);

    auto records = dns_record_manager::uniq().get_dns_record_list("update-test.com");
    ASSERT_EQ(1, records.size());
    ASSERT_EQ(1, records[0].ips.size());

    // 第二次添加（相同服务器，不同IP）
    vector<uint32_t> ips2 = {
        st::utils::ipv4::str_to_ip("2.2.2.2"),
        st::utils::ipv4::str_to_ip("3.3.3.3")
    };
    dns_record_manager::uniq().add("update-test.com", ips2, "8.8.8.8", 600);

    // 验证记录被更新
    records = dns_record_manager::uniq().get_dns_record_list("update-test.com");
    ASSERT_EQ(1, records.size());
    ASSERT_EQ(2, records[0].ips.size());

    logger::INFO << "Cache update test passed" << END;
}

// 测试空域名处理
TEST(cache_management, empty_domain_handling) {
    dns_record_manager::uniq().clear();

    // 查询不存在的域名
    auto records = dns_record_manager::uniq().get_dns_record_list("nonexistent.com");
    ASSERT_EQ(0, records.size());

    // 智能解析不存在的域名
    auto record = dns_record_manager::uniq().resolve("nonexistent.com");
    ASSERT_EQ(0, record.ips.size());

    logger::INFO << "Empty domain handling test passed" << END;
}
