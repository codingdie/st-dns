//
// Created by codingdie on 2020/6/27.
//
#include "integration_test_base.h"
#include "command/dns_command.h"
#include <boost/asio.hpp>
#include <chrono>
#include <future>

class integration_tests : public BaseTest {
protected:
    void SetUp() override {
        BaseTest::SetUp();
    }
    void TearDown() override { BaseTest::TearDown(); }
};

void test_dns(const string &domain) {
    const string server = "127.0.0.1";
    const uint32_t port = 5353;
    std::promise<vector<uint32_t>> promise;
    auto future = promise.get_future();
    dns_client::uniq().udp_dns(domain, server, port, 25000, [&](std::vector<uint32_t> result) {
        promise.set_value(result);
    });
    auto ips = future.get();
    if (ips.size() > 0) {
        logger::INFO << domain << "ips:" << st::utils::ipv4::ips_to_str(ips) << END;
    }
    ASSERT_TRUE(ips.size() > 0);
}

void test_console() {
    string ip = st::dns::config::INSTANCE.console_ip;
    int console_port = st::dns::config::INSTANCE.console_port;

    // 先通过 console 命令解析一次 www.baidu.com，确保缓存中有记录
    auto init_result = console::client::command(ip, console_port, "dns resolve --domain=www.baidu.com", 5000);
    ASSERT_TRUE(init_result.first) << "Initial DNS resolve failed";

    // 从缓存中读取解析结果，获取实际的 IP
    dns_record record = dns_record_manager::uniq().resolve("www.baidu.com");
    ASSERT_FALSE(record.ips.empty()) << "www.baidu.com should have been cached after console resolve";
    uint32_t test_ip = record.ips[0]; // 使用第一个解析出的 IP
    logger::INFO << "Using IP for reverse resolve test: " << st::utils::ipv4::ip_to_str(test_ip) << END;

    auto begin = time::now();
    for (auto i = 0; i < 10000; i++) {
        auto result = console::client::command(ip, console_port, "dns resolve --domain=www.baidu.com", 5000);
        ASSERT_TRUE(result.first);
    }
    // 使用实际解析出的 IP 进行反向解析测试
    for (auto i = 0; i < 10000; i++) {
        const vector<string> &ips = command::dns::reverse_resolve(test_ip);
        ASSERT_FALSE(ips.empty()) << "Reverse resolve should find domain for IP: " << st::utils::ipv4::ip_to_str(test_ip);
    }
    logger::INFO << "command avg time" << (time::now() - begin) * 1.0 / 20000 << END;

    auto result = console::client::command(ip, console_port, "dns record dump", 60000);
    ASSERT_STREQ("/tmp/st-dns-record.txt", result.second.c_str());
    result = console::client::command(ip, console_port, "dns record analyse", 60000);
    ASSERT_TRUE(result.first);
}


TEST_F(integration_tests, test_dns) {
    for (auto i = 0; i < 1000; i++) {
        test_dns("www.baidu.com");
    }
    test_console();
}

TEST_F(integration_tests, test_force_resolve) {
    const string server = "127.0.0.1";
    const uint32_t port = 5353;

    // 测试精确匹配的强制解析: test.codingdie.com -> 1.2.3.4
    {
        std::promise<vector<uint32_t>> promise;
        auto future = promise.get_future();
        dns_client::uniq().udp_dns("test.codingdie.com", server, port, 5000, [&](std::vector<uint32_t> result) {
            promise.set_value(result);
        });
        auto ips = future.get();
        ASSERT_EQ(1, ips.size());
        ASSERT_EQ(st::utils::ipv4::str_to_ip("1.2.3.4"), ips[0]);
        logger::INFO << "Force resolve exact match: test.codingdie.com -> " << st::utils::ipv4::ips_to_str(ips) << END;
    }

    // 测试通配符匹配的强制解析: www.codingdie.com -> 192.168.1.100, 192.168.1.101
    {
        std::promise<vector<uint32_t>> promise;
        auto future = promise.get_future();
        dns_client::uniq().udp_dns("www.codingdie.com", server, port, 5000, [&](std::vector<uint32_t> result) {
            promise.set_value(result);
        });
        auto ips = future.get();
        ASSERT_EQ(2, ips.size());
        ASSERT_EQ(st::utils::ipv4::str_to_ip("192.168.1.100"), ips[0]);
        ASSERT_EQ(st::utils::ipv4::str_to_ip("192.168.1.101"), ips[1]);
        logger::INFO << "Force resolve wildcard match: www.codingdie.com -> " << st::utils::ipv4::ips_to_str(ips) << END;
    }

    // 测试通配符匹配基础域名: codingdie.com -> 192.168.1.100, 192.168.1.101
    {
        std::promise<vector<uint32_t>> promise;
        auto future = promise.get_future();
        dns_client::uniq().udp_dns("codingdie.com", server, port, 5000, [&](std::vector<uint32_t> result) {
            promise.set_value(result);
        });
        auto ips = future.get();
        ASSERT_EQ(2, ips.size());
        ASSERT_EQ(st::utils::ipv4::str_to_ip("192.168.1.100"), ips[0]);
        ASSERT_EQ(st::utils::ipv4::str_to_ip("192.168.1.101"), ips[1]);
        logger::INFO << "Force resolve wildcard base domain: codingdie.com -> " << st::utils::ipv4::ips_to_str(ips) << END;
    }

    // 测试另一个精确匹配: github.com -> 192.30.255.113
    {
        std::promise<vector<uint32_t>> promise;
        auto future = promise.get_future();
        dns_client::uniq().udp_dns("github.com", server, port, 5000, [&](std::vector<uint32_t> result) {
            promise.set_value(result);
        });
        auto ips = future.get();
        ASSERT_EQ(1, ips.size());
        ASSERT_EQ(st::utils::ipv4::str_to_ip("192.30.255.113"), ips[0]);
        logger::INFO << "Force resolve exact match: github.com -> " << st::utils::ipv4::ips_to_str(ips) << END;
    }
}

static std::unique_ptr<st::dns::protocol::udp_request> build_https_query(const string &domain) {
    std::unique_ptr<st::dns::protocol::udp_request> request(new st::dns::protocol::udp_request({domain}));
    auto *query = request->query_zone->querys[0];
    query->data[query->domain->len] = 0x00;
    query->data[query->domain->len + 1] = 0x41;
    return request;
}

TEST(integration_timeout_tests, non_a_query_uses_upstream_timeout_when_forward_capacity_available) {
    st::dns::config::INSTANCE.load("../confs/test");
    dns_record_manager::uniq().clear();
    st::dns::config::INSTANCE.servers[0]->ip = "127.0.0.1";
    st::dns::config::INSTANCE.servers[0]->port = 1;
    st::dns::config::INSTANCE.servers[0]->timeout = 500;

    auto *server = new dns_server(st::dns::config::INSTANCE, 1);
    auto *th = new thread([=]() { server->start(); });
    server->wait_start();

    boost::asio::io_context io_context;
    boost::asio::ip::udp::socket socket(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));
    boost::asio::ip::udp::endpoint server_endpoint(boost::asio::ip::make_address_v4("127.0.0.1"), 5353);

    auto request = build_https_query("capacity.example.com");
    auto begin = time::now();
    socket.send_to(boost::asio::buffer(request->data, request->len), server_endpoint);

    uint8_t buffer[1024] = {0};
    boost::asio::ip::udp::endpoint response_endpoint;
    size_t response_size = socket.receive_from(boost::asio::buffer(buffer, sizeof(buffer)), response_endpoint);
    auto cost = time::now() - begin;

    server->shutdown();
    th->join();
    delete th;
    delete server;
    st::dns::config::INSTANCE.unload();

    ASSERT_GT(response_size, 0);
    ASSERT_GE(cost, 400);
    ASSERT_LE(cost, 800);
}

TEST(integration_timeout_tests, non_a_query_rejected_immediately_when_forward_concurrency_full) {
    st::dns::config::INSTANCE.load("../confs/test");
    dns_record_manager::uniq().clear();
    st::dns::config::INSTANCE.servers[0]->ip = "127.0.0.1";
    st::dns::config::INSTANCE.servers[0]->port = 1;
    st::dns::config::INSTANCE.servers[0]->timeout = 500;

    auto *server = new dns_server(st::dns::config::INSTANCE, 1);
    auto *th = new thread([=]() { server->start(); });
    server->wait_start();

    boost::asio::io_context io_context;
    boost::asio::ip::udp::endpoint server_endpoint(boost::asio::ip::make_address_v4("127.0.0.1"), 5353);
    boost::asio::ip::udp::socket slow_socket(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));
    boost::asio::ip::udp::socket rejected_socket(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));

    auto slow_request = build_https_query("slow.example.com");
    slow_socket.send_to(boost::asio::buffer(slow_request->data, slow_request->len), server_endpoint);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto rejected_request = build_https_query("rejected.example.com");
    auto begin = time::now();
    rejected_socket.send_to(boost::asio::buffer(rejected_request->data, rejected_request->len), server_endpoint);

    uint8_t rejected_buffer[1024] = {0};
    boost::asio::ip::udp::endpoint rejected_endpoint;
    size_t rejected_size = rejected_socket.receive_from(boost::asio::buffer(rejected_buffer, sizeof(rejected_buffer)), rejected_endpoint);
    auto rejected_cost = time::now() - begin;

    uint8_t slow_buffer[1024] = {0};
    boost::asio::ip::udp::endpoint slow_endpoint;
    size_t slow_size = slow_socket.receive_from(boost::asio::buffer(slow_buffer, sizeof(slow_buffer)), slow_endpoint);

    server->shutdown();
    th->join();
    delete th;
    delete server;
    st::dns::config::INSTANCE.unload();

    ASSERT_GT(rejected_size, 0);
    ASSERT_GT(slow_size, 0);
    ASSERT_LE(rejected_cost, 150);
}
