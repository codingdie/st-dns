//
// Created by codingdie on 2020/6/27.
//
#include <gtest/gtest.h>
#include "dns_client.h"
#include "dns_server.h"
#include "st.h"
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
}

TEST(UnitTests, testAreaIP) {
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



TEST(UnitTests, testIPAreaNetwork) {
    st::areaip::manager::uniq().is_area_ip("JP", "14.0.42.1");
    st::areaip::manager::uniq().is_area_ip("TW", "118.163.193.132");

    std::this_thread::sleep_for(std::chrono::seconds(5));
    bool result = st::areaip::manager::uniq().is_area_ip("JP", "14.0.42.1");
    ASSERT_TRUE(result);
}


TEST(UnitTests, test_ip_sort) {
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    vector<dns_ip_record> ip_records;
    auto create = [](uint32_t ip, bool match, bool forbid) {
        dns_ip_record b;
        b.ip = ip;
        b.match_area = match;
        b.forbid = forbid;
        return b;
    };
    ip_records.emplace_back(create(3, true, false));
    ip_records.emplace_back(create(1, false, false));
    ip_records.emplace_back(create(2, true, true));
    ip_records.emplace_back(create(4, true, false));
    std::shuffle(ip_records.begin(), ip_records.end(), std::default_random_engine(seed));
    std::sort(ip_records.begin(), ip_records.end(), dns_ip_record::compare);
    for (auto item : ip_records) {
        logger::INFO << item.ip;
    }
    logger::INFO << END;
    ASSERT_EQ(1, ip_records[3].ip);
    ASSERT_EQ(2, ip_records[2].ip);
}


TEST(UnitTests, test_dns_cache) {
    dns_record_manager::uniq().add("test01.com", {1, 2, 3}, "192.168.31.2", 60);
    dns_record_manager::uniq().add("test01.com", {1, 2, 3}, "192.168.31.1", 60);
    dns_record_manager::uniq().add("test01.com", {1, 2}, "192.168.31.1", 60);
    const proto::records &records = dns_record_manager::uniq().get_dns_records_pb("test01.com");
    auto ma = records.map();
    for (const auto &item : ma) {
        cout << item.first << endl;
    }
    ASSERT_EQ(2, records.map_size());
    ASSERT_EQ(2, records.map().at("192.168.31.1").ips_size());
    ASSERT_EQ(3, records.map().at("192.168.31.2").ips_size());
}
