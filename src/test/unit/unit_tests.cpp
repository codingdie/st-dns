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
