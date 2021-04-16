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
        dnsServer = new DNSServer(st::dns::Config::INSTANCE);
        th = new thread([=]() { dnsServer->start(); });
        dnsServer->waitStart();
    }
    void TearDown() override {
        dnsServer->shutdown();
        th->join();
        delete th;
        delete dnsServer;
    }
};
class IntegrationTests : public BaseTest {
protected:
    void SetUp() override {
        BaseTest::SetUp();// Sets up the base fixture first.
    }
    void TearDown() override { BaseTest::TearDown(); }
};


template<typename FUNC>
void testParallel(FUNC testMethod, uint32_t count, uint32_t parral) {
    boost::asio::thread_pool threadPool(parral);
    atomic<uint32_t> taskCount(0);
    for (int i = 0; i < count; i++) {
        auto task = [&]() {
            Logger::traceId = 1000;
            testMethod();
            taskCount++;
        };
        boost::asio::post(threadPool, task);
    }
    while (taskCount != count) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}
void testUdp(const string &domain, const string &server, const uint32_t port, const int count, const int parral) {
    testParallel(
            [=]() {
                auto dnsResponse = DNSClient::udpDns(domain, server, port, 5000);
                EXPECT_NE(dnsResponse, nullptr);
                EXPECT_STRNE("", st::utils::ipv4::ipsToStr(dnsResponse->ips).c_str());
                if (dnsResponse != nullptr) {
                    cout << st::utils::ipv4::ipsToStr(dnsResponse->ips) << endl;
                    delete dnsResponse;
                }
            },
            count, parral);
}

TEST_F(IntegrationTests, testCURL) {
    testUdp("baidu.com", "127.0.0.1", 5353, 10000, 16);
    cout << "run" << endl;
    std::this_thread::sleep_for(std::chrono::seconds(20));
    st::mem::pgc();
    cout << "pgc" << endl;
    std::this_thread::sleep_for(std::chrono::seconds(20));
}
