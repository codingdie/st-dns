//
// Created by codingdie on 2026/02/15.
// 服务器选择逻辑单元测试
//

#include <gtest/gtest.h>
#include "config.h"
#include "st.h"

// 测试白名单匹配 - 精确匹配
TEST(server_selection, whitelist_exact_match) {
    vector<remote_dns_server *> servers;

    // 创建两个服务器
    auto server1 = new remote_dns_server("8.8.8.8", 53, "UDP");
    server1->whitelist.insert("test.example.com");

    auto server2 = new remote_dns_server("114.114.114.114", 53, "UDP");

    servers.push_back(server1);
    servers.push_back(server2);

    // 测试白名单精确匹配
    auto result = remote_dns_server::select_servers("test.example.com", servers);

    ASSERT_EQ(1, result.size());
    ASSERT_EQ("8.8.8.8", result[0]->ip);

    // 清理
    delete server1;
    delete server2;
}

// 测试白名单匹配 - 正则表达式匹配
TEST(server_selection, whitelist_regex_match) {
    vector<remote_dns_server *> servers;

    auto server1 = new remote_dns_server("192.168.1.1", 53, "UDP");
    server1->whitelist.insert(".*\\.example\\.com");

    auto server2 = new remote_dns_server("8.8.8.8", 53, "UDP");

    servers.push_back(server1);
    servers.push_back(server2);

    // 测试正则表达式匹配
    auto result = remote_dns_server::select_servers("test.example.com", servers);

    ASSERT_EQ(1, result.size());
    ASSERT_EQ("192.168.1.1", result[0]->ip);

    result = remote_dns_server::select_servers("api.example.com", servers);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ("192.168.1.1", result[0]->ip);

    // 清理
    delete server1;
    delete server2;
}

// 测试黑名单过滤
TEST(server_selection, blacklist_filter) {
    vector<remote_dns_server *> servers;

    auto server1 = new remote_dns_server("223.5.5.5", 53, "UDP");
    server1->blacklist.insert("google.com");
    server1->blacklist.insert("youtube.com");

    auto server2 = new remote_dns_server("8.8.8.8", 53, "UDP");

    servers.push_back(server1);
    servers.push_back(server2);

    // 测试黑名单域名被过滤
    auto result = remote_dns_server::select_servers("google.com", servers);

    ASSERT_EQ(1, result.size());
    ASSERT_EQ("8.8.8.8", result[0]->ip);

    // 测试非黑名单域名返回所有服务器
    result = remote_dns_server::select_servers("baidu.com", servers);
    ASSERT_EQ(2, result.size());

    // 清理
    delete server1;
    delete server2;
}

// 测试 LAN 域名特殊处理
TEST(server_selection, lan_domain_handling) {
    vector<remote_dns_server *> servers;

    auto server1 = new remote_dns_server("192.168.1.1", 53, "UDP");
    server1->areas.push_back("LAN");

    auto server2 = new remote_dns_server("8.8.8.8", 53, "UDP");
    server2->areas.push_back("CN");

    servers.push_back(server1);
    servers.push_back(server2);

    // 测试 .local 域名只返回 LAN 服务器
    auto result = remote_dns_server::select_servers("test.local", servers);

    ASSERT_EQ(1, result.size());
    ASSERT_EQ("192.168.1.1", result[0]->ip);

    // 清理
    delete server1;
    delete server2;
}

// 测试服务器顺序
TEST(server_selection, server_order) {
    vector<remote_dns_server *> servers;

    auto server1 = new remote_dns_server("1.1.1.1", 53, "UDP");
    auto server2 = new remote_dns_server("2.2.2.2", 53, "UDP");
    auto server3 = new remote_dns_server("3.3.3.3", 53, "UDP");

    servers.push_back(server1);
    servers.push_back(server2);
    servers.push_back(server3);

    // 测试返回的服务器顺序与配置顺序一致
    auto result = remote_dns_server::select_servers("test.com", servers);

    ASSERT_EQ(3, result.size());
    ASSERT_EQ("1.1.1.1", result[0]->ip);
    ASSERT_EQ("2.2.2.2", result[1]->ip);
    ASSERT_EQ("3.3.3.3", result[2]->ip);

    // 清理
    delete server1;
    delete server2;
    delete server3;
}

// 测试白名单优先级最高
TEST(server_selection, whitelist_priority) {
    vector<remote_dns_server *> servers;

    auto server1 = new remote_dns_server("1.1.1.1", 53, "UDP");

    auto server2 = new remote_dns_server("2.2.2.2", 53, "UDP");
    server2->whitelist.insert("priority.com");

    auto server3 = new remote_dns_server("3.3.3.3", 53, "UDP");

    servers.push_back(server1);
    servers.push_back(server2);
    servers.push_back(server3);

    // 测试白名单匹配时只返回该服务器，忽略其他服务器
    auto result = remote_dns_server::select_servers("priority.com", servers);

    ASSERT_EQ(1, result.size());
    ASSERT_EQ("2.2.2.2", result[0]->ip);

    // 清理
    delete server1;
    delete server2;
    delete server3;
}

// 测试空服务器列表
TEST(server_selection, empty_server_list) {
    vector<remote_dns_server *> servers;

    auto result = remote_dns_server::select_servers("test.com", servers);

    ASSERT_EQ(0, result.size());
}

// 测试所有服务器都被黑名单过滤
TEST(server_selection, all_servers_blacklisted) {
    vector<remote_dns_server *> servers;

    auto server1 = new remote_dns_server("1.1.1.1", 53, "UDP");
    server1->blacklist.insert("blocked.com");

    auto server2 = new remote_dns_server("2.2.2.2", 53, "UDP");
    server2->blacklist.insert("blocked.com");

    servers.push_back(server1);
    servers.push_back(server2);

    auto result = remote_dns_server::select_servers("blocked.com", servers);

    ASSERT_EQ(0, result.size());

    // 清理
    delete server1;
    delete server2;
}

// 测试白名单和黑名单组合
TEST(server_selection, whitelist_and_blacklist_combination) {
    vector<remote_dns_server *> servers;

    auto server1 = new remote_dns_server("1.1.1.1", 53, "UDP");
    server1->whitelist.insert("allowed.com");
    server1->blacklist.insert("blocked.com");

    auto server2 = new remote_dns_server("2.2.2.2", 53, "UDP");

    servers.push_back(server1);
    servers.push_back(server2);

    // 测试白名单域名
    auto result = remote_dns_server::select_servers("allowed.com", servers);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ("1.1.1.1", result[0]->ip);

    // 测试黑名单域名
    result = remote_dns_server::select_servers("blocked.com", servers);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ("2.2.2.2", result[0]->ip);

    // 清理
    delete server1;
    delete server2;
}
