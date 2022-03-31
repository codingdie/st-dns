//
// Created by codingdie on 2020/6/27.
//
#include "DNSServer.h"
#include "DNSClient.h"
#include <gtest/gtest.h>
class BaseTest : public ::testing::Test {
protected:
    DNSServer *dnsServer;
    thread *th;
    void SetUp() override {
        st::dns::Config::INSTANCE.load("../confs/test");
        file::del(st::dns::Config::INSTANCE.dnsCacheFile);
        dnsServer = new DNSServer(st::dns::Config::INSTANCE);
        th = new thread([=]() { dnsServer->start(); });
        dnsServer->waitStart();
    }
    void TearDown() override {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        dnsServer->shutdown();
        th->join();
        APMLogger::disable();
        delete th;
        delete dnsServer;
        std::this_thread::sleep_for(std::chrono::seconds(1));
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
    DNSClient::INSTANCE.udpDns(domain, server, port, 25000, [&](std::vector<uint32_t> result) {
        ips = result;
        lock.unlock();
    });
    lock.lock();
    if (ips.size() > 0) {
        Logger::INFO << domain << "ips:" << st::utils::ipv4::ipsToStr(ips) << END;
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
    Logger::INFO << result << END;
}
