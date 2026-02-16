//
// Created by codingdie on 2020/6/27.
//
#include "st.h"
#include "taskquque/task_queue.h"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <vector>


TEST(unit_tests, test_base64) {
    string oriStr = "asdkhb123b";
    string base64Str = st::utils::base64::encode(oriStr);
    ASSERT_STREQ("YXNka2hiMTIzYg==", base64Str.c_str());
    string decodeStr = st::utils::base64::decode(base64Str);
    ASSERT_STREQ(oriStr.c_str(), decodeStr.c_str());
}

TEST(unit_tests, test_shm) {
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

TEST(unit_tests, test_area_ip) {
    // 辅助函数：轮询等待直到 is_area_ip 返回预期结果（或超时）
    auto wait_for_area_ip = [](const string& area, const string& ip_str, bool expected, int timeout_ms = 6000) {
        uint32_t ip = st::utils::ipv4::str_to_ip(ip_str);
        auto start = std::chrono::steady_clock::now();
        while (true) {
            bool result = st::areaip::manager::uniq().is_area_ip(area, ip);
            if (result == expected) {
                return true;  // 成功
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeout_ms) {
                return false;  // 超时
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    };

    // 首次调用触发异步加载，然后轮询等待加载完成
    ASSERT_TRUE(wait_for_area_ip("cn", "223.5.5.5", true));
    ASSERT_TRUE(wait_for_area_ip("cn", "220.181.38.148", true));
    ASSERT_TRUE(wait_for_area_ip("cn", "123.117.76.165", true));
    ASSERT_TRUE(wait_for_area_ip("cn", "172.217.5.110", false));
    ASSERT_TRUE(wait_for_area_ip("us", "172.217.5.110", true));
    ASSERT_TRUE(wait_for_area_ip("jp", "114.48.198.220", true));
    ASSERT_TRUE(wait_for_area_ip("us", "114.48.198.220", false));
    ASSERT_TRUE(wait_for_area_ip("cn", "218.146.11.198", false));
    ASSERT_TRUE(wait_for_area_ip("kr", "218.146.11.198", true));
}


TEST(unit_tests, test_ip_area_network) {
    st::areaip::manager::uniq().is_area_ip("JP", "14.0.42.1");
    st::areaip::manager::uniq().is_area_ip("TW", "118.163.193.132");

    std::this_thread::sleep_for(std::chrono::seconds(5));
    bool result = st::areaip::manager::uniq().is_area_ip("JP", "14.0.42.1");
    ASSERT_TRUE(result);
}


TEST(unit_tests, test_udp_console) {
    auto console = new st::console::udp_console("127.0.0.1", 2222);
    namespace po = boost::program_options;
    console->desc.add_options()("version,v", "print version string")("help", "produce help message");
    console->impl = [](const vector<std::string> &commands, const boost::program_options::variables_map &options) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return make_pair(true,
                         "command:" + strutils::join(commands, " ") + " , opts size:" + to_string(options.size()));
    };
    console->start();
    auto begin = time::now();
    auto result = st::console::client::command("127.0.0.1", 2222, "xx asd --help 123", 1000);
    auto end = time::now();
    auto cost = end - begin;
    ASSERT_TRUE(cost > 800);
    ASSERT_TRUE(cost < 1200);
    ASSERT_STREQ("network error!", result.second.c_str());
    begin = time::now();
    auto newResult = st::console::client::command("127.0.0.1", 2222, "xx asd --help 123", 6000);
    end = time::now();
    cost = end - begin;
    ASSERT_TRUE(cost >= 3000);
    cout << newResult.second << endl;
    ASSERT_STREQ("command:xx asd , opts size:1", newResult.second.c_str());
    delete console;
}
TEST(unit_tests, test_dns_resolve) {
    ASSERT_STRNE(st::utils::ipv4::ips_to_str(st::utils::dns::query("114.114.114.114", "google.com")).c_str(), "");
    ASSERT_STRNE(st::utils::ipv4::ips_to_str(st::utils::dns::query("google.com")).c_str(), "");
    ASSERT_STREQ(st::utils::ipv4::ips_to_str(st::utils::dns::query("0.0.0.0", "google.com")).c_str(), "");
}

// Demonstrate some basic assertions.
TEST(unit_tests, test_area_2_mark) {
    uint32_t mark = st::areaip::area_to_code("CN");
    string area = st::areaip::code_to_area(mark);
    ASSERT_STREQ("CN", area.c_str());
    mark = st::areaip::area_to_code("US");
    area = st::areaip::code_to_area(mark);
    ASSERT_STREQ("US", area.c_str());
}

TEST(unit_tests, test_ip_str) {
    ASSERT_TRUE(st::utils::ipv4::str_to_ip("1.b.c.d") == 0);
    ASSERT_TRUE(st::utils::ipv4::str_to_ip("1.1.1.1") == 16843009);
    ASSERT_TRUE(st::utils::ipv4::str_to_ip("112.2.1.1") == 1879179521);
    ASSERT_TRUE(st::utils::ipv4::str_to_ip("1.1.1.1.1") == 0);
    ASSERT_TRUE(st::utils::ipv4::str_to_ip(".1.1.1.1") == 0);
    ASSERT_TRUE(st::utils::ipv4::str_to_ip("baidu.com") == 0);
}

TEST(unit_tests, test_disk_kv) {
    st::kv::disk_kv kv("test", 1024 * 1024);
    auto time = time::now_str();
    kv.put("time", time);
    kv.put("author", "哈哈");
    ASSERT_TRUE(st::utils::ipv4::str_to_ip("1.b.c.d") == 0);
    ASSERT_TRUE(st::utils::ipv4::str_to_ip("1.1.1.1") == 16843009);
    ASSERT_TRUE(st::utils::ipv4::str_to_ip("112.2.1.1") == 1879179521);
    ASSERT_TRUE(st::utils::ipv4::str_to_ip("1.1.1.1.1") == 0);
    ASSERT_TRUE(st::utils::ipv4::str_to_ip(".1.1.1.1") == 0);
    ASSERT_TRUE(st::utils::ipv4::str_to_ip("baidu.com") == 0);
}

TEST(unit_tests, test_disk_kv_expire) {
    st::kv::disk_kv kv("test_expire", 1024 * 1024);
    kv.clear();

    // 测试永不过期的记录 (expire = 0)
    kv.put("never_expire", "value1", 0);
    ASSERT_STREQ("value1", kv.get("never_expire").c_str());

    // 测试未过期的记录 (expire = 10秒后)
    kv.put("not_expired", "value2", 10);
    ASSERT_STREQ("value2", kv.get("not_expired").c_str());

    // 测试已过期的记录 (expire = 1秒后)
    kv.put("will_expire", "value3", 1);
    ASSERT_STREQ("value3", kv.get("will_expire").c_str());

    // 等待 2 秒，让记录过期
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 已过期的记录应该返回空字符串
    ASSERT_STREQ("", kv.get("will_expire").c_str());

    // 未过期的记录应该仍然存在
    ASSERT_STREQ("value2", kv.get("not_expired").c_str());

    // 永不过期的记录应该仍然存在
    ASSERT_STREQ("value1", kv.get("never_expire").c_str());
}

TEST(unit_tests, test_task_queue) {
    using namespace st::task;
    int total = 5;
    vector<string> result;
    st::task::queue<string> que("st-unit-test", 1, 1, [&result, &que](const priority_task<string> &task) {
        result.emplace_back(task.get_input());
        que.complete(task);
    });
    for (int i = 0; i < total; ++i) {
        priority_task<string> task(to_string(i), i, "" + i);
        ASSERT_TRUE(que.submit(task));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds((1 + total) * 1000));
    ASSERT_EQ(5, result.size());
    ASSERT_TRUE(result[0] == to_string(total - 1));

    result.clear();
    for (int i = 0; i < total; ++i) {
        priority_task<string> task(to_string(i), 0, "" + i);
        ASSERT_TRUE(que.submit(task));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds((1 + total) * 1000));
    ASSERT_EQ(5, result.size());
    ASSERT_TRUE(result[0] == "0");


    result.clear();
    priority_task<string> task("0", 0, "123");
    ASSERT_TRUE(que.submit(task));
    ASSERT_FALSE(que.submit(task));
    std::this_thread::sleep_for(std::chrono::milliseconds((1 + total) * 1000));
    ASSERT_EQ(1, result.size());
    ASSERT_TRUE(result[0] == "0");
    ASSERT_EQ(0, que.size());
}


TEST(unit_tests, test_limie_file_cnt) {
    auto path = "/tmp/" + st::utils::strutils::uuid();
    st::utils::file::mkdirs(path);
    for (int i = 0; i < 10; i++) {
        st::utils::file::create_if_not_exits(path + "/" + to_string(i) + ".txt");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    ASSERT_EQ(4, st::utils::file::limit_file_cnt(path, 6));
}

TEST(unit_tests, test_logger) {
    boost::property_tree::ptree tree;
    st::utils::logger::init(tree);
    for (int i = 0; i < 1000000; i++) {
        st::utils::apm_logger::perf("123", {}, 100);
        st::utils::logger::INFO << i << time::now_str() << END;
    }
    st::utils::logger::disable();
    ASSERT_TRUE(st::utils::file::get_file_cnt("/tmp/st") >= 4);
    ASSERT_TRUE(st::utils::file::get_file_cnt("/tmp/st/perf") >= 1);
}