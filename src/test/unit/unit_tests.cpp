//
// Created by codingdie on 2020/6/27.
//
#include <gtest/gtest.h>
#include "dns_client.h"
#include "dns_server.h"
#include "st.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>


TEST(UnitTests, test_ip_sort) {
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    vector<dns_ip_record> ip_records;
    auto create = [](uint32_t ip, bool match, bool forbid) {
        dns_ip_record b;
        b.ip = ip;
        b.match_area = match;
        b.forbid = forbid;
        return b;
    };
    ip_records.emplace_back(create(3, true, false));
    ip_records.emplace_back(create(1, false, false));
    ip_records.emplace_back(create(2, true, true));
    ip_records.emplace_back(create(4, true, false));
    std::shuffle(ip_records.begin(), ip_records.end(), std::default_random_engine(seed));
    std::sort(ip_records.begin(), ip_records.end(), dns_ip_record::compare);
    for (auto item : ip_records) {
        logger::INFO << item.ip;
    }
    logger::INFO << END;
    ASSERT_EQ(1, ip_records[3].ip);
    ASSERT_EQ(2, ip_records[2].ip);
}


TEST(UnitTests, test_dns_cache) {
    dns_record_manager::uniq().add("test01.com", {1, 2, 3}, "192.168.31.2", 60);
    dns_record_manager::uniq().add("test01.com", {1, 2, 3}, "192.168.31.1", 60);
    dns_record_manager::uniq().add("test01.com", {1, 2}, "192.168.31.1", 60);
    const proto::records &records = dns_record_manager::uniq().get_dns_records_pb("test01.com");
    auto ma = records.map();
    for (const auto &item : ma) {
        cout << item.first << endl;
    }
    ASSERT_EQ(2, records.map_size());
    ASSERT_EQ(2, records.map().at("192.168.31.1").ips_size());
    ASSERT_EQ(3, records.map().at("192.168.31.2").ips_size());
}

TEST(UnitTests, test_dns_cache_max_4_ips) {
    // 测试超过4个IP时，只缓存4个
    vector<uint32_t> many_ips = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    dns_record_manager::uniq().add("test-max-ips.com", many_ips, "8.8.8.8", 60);
    const proto::records &records = dns_record_manager::uniq().get_dns_records_pb("test-max-ips.com");
    ASSERT_EQ(1, records.map_size());
    ASSERT_LE(records.map().at("8.8.8.8").ips_size(), 4);
    logger::INFO << "Cached IPs count: " << records.map().at("8.8.8.8").ips_size() << END;
}

TEST(UnitTests, test_force_resolve_exact_match) {
    // 测试精确匹配
    force_resolve_rule rule("test.codingdie.com", {st::utils::ipv4::str_to_ip("1.2.3.4")});

    ASSERT_TRUE(rule.match("test.codingdie.com"));
    ASSERT_FALSE(rule.match("www.test.codingdie.com"));
    ASSERT_FALSE(rule.match("codingdie.com"));
    ASSERT_FALSE(rule.match("other.com"));

    logger::INFO << "Force resolve exact match test passed" << END;
}

TEST(UnitTests, test_force_resolve_wildcard_match) {
    // 测试通配符匹配
    force_resolve_rule rule("*.codingdie.com", {
        st::utils::ipv4::str_to_ip("192.168.1.100"),
        st::utils::ipv4::str_to_ip("192.168.1.101")
    });

    // 应该匹配所有子域名
    ASSERT_TRUE(rule.match("www.codingdie.com"));
    ASSERT_TRUE(rule.match("api.codingdie.com"));
    ASSERT_TRUE(rule.match("test.codingdie.com"));

    // 应该匹配基础域名
    ASSERT_TRUE(rule.match("codingdie.com"));

    // 不应该匹配其他域名
    ASSERT_FALSE(rule.match("codingdie.cn"));
    ASSERT_FALSE(rule.match("other.com"));
    ASSERT_FALSE(rule.match("test.other.com"));

    logger::INFO << "Force resolve wildcard match test passed" << END;
}

TEST(UnitTests, test_force_resolve_multi_level_wildcard) {
    // 测试多级子域名通配符匹配
    force_resolve_rule rule("*.example.com", {st::utils::ipv4::str_to_ip("10.0.0.1")});

    ASSERT_TRUE(rule.match("a.example.com"));
    ASSERT_TRUE(rule.match("a.b.example.com"));
    ASSERT_TRUE(rule.match("a.b.c.example.com"));
    ASSERT_TRUE(rule.match("example.com"));

    logger::INFO << "Force resolve multi-level wildcard test passed" << END;
}

TEST(UnitTests, test_force_resolve_ips) {
    // 测试多个IP地址
    vector<uint32_t> ips = {
        st::utils::ipv4::str_to_ip("192.168.1.1"),
        st::utils::ipv4::str_to_ip("192.168.1.2"),
        st::utils::ipv4::str_to_ip("192.168.1.3")
    };

    force_resolve_rule rule("test.com", ips);

    ASSERT_EQ(3, rule.ips.size());
    ASSERT_EQ(ips[0], rule.ips[0]);
    ASSERT_EQ(ips[1], rule.ips[1]);
    ASSERT_EQ(ips[2], rule.ips[2]);

    logger::INFO << "Force resolve multiple IPs test passed" << END;
}
