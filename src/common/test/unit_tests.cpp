//
// Created by codingdie on 2020/6/27.
//
#include "st.h"
#include "taskquque/task_queue.h"
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <future>
#include <gtest/gtest.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace {
    string test_runtime_dir() {
        const char *configured_runtime_dir = std::getenv("ST_RUNTIME_DIR");
        if (configured_runtime_dir != nullptr && configured_runtime_dir[0] != '\0') {
            return configured_runtime_dir;
        }
        return "/tmp/st";
    }

    string test_area_ip_dir() {
        const char *configured_area_ip_dir = std::getenv("ST_AREA_IP_DIR");
        if (configured_area_ip_dir != nullptr && configured_area_ip_dir[0] != '\0') {
            return configured_area_ip_dir;
        }
        return "/etc/area-ips";
    }

    bool perf_log_contains(const string &needle) {
        const string perf_dir = test_runtime_dir() + "/perf";
        if (!boost::filesystem::exists(perf_dir)) {
            return false;
        }
        for (const auto &entry : boost::filesystem::directory_iterator(perf_dir)) {
            if (!boost::filesystem::is_regular_file(entry.path())) {
                continue;
            }
            std::ifstream input(entry.path().string().c_str());
            std::string line;
            while (std::getline(input, line)) {
                if (line.find(needle) != string::npos) {
                    return true;
                }
            }
        }
        return false;
    }

    int perf_log_match_count(const string &needle) {
        const string perf_dir = test_runtime_dir() + "/perf";
        int match_count = 0;
        if (!boost::filesystem::exists(perf_dir)) {
            return match_count;
        }
        for (const auto &entry : boost::filesystem::directory_iterator(perf_dir)) {
            if (!boost::filesystem::is_regular_file(entry.path())) {
                continue;
            }
            std::ifstream input(entry.path().string().c_str());
            std::string line;
            while (std::getline(input, line)) {
                if (line.find(needle) != string::npos) {
                    match_count++;
                }
            }
        }
        return match_count;
    }
}


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
    const string area_ip_dir = test_area_ip_dir();
    st::utils::file::mkdirs(area_ip_dir);
    {
        std::ofstream area_file(area_ip_dir + "/IP_NET_AREA", std::ios::out | std::ios::trunc);
        area_file << "223.5.5.5\tCN\n";
        area_file << "220.181.38.148\tCN\n";
        area_file << "123.117.76.165\tCN\n";
        area_file << "172.217.5.110\tUS\n";
        area_file << "114.48.198.220\tJP\n";
    }
    st::areaip::manager::uniq().start();
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
    st::areaip::manager::uniq().stop();
}

TEST(unit_tests, area_ip_start_stop_is_idempotent) {
    st::areaip::manager manager;

    ASSERT_FALSE(manager.started());
    ASSERT_EQ("LAN", manager.get_area(st::utils::ipv4::str_to_ip("192.168.1.1")));
    manager.async_load_ip_info_from_net(st::utils::ipv4::str_to_ip("8.8.8.8"));
    ASSERT_FALSE(manager.started());

    manager.start();
    ASSERT_TRUE(manager.started());

    manager.start();
    ASSERT_TRUE(manager.started());

    manager.stop();
    ASSERT_FALSE(manager.started());

    manager.stop();
    ASSERT_FALSE(manager.started());

    manager.start();
    ASSERT_TRUE(manager.started());

    manager.stop();
    ASSERT_FALSE(manager.started());
}

TEST(unit_tests, area_ip_stop_cleans_pending_delay_timers) {
    st::areaip::manager manager;

    for (int i = 0; i < 5; i++) {
        manager.start();
        ASSERT_TRUE(manager.started());

        manager.async_load_ip_info_from_net(st::utils::ipv4::str_to_ip("8.8.8.8"));
        manager.async_load_ip_info_from_net(st::utils::ipv4::str_to_ip("1.1.1.1"));

        manager.stop();
        ASSERT_FALSE(manager.started());
    }
}

TEST(unit_tests, area_ip_restart_ignores_old_pending_delay_handlers) {
    st::areaip::manager manager;
    uint32_t ip = st::utils::ipv4::str_to_ip("8.8.8.8");

    for (int i = 0; i < 5; i++) {
        manager.start();
        manager.async_load_ip_info_from_net(ip);
        manager.stop();

        manager.start();
        manager.async_load_ip_info_from_net(ip);
        manager.stop();

        ASSERT_FALSE(manager.started());
    }
}


TEST(unit_tests, test_udp_console) {
    auto console = new st::console::udp_console("127.0.0.1", 2222);
    namespace po = boost::program_options;
    std::mutex handler_mutex;
    std::condition_variable handler_started;
    bool is_handling_command = false;
    console->desc.add_options()("version,v", "print version string")("help", "produce help message");
    console->impl = [&](const vector<std::string> &commands, const boost::program_options::variables_map &options) {
        {
            std::lock_guard<std::mutex> lock(handler_mutex);
            is_handling_command = true;
        }
        handler_started.notify_one();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return make_pair(true,
                         "command:" + strutils::join(commands, " ") + " , opts size:" + to_string(options.size()));
    };
    console->start();

    std::promise<pair<bool, string>> timeout_result_promise;
    auto timeout_result_future = timeout_result_promise.get_future();
    std::thread timeout_client([&]() {
        timeout_result_promise.set_value(st::console::client::command("127.0.0.1", 2222, "xx asd --help 123", 100));
    });
    bool handling_started = false;
    {
        std::unique_lock<std::mutex> lock(handler_mutex);
        handling_started = handler_started.wait_for(lock, std::chrono::milliseconds(500), [&]() {
            return is_handling_command;
        });
    }
    auto result = timeout_result_future.get();
    timeout_client.join();
    ASSERT_TRUE(handling_started);
    ASSERT_STREQ("network error!", result.second.c_str());

    auto newResult = st::console::client::command("127.0.0.1", 2222, "xx asd --help 123", 1000);
    ASSERT_STREQ("command:xx asd , opts size:1", newResult.second.c_str());
    delete console;
}

TEST(unit_tests, udp_console_command_cancels_without_waiting_for_timeout) {
    io_context silent_server_context;
    udp::socket silent_server(silent_server_context, udp::endpoint(udp::v4(), 0));
    const auto port = silent_server.local_endpoint().port();
    std::atomic_bool cancelled{false};
    std::thread canceller([&cancelled]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        cancelled.store(true);
    });

    auto begin = time::now();
    auto result = st::console::client::command(
            "127.0.0.1", port, "noop", 1000, [&cancelled]() { return cancelled.load(); });
    auto cost = time::now() - begin;
    canceller.join();

    ASSERT_FALSE(result.first);
    ASSERT_LT(cost, 200);
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
    mutex result_lock;
    condition_variable result_ready;
    st::task::queue<string> que("st-unit-test", 100, 1, [&result, &result_lock, &result_ready, &que](const priority_task<string> &task) {
        {
            lock_guard<mutex> lock(result_lock);
            result.emplace_back(task.get_input());
        }
        result_ready.notify_all();
        que.complete(task);
    });
    auto wait_for_result_count = [&](size_t count) {
        unique_lock<mutex> lock(result_lock);
        return result_ready.wait_for(lock, std::chrono::seconds(2), [&result, count]() { return result.size() == count; });
    };

    for (int i = 0; i < total; ++i) {
        priority_task<string> task(to_string(i), i, "" + i);
        ASSERT_TRUE(que.submit(task));
    }
    ASSERT_TRUE(wait_for_result_count(total));
    {
        lock_guard<mutex> lock(result_lock);
        ASSERT_EQ(5, result.size());
        ASSERT_TRUE(result[0] == to_string(total - 1));
        result.clear();
    }

    for (int i = 0; i < total; ++i) {
        priority_task<string> task(to_string(i), 0, "" + i);
        ASSERT_TRUE(que.submit(task));
    }
    ASSERT_TRUE(wait_for_result_count(total));
    {
        lock_guard<mutex> lock(result_lock);
        ASSERT_EQ(5, result.size());
        ASSERT_TRUE(result[0] == "0");
        result.clear();
    }

    priority_task<string> task("0", 0, "123");
    ASSERT_TRUE(que.submit(task));
    ASSERT_FALSE(que.submit(task));
    ASSERT_TRUE(wait_for_result_count(1));
    {
        lock_guard<mutex> lock(result_lock);
        ASSERT_EQ(1, result.size());
        ASSERT_TRUE(result[0] == "0");
    }
    ASSERT_EQ(0, que.size());
}


TEST(unit_tests, test_limie_file_cnt) {
    auto path = "/tmp/" + st::utils::strutils::uuid();
    st::utils::file::mkdirs(path);
    for (int i = 0; i < 10; i++) {
        auto file_path = path + "/" + to_string(i) + ".txt";
        st::utils::file::create_if_not_exits(file_path);
        boost::filesystem::last_write_time(file_path, std::time(nullptr) - (10 - i));
    }
    ASSERT_EQ(4, st::utils::file::limit_file_cnt(path, 6));
}

TEST(unit_tests, logger_writes_log_files) {
    boost::property_tree::ptree tree;
    st::utils::logger::init(tree);
    st::utils::apm_logger::init();
    for (int i = 0; i < 100; i++) {
        st::utils::apm_logger::perf("123", {}, 100);
        st::utils::logger::INFO << i << time::now_str() << END;
    }
    st::utils::logger::disable();
    st::utils::apm_logger::disable();
    ASSERT_TRUE(st::utils::file::exists(test_runtime_dir() + "/" + st::utils::logger::TAG + ".log"));
    ASSERT_TRUE(st::utils::file::get_file_cnt(test_runtime_dir() + "/perf") >= 1);
}

TEST(unit_tests, apm_logger_disable_can_skip_status_log) {
    boost::property_tree::ptree tree;
    st::utils::logger::init(tree);
    st::utils::apm_logger::init();
    st::utils::apm_logger::perf("skip-status-log", {}, 100);

    st::utils::apm_logger::disable(false);
    st::utils::logger::disable();

    ASSERT_TRUE(st::utils::file::get_file_cnt(test_runtime_dir() + "/perf") >= 1);
}

TEST(unit_tests, logger_disable_does_not_close_apm_logger) {
    const string log_path = test_runtime_dir() + "/logger-status-test.log";
    const string metric_name = "status-log-" + st::utils::strutils::uuid();
    if (st::utils::file::exists(log_path)) {
        st::utils::file::del(log_path);
    }

    boost::property_tree::ptree tree;
    tree.put("log.tag", "logger-status-test");
    st::utils::logger::init(tree);
    st::utils::apm_logger::init();
    st::utils::apm_logger::perf(metric_name, {}, 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    st::utils::logger::disable();

    string log_content = st::utils::file::read(log_path);
    ASSERT_EQ(string::npos, log_content.find("apm log report at"));

    st::utils::apm_logger::disable();
    ASSERT_TRUE(perf_log_contains(metric_name));

    // logger 已关闭，APM status log 会回退到 stdout，而不是继续写文件。
    log_content = st::utils::file::read(log_path);
    ASSERT_EQ(string::npos, log_content.find("apm log report at"));
}

TEST(unit_tests, logger_init_disable_is_idempotent) {
    const string log_path = test_runtime_dir() + "/logger-idempotent.log";
    const string perf_dir = test_runtime_dir() + "/perf";
    st::utils::file::mkdirs(perf_dir);
    const int perf_file_count_before = st::utils::file::get_file_cnt(perf_dir);

    if (st::utils::file::exists(log_path)) {
        st::utils::file::del(log_path);
    }

    boost::property_tree::ptree tree;
    tree.put("log.tag", "logger-idempotent");

    st::utils::logger::init(tree);
    st::utils::logger::init(tree);
    st::utils::logger::INFO << "logger init disable idempotent" << END;
    st::utils::apm_logger::perf("logger-without-apm-init", {}, 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    st::utils::logger::disable();
    st::utils::logger::disable();

    ASSERT_NE(string::npos,
              st::utils::file::read(log_path).find("logger init disable idempotent"));
    ASSERT_EQ(perf_file_count_before, st::utils::file::get_file_cnt(perf_dir));
}

TEST(unit_tests, area_ip_sync_reports_load_net_ip_info_health_metric) {
    const string perf_dir = test_runtime_dir() + "/perf";
    boost::filesystem::remove_all(perf_dir);
    st::utils::file::mkdirs(perf_dir);
    st::areaip::manager::uniq().stop();
    st::utils::apm_logger::disable(false);
    st::utils::logger::disable();

    boost::property_tree::ptree tree;
    tree.put("log.tag", "area-ip-health-test");
    st::utils::logger::init(tree);
    st::utils::apm_logger::init();
    st::areaip::manager::uniq().start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    st::areaip::manager::uniq().stop();
    st::utils::apm_logger::disable(false);
    st::utils::logger::disable();

    bool found = false;
    for (int i = 0; i < 10; ++i) {
        if (perf_log_match_count("\"source\":\"cache-sync\"") == 1 &&
            perf_log_contains("\"name\":\"load-net-ip-info\"") &&
            perf_log_contains("\"success\":1")) {
            found = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ASSERT_TRUE(found) << "cache-sync health metric not flushed in current test environment";
}

TEST(unit_tests, apm_init_disable_is_idempotent) {
    const string first_metric_name = "apm-first-" + st::utils::strutils::uuid();
    const string second_metric_name = "apm-second-" + st::utils::strutils::uuid();

    st::utils::apm_logger::disable(false);
    st::utils::apm_logger::disable(false);
    st::utils::apm_logger::init();
    st::utils::apm_logger::init();
    st::utils::apm_logger::perf(first_metric_name, {}, 100);
    st::utils::apm_logger::disable(false);
    st::utils::apm_logger::disable(false);

    ASSERT_EQ(1, perf_log_match_count(first_metric_name));

    st::utils::apm_logger::init();
    st::utils::apm_logger::init();
    st::utils::apm_logger::perf(second_metric_name, {}, 100);
    st::utils::apm_logger::disable(false);
    st::utils::apm_logger::disable(false);

    ASSERT_EQ(1, perf_log_match_count(first_metric_name));
    ASSERT_EQ(1, perf_log_match_count(second_metric_name));
}
