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
#include <fstream>
#include <boost/filesystem.hpp>


TEST(unit_tests, test_ip_sort) {
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


TEST(unit_tests, test_dns_cache) {
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

TEST(unit_tests, test_dns_cache_max_4_ips) {
    // 测试超过4个IP时，只缓存4个
    vector<uint32_t> many_ips = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    dns_record_manager::uniq().add("test-max-ips.com", many_ips, "8.8.8.8", 60);
    const proto::records &records = dns_record_manager::uniq().get_dns_records_pb("test-max-ips.com");
    ASSERT_EQ(1, records.map_size());
    ASSERT_LE(records.map().at("8.8.8.8").ips_size(), 4);
    logger::INFO << "Cached IPs count: " << records.map().at("8.8.8.8").ips_size() << END;
}

TEST(unit_tests, test_force_resolve_exact_match) {
    // 测试精确匹配
    force_resolve_rule rule("test.codingdie.com", {st::utils::ipv4::str_to_ip("1.2.3.4")});

    ASSERT_TRUE(rule.match("test.codingdie.com"));
    ASSERT_FALSE(rule.match("www.test.codingdie.com"));
    ASSERT_FALSE(rule.match("codingdie.com"));
    ASSERT_FALSE(rule.match("other.com"));

    logger::INFO << "Force resolve exact match test passed" << END;
}

TEST(unit_tests, test_force_resolve_wildcard_match) {
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

TEST(unit_tests, test_force_resolve_multi_level_wildcard) {
    // 测试多级子域名通配符匹配
    force_resolve_rule rule("*.example.com", {st::utils::ipv4::str_to_ip("10.0.0.1")});

    ASSERT_TRUE(rule.match("a.example.com"));
    ASSERT_TRUE(rule.match("a.b.example.com"));
    ASSERT_TRUE(rule.match("a.b.c.example.com"));
    ASSERT_TRUE(rule.match("example.com"));

    logger::INFO << "Force resolve multi-level wildcard test passed" << END;
}

TEST(unit_tests, test_force_resolve_regex_match) {
    force_resolve_rule rule("", {st::utils::ipv4::str_to_ip("203.0.113.10")},
                            "^(api|www)\\.regex\\.codingdie\\.com$");

    ASSERT_TRUE(rule.match("api.regex.codingdie.com"));
    ASSERT_TRUE(rule.match("www.regex.codingdie.com"));
    ASSERT_FALSE(rule.match("cdn.regex.codingdie.com"));
    ASSERT_FALSE(rule.match("api.regex.codingdie.cn"));

    logger::INFO << "Force resolve regex match test passed" << END;
}

TEST(unit_tests, config_re_prefixed_force_resolve_rule_match) {
    st::dns::config::INSTANCE.unload();

    auto temp_dir = boost::filesystem::temp_directory_path() /
                    boost::filesystem::unique_path("st-dns-re-prefix-%%%%-%%%%-%%%%");
    boost::filesystem::create_directories(temp_dir);
    auto config_path = temp_dir / "config.json";

    {
        std::ofstream config_file(config_path.string());
        config_file << R"({
  "ip": "127.0.0.1",
  "port": 5353,
  "auto_upstream_dns": false,
  "servers": [
    {
      "type": "UDP",
      "ip": "127.0.0.1",
      "port": 53,
      "areas": ["LAN"]
    }
  ],
  "force_resolve_rules": [
    {
      "pattern": "re:^(api|www)\\.re-prefix\\.example\\.net$",
      "ips": ["203.0.113.11"]
    }
  ]
})";
    }

    st::dns::config::INSTANCE.load(temp_dir.string());
    ASSERT_EQ(1, st::dns::config::INSTANCE.force_resolve_rules.size());
    auto *rule = st::dns::config::INSTANCE.force_resolve_rules.front();
    ASSERT_TRUE(rule->match("api.re-prefix.example.net"));
    ASSERT_TRUE(rule->match("www.re-prefix.example.net"));
    ASSERT_FALSE(rule->match("cdn.re-prefix.example.net"));

    st::dns::config::INSTANCE.unload();
    boost::filesystem::remove_all(temp_dir);
}

TEST(unit_tests, test_force_resolve_ips) {
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

TEST(unit_tests, config_load_unload_is_repeatable) {
    st::dns::config::INSTANCE.unload();

    for (int i = 0; i < 2; i++) {
        st::dns::config::INSTANCE.load("../confs/test");

        ASSERT_TRUE(st::dns::config::INSTANCE.loaded);
        ASSERT_FALSE(st::dns::config::INSTANCE.servers.empty());
        ASSERT_FALSE(st::dns::config::INSTANCE.force_resolve_rules.empty());
        ASSERT_TRUE(st::areaip::manager::uniq().started());
        ASSERT_EQ(32, st::dns::config::INSTANCE.forward_max_running);

        st::dns::config::INSTANCE.unload();

        ASSERT_FALSE(st::dns::config::INSTANCE.loaded);
        ASSERT_TRUE(st::dns::config::INSTANCE.servers.empty());
        ASSERT_TRUE(st::dns::config::INSTANCE.force_resolve_rules.empty());
        ASSERT_FALSE(st::areaip::manager::uniq().started());
        ASSERT_EQ("127.0.0.1", st::dns::config::INSTANCE.ip);
        ASSERT_EQ(53, st::dns::config::INSTANCE.port);
        ASSERT_EQ("127.0.0.1", st::dns::config::INSTANCE.console_ip);
        ASSERT_EQ(5757, st::dns::config::INSTANCE.console_port);
        ASSERT_EQ(60 * 10, st::dns::config::INSTANCE.dns_cache_expire);
        ASSERT_EQ(32, st::dns::config::INSTANCE.forward_max_running);
        ASSERT_EQ("/usr/local/etc/st/dns", st::dns::config::INSTANCE.base_conf_dir);
    }
}

TEST(unit_tests, config_invalid_force_resolve_rules_are_skipped) {
    st::dns::config::INSTANCE.unload();

    auto temp_dir = boost::filesystem::temp_directory_path() /
                    boost::filesystem::unique_path("st-dns-invalid-regex-%%%%-%%%%-%%%%");
    boost::filesystem::create_directories(temp_dir);
    auto config_path = temp_dir / "config.json";

    {
        std::ofstream config_file(config_path.string());
        config_file << R"({
  "ip": "127.0.0.1",
  "port": 5353,
  "auto_upstream_dns": false,
  "servers": [
    {
      "type": "UDP",
      "ip": "127.0.0.1",
      "port": 53,
      "areas": ["LAN"]
    }
  ],
  "force_resolve_rules": [
    {
      "pattern": "re:",
      "ips": ["203.0.113.8"]
    },
    {
      "pattern": "re:api",
      "regex": "api",
      "ips": ["203.0.113.9"]
    },
    {
      "regex": "[",
      "ips": ["203.0.113.10"]
    }
  ]
})";
    }

    st::dns::config::INSTANCE.load(temp_dir.string());
    ASSERT_TRUE(st::dns::config::INSTANCE.loaded);
    ASSERT_TRUE(st::dns::config::INSTANCE.force_resolve_rules.empty());

    st::dns::config::INSTANCE.unload();
    boost::filesystem::remove_all(temp_dir);
}

TEST(unit_tests, config_auto_lan_udp_server_uses_system_upstream) {
    st::dns::config::INSTANCE.unload();

    auto temp_dir = boost::filesystem::temp_directory_path() /
                    boost::filesystem::unique_path("st-dns-auto-upstream-%%%%-%%%%-%%%%");
    boost::filesystem::create_directories(temp_dir);
    auto resolv_path = temp_dir / "resolv.conf";
    auto config_path = temp_dir / "config.json";

    {
        std::ofstream resolv_file(resolv_path.string());
        resolv_file << "nameserver 10.9.8.7\n";
    }

    {
        std::ofstream config_file(config_path.string());
        config_file << R"({
  "ip": "127.0.0.1",
  "port": 5353,
  "auto_upstream_dns": true,
  "resolv_conf_paths": [")" << resolv_path.string() << R"("],
  "servers": [
    {
      "type": "UDP",
      "ip": "AUTO_LAN_IP",
      "port": 53,
      "areas": ["LAN"],
      "timeout": "500",
      "dns_cache_expire": 60
    }
  ],
  "dns_cache_expire": 600,
  "forward_max_running": 32,
  "log": {
    "level": 1,
    "tag": "st-dns-test"
  }
})";
    }

    st::dns::config::INSTANCE.load(temp_dir.string());

    ASSERT_EQ(1, st::dns::config::INSTANCE.servers.size());
    ASSERT_EQ("10.9.8.7", st::dns::config::INSTANCE.servers[0]->ip);
    ASSERT_EQ("UDP", st::dns::config::INSTANCE.servers[0]->type);
    ASSERT_EQ(1, st::dns::config::INSTANCE.system_upstream_servers.size());
    ASSERT_EQ("10.9.8.7", st::dns::config::INSTANCE.system_upstream_servers[0]->ip);

    st::dns::config::INSTANCE.unload();
    boost::filesystem::remove_all(temp_dir);
}
