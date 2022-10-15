//
// Created by codingdie on 2020/6/27.
//
#include "st.h"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <vector>


TEST(UnitTests, test_base64) {
    string oriStr = "asdkhb123b";
    string base64Str = st::utils::base64::encode(oriStr);
    ASSERT_STREQ("YXNka2hiMTIzYg==", base64Str.c_str());
    string decodeStr = st::utils::base64::decode(base64Str);
    ASSERT_STREQ(oriStr.c_str(), decodeStr.c_str());
}

TEST(UnitTests, test_shm_kv) {
    auto ns = "TEST";
    kv::shm_kv::create(ns, 5 * 1024 * 1024);
    kv::shm_kv::share(ns)->clear();
    uint64_t size0 = kv::shm_kv::share(ns)->free_size();
    ASSERT_TRUE(size0 > 1024 * 1024 * 3);
    int count = 10000;
    for (int i = 0; i < count; i++) {
        kv::shm_kv::share(ns)->put(to_string(i), to_string(i) + "baidu.com");
    }
    ASSERT_TRUE(size0 > 1024 * 1024 * 3);

    uint64_t size01 = kv::shm_kv::share(ns)->free_size();
    ASSERT_TRUE(size0 > size01);

    for (int i = count; i < count * 2; i++) {
        kv::shm_kv::share(ns)->put(to_string(i), to_string(i) + "baidu.com");
    }
    uint64_t size02 = kv::shm_kv::share(ns)->free_size();
    ASSERT_TRUE(size01 > size02);
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < 100; j++) {
            auto host = kv::shm_kv::share(ns)->get(to_string(i));
            ASSERT_STREQ((to_string(i) + "baidu.com").c_str(), host.c_str());
        }
    }

    auto testIp = count * 2 + 1;
    auto host2 = st::dns::shm::share().reverse_resolve(testIp);
    ASSERT_STREQ(st::utils::ipv4::ip_to_str(testIp).c_str(), host2.c_str());
}

TEST(UnitTests, test_area_ip) {
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("TW", "118.163.193.132"));
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("cn", "223.5.5.5"));
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("cn", "220.181.38.148"));
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("cn", "123.117.76.165"));
    ASSERT_TRUE(!st::areaip::manager::uniq().is_area_ip("cn", "172.217.5.110"));
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("us", "172.217.5.110"));
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("jp", "114.48.198.220"));
    ASSERT_TRUE(!st::areaip::manager::uniq().is_area_ip("us", "114.48.198.220"));
    ASSERT_TRUE(!st::areaip::manager::uniq().is_area_ip("cn", "218.146.11.198"));
    ASSERT_TRUE(st::areaip::manager::uniq().is_area_ip("kr", "218.146.11.198"));
}


TEST(UnitTests, test_ip_area_network) {
    st::areaip::manager::uniq().is_area_ip("JP", "14.0.42.1");
    st::areaip::manager::uniq().is_area_ip("TW", "118.163.193.132");

    std::this_thread::sleep_for(std::chrono::seconds(5));
    bool result = st::areaip::manager::uniq().is_area_ip("JP", "14.0.42.1");
    ASSERT_TRUE(result);
}


TEST(UnitTests, test_udp_console) {
    auto console = new st::console::udp_console("127.0.0.1", 2222);
    namespace po = boost::program_options;
    console->desc.add_options()("version,v", "print version string")("help", "produce help message");
    console->impl = [](const vector<std::string> &commands, const boost::program_options::variables_map &options) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return make_pair(true, "command:" + strutils::join(commands, " ") + " , opts size:" + to_string(options.size()));
    };
    console->start();
    auto begin = time::now();
    string result = st::console::client::command("127.0.0.1", 2222, "xx asd --help 123", 1000);
    auto end = time::now();
    auto cost = end - begin;
    ASSERT_TRUE(cost > 800);
    ASSERT_TRUE(cost < 1200);
    ASSERT_STREQ("", result.c_str());
    begin = time::now();
    auto newResult = st::console::client::command("127.0.0.1", 2222, "xx asd --help 123", 6000);
    end = time::now();
    cost = end - begin;
    ASSERT_TRUE(cost >= 3000);
    cout << newResult << endl;
    ASSERT_STREQ("success\ncommand:xx asd , opts size:1", newResult.c_str());
    delete console;
}


TEST(UnitTests, test_option_console) {}
