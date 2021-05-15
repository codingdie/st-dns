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
        file::del("../confs/test/cache.txt");
        dnsServer = new DNSServer(st::dns::Config::INSTANCE);
        th = new thread([=]() { dnsServer->start(); });
        dnsServer->waitStart();
    }
    void TearDown() override {
        std::this_thread::sleep_for(std::chrono::seconds(5));
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


template<typename FUNC>
void testParallel(vector<FUNC> testMethods, uint32_t parral) {
    boost::asio::thread_pool threadPool(parral);
    atomic<uint32_t> taskCount(0);
    for (auto testMethod : testMethods) {
        auto task = [=, &taskCount]() {
            Logger::traceId = 1000;
            testMethod();
            taskCount++;
        };
        boost::asio::post(threadPool, task);
    }
    while (taskCount != (int32_t) testMethods.size()) {
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
void testUdp(const string &domain, const string &server, const uint32_t port) {
    auto dnsResponse = DNSClient::udpDns(domain, server, port, 5000);
    if (dnsResponse != nullptr) {
        string ips = st::utils::ipv4::ipsToStr(dnsResponse->ips);
        Logger::INFO << "ips:" << ips << END;
        delete dnsResponse;
        ASSERT_STRNE("", ips.c_str());
    }
    ASSERT_TRUE(dnsResponse != nullptr);
}

void testUdp(const string &domain) {
    testUdp(domain, "127.0.0.1", 5353);
}

void testUdp(unordered_set<string> &domains, const string &server, const uint32_t port, const int parral) {
    vector<std::function<void()>> tests;
    for (string domain : domains) {
        tests.emplace_back([=]() {
            testUdp(domain, server, port);
        });
    }
    testParallel(tests, parral);
}

void testUdp(unordered_set<string> &domains, const int parral) {
    testUdp(domains, "127.0.0.1", 5353, parral);
}

// TEST_F(IntegrationTests, testDNS) {
//     // testUdp("qvbuy.com");

//     unordered_set<string> cnDomains;
//     unordered_set<string> overSeaDomains;
//     file::read("../confs/test/domains/CN", cnDomains);
//     file::read("../confs/test/domains/OVERSEA", overSeaDomains);
//     EXPECT_FALSE(cnDomains.empty());
//     EXPECT_FALSE(overSeaDomains.empty());
//     testUdp(cnDomains, "192.168.31.1", 53, 10);
//     // testUdp(cnDomains, 10);
// }
TEST_F(IntegrationTests, testDNS) {
    testParallel(
            [=]() {
                string resultStr;
                shell::exec("dig CNAME  baidu.com @127.0.0.1 -p 5353", resultStr);
            },
            30, 30);
}
