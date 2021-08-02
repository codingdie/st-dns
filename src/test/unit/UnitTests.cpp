//
// Created by codingdie on 2020/6/27.
//
#include <gtest/gtest.h>
#include "DNSClient.h"
#include "DNSServer.h"
#include "STUtils.h"
#include <iostream>

#include "DNS.h"
#include "WhoisClient.h"
#include <boost/pool/pool.hpp>
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
    st::utils::dns::DNSReverseSHM dnsSHM(false);
    st::utils::dns::DNSReverseSHM dnsSHMRead;
    for (int i = 0; i < 20000; i++) {
        cout << i << std::endl;
        dnsSHM.addOrUpdate(i, to_string(i) + "baidu.com");
    }
    for (int i = 0; i < 20000; i++) {
        auto host = dnsSHMRead.query(i);
        ASSERT_STREQ((to_string(i) + "baidu.com").c_str(), host.c_str());
    }

    auto host2 = dnsSHMRead.query(20000+1);
    ASSERT_STREQ("", host2.c_str());
}
