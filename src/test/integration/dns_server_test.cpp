//
// Created by codingdie on 2020/6/27.
//
#include "dns_server.h"
#include "dns_client.h"
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
    for (int i = 0; i < 3; i++) {
        test_dns("twitter.com");
        test_dns("google.com");
        test_dns("youtube.com");
        test_dns("facebook.com");
        test_dns("twitter.com");
        test_dns("baidu.com");
        string result;
        shell::exec("dig cname baidu.com @127.0.0.1 -p5353", result);
    }

    string ip = st::dns::config::INSTANCE.console_ip;
    int console_port = st::dns::config::INSTANCE.console_port;
    auto result = console::client::command(ip, console_port, "dns record get --domain=baidu.com", 1000);
    ASSERT_TRUE(!result.empty());
    result = console::client::command(ip, console_port, "dns record dump", 60000);
    ASSERT_STREQ("success\n/tmp/st-dns-record.txt\n", result.c_str());
    result = console::client::command(ip, console_port, "dns record analyse", 60000);
    ASSERT_TRUE(result.find("success") != string::npos);
}
