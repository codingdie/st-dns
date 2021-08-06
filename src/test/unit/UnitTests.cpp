//
// Created by codingdie on 2020/6/27.
//
#include <gtest/gtest.h>
#include "DNSClient.h"
#include "DNSServer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
TEST(UnitTests, testBase64) {
    string oriStr = "asdkhb123b";
    string base64Str = st::utils::base64::encode(oriStr);
    ASSERT_STREQ("YXNka2hiMTIzYg==", base64Str.c_str());
    string decodeStr = st::utils::base64::decode(base64Str);
    ASSERT_STREQ(oriStr.c_str(), decodeStr.c_str());
}

TEST(UnitTests, testSHM) {
    int count = 100;
    st::utils::dns::DNSReverseSHM dnsSHM(false);
    st::utils::dns::DNSReverseSHM dnsSHMRead;
    for (int i = 0; i < count; i++) {
        dnsSHM.addOrUpdate(i, to_string(i) + "baidu.com");
    }
    for (int i = 0; i < count; i++) {
        auto host = dnsSHMRead.query(i);
        ASSERT_STREQ((to_string(i) + "baidu.com").c_str(), host.c_str());
    }

    auto testIp = count + 1;
    auto host2 = dnsSHMRead.query(testIp);
    ASSERT_STREQ(st::utils::ipv4::ipToStr(testIp).c_str(), host2.c_str());
}


void testUdp(const string &domain, const string &server, const uint32_t port) {
    mutex lock;
    lock.lock();
    UdpDNSResponse *result = nullptr;
    DNSClient::INSTANCE.udpDns(domain, server, port, 5000, [&](UdpDNSResponse *dnsResponse) {
        result = dnsResponse;
        lock.unlock();
    });
    lock.lock();
    ASSERT_TRUE(result != nullptr);

    if (result != nullptr) {
        string ips = st::utils::ipv4::ipsToStr(result->ips);
        Logger::INFO << domain << "ips:" << ips << END;
        delete result;
        ASSERT_STRNE("", ips.c_str()) << "expect not empty";
    }
    ASSERT_TRUE(result != nullptr);
    lock.unlock();
}

TEST(UnitTests, testUdp) {
    testUdp("baidu.com", "114.114.114.114", 53);
}
