//
// Created by codingdie on 2020/6/27.
//
#include "dns_server.h"
#include "dns_client.h"
#include <gtest/gtest.h>
class BaseTest : public ::testing::Test {
protected:
    dns_server *dnsServer;
    thread *th;
    void SetUp() override {
        st::dns::config::INSTANCE.load("../confs/test");
        file::del(st::dns::config::INSTANCE.dns_cache_file);
        dnsServer = new dns_server(st::dns::config::INSTANCE);
        th = new thread([=]() { dnsServer->start(); });
        dnsServer->wait_start();
    }
    void TearDown() override {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        dnsServer->shutdown();
        th->join();
        apm_logger::disable();
        delete th;
        delete dnsServer;
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
};
class IntegrationTests : public BaseTest {
protected:
    void SetUp() override {
        BaseTest::SetUp();// Sets up the base fixture first.
    }
    void TearDown() override { BaseTest::TearDown(); }
};

void testDNS(const string &domain) {
    const string server = "127.0.0.1";
    const uint32_t port = 5353;
    mutex lock;
    lock.lock();
    vector<uint32_t> ips;
    dns_client::INSTANCE.udp_dns(domain, server, port, 25000, [&](std::vector<uint32_t> result) {
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

TEST_F(IntegrationTests, testDNS) {
    testDNS("google.com");
    testDNS("youtube.com");
    testDNS("facebook.com");
    testDNS("twitter.com");
    testDNS("baidu.com");
}

TEST_F(IntegrationTests, testForwardUDP) {
    string result;
    for (int i = 0; i < 10; i++) {
        shell::exec("dig cname baidu.com @127.0.0.1 -p5353",result);
    }
    logger::INFO << result << END;
}
