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
        std::this_thread::sleep_for(std::chrono::seconds(5));
        dnsServer->shutdown();
        th->join();
        APMLogger::disable();
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
        auto task = [=, &taskCount]() {
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
                DNSClient::INSTANCE.udpDns(domain, server, port, 5000, [](UdpDNSResponse *dnsResponse) {
                    EXPECT_NE(dnsResponse, nullptr);
                    EXPECT_STRNE("", st::utils::ipv4::ipsToStr(dnsResponse->ips).c_str());
                    if (dnsResponse != nullptr) {
                        cout << st::utils::ipv4::ipsToStr(dnsResponse->ips) << endl;
                        delete dnsResponse;
                    }
                });
            },
            count, parral);
}
void testUdp(const string &domain, const string &server, const uint32_t port) {
    mutex lock;
    lock.lock();
    UdpDNSResponse *result = nullptr;
    DNSClient::INSTANCE.udpDns(domain, server, port, 10000, [&](UdpDNSResponse *dnsResponse) {
        result = dnsResponse;
        lock.unlock();
    });
    lock.lock();
    ASSERT_TRUE(result != nullptr);

    if (result != nullptr) {
        string ips = st::utils::ipv4::ipsToStr(result->ips);
        Logger::DEBUG << domain << "ips:" << ips << END;
        delete result;
        ASSERT_STRNE("", ips.c_str()) << "expect not empty";
    }
    ASSERT_TRUE(result != nullptr);
    lock.unlock();
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
TEST_F(IntegrationTests, testDNS) {
    for (int i = 0; i < 10; i++) {
        testUdp("baidu.com", "127.0.0.1", 5353);
    }
    for (int i = 0; i < 10; i++) {
        testUdp("google.com", "127.0.0.1", 5353);
    }
}
