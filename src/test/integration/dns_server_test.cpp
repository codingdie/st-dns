//
// Created by codingdie on 2020/6/27.
//
#include "dns_server.h"
#include "dns_client.h"
#include "command/dns_command.h"
#include <gtest/gtest.h>
class BaseTest : public ::testing::Test {
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
class IntegrationTests : public BaseTest {
protected:
    void SetUp() override {
        BaseTest::SetUp();// Sets up the base fixture first.
    }
    void TearDown() override { BaseTest::TearDown(); }
};

void test_dns(const string &domain) {
    const string server = "127.0.0.1";
    const uint32_t port = 5353;
    mutex lock;
    lock.lock();
    vector<uint32_t> ips;
    dns_client::uniq().udp_dns(domain, server, port, 25000, [&](std::vector<uint32_t> result) {
        ips = result;
        lock.unlock();
    });
    lock.lock();
    if (ips.size() > 0) {
        logger::INFO << domain << "ips:" << st::utils::ipv4::ips_to_str(ips) << END;
    }
    ASSERT_TRUE(ips.size() > 0);
    lock.unlock();
}

void test_console() {
    string ip = st::dns::config::INSTANCE.console_ip;
    int console_port = st::dns::config::INSTANCE.console_port;
    auto begin = time::now();
    for (auto i = 0; i < 10000; i++) {
        auto result = console::client::command(ip, console_port, "dns resolve --domain=www.baidu.com", 1000);
        ASSERT_TRUE(result.first);
    }
    for (auto i = 0; i < 10000; i++) {
        const vector<string> &ips = command::dns::reverse_resolve(st::utils::ipv4::str_to_ip("110.242.68.3"));
        ASSERT_FALSE(ips.empty());
    }
    logger::INFO << "command avg time" << (time::now() - begin) * 1.0 / 20000 << END;

    auto result = console::client::command(ip, console_port, "dns record dump", 60000);
    ASSERT_STREQ("/tmp/st-dns-record.txt", result.second.c_str());
    result = console::client::command(ip, console_port, "dns record analyse", 60000);
    ASSERT_TRUE(result.first);
}


TEST_F(IntegrationTests, test_dns) {
    for (auto i = 0; i < 1000; i++) {
        test_dns("www.baidu.com");
    }
    test_console();
}

TEST_F(IntegrationTests, test_force_resolve) {
    const string server = "127.0.0.1";
    const uint32_t port = 5353;
    mutex lock;

    // 测试精确匹配的强制解析: test.codingdie.com -> 1.2.3.4
    {
        lock.lock();
        vector<uint32_t> ips;
        dns_client::uniq().udp_dns("test.codingdie.com", server, port, 5000, [&](std::vector<uint32_t> result) {
            ips = result;
            lock.unlock();
        });
        lock.lock();
        ASSERT_EQ(1, ips.size());
        ASSERT_EQ(st::utils::ipv4::str_to_ip("1.2.3.4"), ips[0]);
        logger::INFO << "Force resolve exact match: test.codingdie.com -> " << st::utils::ipv4::ips_to_str(ips) << END;
        lock.unlock();
    }

    // 测试通配符匹配的强制解析: www.codingdie.com -> 192.168.1.100, 192.168.1.101
    {
        lock.lock();
        vector<uint32_t> ips;
        dns_client::uniq().udp_dns("www.codingdie.com", server, port, 5000, [&](std::vector<uint32_t> result) {
            ips = result;
            lock.unlock();
        });
        lock.lock();
        ASSERT_EQ(2, ips.size());
        ASSERT_EQ(st::utils::ipv4::str_to_ip("192.168.1.100"), ips[0]);
        ASSERT_EQ(st::utils::ipv4::str_to_ip("192.168.1.101"), ips[1]);
        logger::INFO << "Force resolve wildcard match: www.codingdie.com -> " << st::utils::ipv4::ips_to_str(ips) << END;
        lock.unlock();
    }

    // 测试通配符匹配基础域名: codingdie.com -> 192.168.1.100, 192.168.1.101
    {
        lock.lock();
        vector<uint32_t> ips;
        dns_client::uniq().udp_dns("codingdie.com", server, port, 5000, [&](std::vector<uint32_t> result) {
            ips = result;
            lock.unlock();
        });
        lock.lock();
        ASSERT_EQ(2, ips.size());
        ASSERT_EQ(st::utils::ipv4::str_to_ip("192.168.1.100"), ips[0]);
        ASSERT_EQ(st::utils::ipv4::str_to_ip("192.168.1.101"), ips[1]);
        logger::INFO << "Force resolve wildcard base domain: codingdie.com -> " << st::utils::ipv4::ips_to_str(ips) << END;
        lock.unlock();
    }

    // 测试另一个精确匹配: github.com -> 192.30.255.113
    {
        lock.lock();
        vector<uint32_t> ips;
        dns_client::uniq().udp_dns("github.com", server, port, 5000, [&](std::vector<uint32_t> result) {
            ips = result;
            lock.unlock();
        });
        lock.lock();
        ASSERT_EQ(1, ips.size());
        ASSERT_EQ(st::utils::ipv4::str_to_ip("192.30.255.113"), ips[0]);
        logger::INFO << "Force resolve exact match: github.com -> " << st::utils::ipv4::ips_to_str(ips) << END;
        lock.unlock();
    }
}
