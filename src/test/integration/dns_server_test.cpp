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


TEST_F(IntegrationTests, test_dns) {
    test_dns("www.baidu.com");

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
