#include "utils/area_ip.h"
#include "utils/file.h"
#include <gtest/gtest.h>
#include <fstream>
#include <future>
#include <thread>

namespace st {
namespace areaip {
// 只用于替换下载边界、控制后台队列和重试时钟，测试不访问公网。
class manager_test_access {
public:
    static void post(manager &value, std::function<void()> task) { value.ctx.post(std::move(task)); }
    static void set_download(manager &value, std::function<std::pair<int, string>(const string &)> download) {
        value.area_download = std::move(download);
    }
    static void allow_retry(manager &value, const string &area) {
        std::lock_guard<std::mutex> guard(value.default_lock);
        value.area_retry_after[area] = std::chrono::steady_clock::now();
    }
};
}
}

namespace {
using st::areaip::manager;
using st::areaip::manager_test_access;
namespace ipv4 = st::utils::ipv4;

string area_path(const string &area) {
    const char *root = std::getenv("ST_AREA_IP_DIR");
    return string(root ? root : "/etc/area-ips") + "/" + area;
}

void write_area(const string &area, const string &data) {
    std::ofstream output(area_path(area));
    output << data;
}

bool wait_for_area(manager &value, const string &expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (value.get_area(ipv4::str_to_ip("203.0.113.1"), false) == expected) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}
}

TEST(area_ip_loading, slow_load_does_not_block_queries_or_duplicate_requests) {
    manager value;
    boost::filesystem::remove(area_path("ZZ"));
    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    std::atomic<int> downloads{0};
    manager_test_access::set_download(value, [&](const string &) {
        if (++downloads == 1) {
            entered.set_value();
        }
        released.wait();
        return std::make_pair(200, string("203.0.113.0/24\n"));
    });
    value.start();
    auto load = std::async(std::launch::async, [&]() { return value.load_area_ips("zz"); });
    const auto download_status = entered.get_future().wait_for(std::chrono::seconds(2));
    auto query = std::async(std::launch::async, [&]() {
        const auto area = value.get_area(ipv4::str_to_ip("192.168.1.1"), false);
        const auto missing = value.get_area(ipv4::str_to_ip("203.0.113.1"), false);
        for (int i = 0; i < 30; ++i) {
            value.async_load_area_ips(i % 2 ? "ZZ" : "zz");
        }
        const bool duplicate = value.load_area_ips("!zz");
        return area == "LAN" && missing == "default" && !duplicate;
    });
    const auto query_status = query.wait_for(std::chrono::milliseconds(500));
    // 先解除下载阻塞，再断言，避免回归时测试线程无法退出。
    release.set_value();
    EXPECT_EQ(std::future_status::ready, download_status);
    EXPECT_EQ(std::future_status::ready, query_status);
    EXPECT_TRUE(query.get());
    EXPECT_TRUE(load.get());
    value.stop();
    EXPECT_EQ(1, downloads.load());
    EXPECT_EQ("ZZ", value.get_area(ipv4::str_to_ip("203.0.113.1"), false));
}

TEST(area_ip_loading, failed_load_backs_off_per_country_and_can_retry) {
    manager value;
    boost::filesystem::remove(area_path("ZZ"));
    std::atomic<int> downloads{0};
    manager_test_access::set_download(value, [&](const string &) {
        if (++downloads == 1) {
            return std::make_pair(404, string());
        }
        return std::make_pair(200, string("203.0.113.0/24\n"));
    });
    EXPECT_FALSE(value.load_area_ips("ZZ"));
    EXPECT_FALSE(value.load_area_ips("zz"));

    value.start();
    for (int i = 0; i < 30; ++i) {
        value.async_load_area_ips("ZZ");
    }
    std::promise<void> drained;
    manager_test_access::post(value, [&]() { drained.set_value(); });
    drained.get_future().wait();
    EXPECT_EQ(1, downloads.load());
    EXPECT_EQ("default", value.get_area(ipv4::str_to_ip("203.0.113.1"), false));

    write_area("YY", "198.51.100.0/24\n");
    EXPECT_TRUE(value.load_area_ips("YY"));
    EXPECT_EQ("YY", value.get_area(ipv4::str_to_ip("198.51.100.1"), false));

    manager_test_access::allow_retry(value, "ZZ");
    value.async_load_area_ips("zz");
    EXPECT_TRUE(wait_for_area(value, "ZZ"));
    value.stop();
    EXPECT_EQ(2, downloads.load());
}

TEST(area_ip_loading, download_exception_releases_reservation_and_backs_off) {
    manager value;
    boost::filesystem::remove(area_path("ZZ"));
    int downloads = 0;
    manager_test_access::set_download(value, [&](const string &) {
        if (++downloads == 1) {
            throw std::runtime_error("模拟下载异常");
        }
        return std::make_pair(200, string("203.0.113.0/24\n"));
    });
    EXPECT_FALSE(value.load_area_ips("ZZ"));
    EXPECT_FALSE(value.load_area_ips("ZZ"));
    EXPECT_EQ(1, downloads);
    manager_test_access::allow_retry(value, "ZZ");
    EXPECT_TRUE(value.load_area_ips("ZZ"));
    EXPECT_EQ(2, downloads);
}

TEST(area_ip_loading, queued_load_is_deduplicated_and_restart_releases_reservation) {
    manager value;
    write_area("ZZ", "203.0.113.0/24\n");
    value.start();
    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    manager_test_access::post(value, [&]() {
        entered.set_value();
        released.wait();
    });
    entered.get_future().wait();
    for (int i = 0; i < 30; ++i) {
        value.async_load_area_ips(i % 2 ? "ZZ" : "zz");
    }
    EXPECT_FALSE(value.load_area_ips("ZZ"));

    auto stopped = std::async(std::launch::async, [&]() { value.stop(); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (value.started() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    const bool stopping = !value.started();
    release.set_value();
    stopped.get();
    EXPECT_TRUE(stopping);
    EXPECT_EQ("default", value.get_area(ipv4::str_to_ip("203.0.113.1"), false));

    value.start();
    value.async_load_area_ips("ZZ");
    EXPECT_TRUE(wait_for_area(value, "ZZ"));
}

TEST(area_ip_loading, stopping_during_download_discards_old_result_and_allows_restart) {
    manager value;
    boost::filesystem::remove(area_path("ZZ"));
    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    manager_test_access::set_download(value, [&](const string &) {
        entered.set_value();
        released.wait();
        return std::make_pair(200, string("203.0.113.0/24\n"));
    });
    value.start();
    value.async_load_area_ips("ZZ");
    const auto download_status = entered.get_future().wait_for(std::chrono::seconds(2));
    auto stopped = std::async(std::launch::async, [&]() { value.stop(); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (value.started() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    const bool stopping = !value.started();
    release.set_value();
    stopped.get();
    EXPECT_EQ(std::future_status::ready, download_status);
    EXPECT_TRUE(stopping);
    EXPECT_EQ("default", value.get_area(ipv4::str_to_ip("203.0.113.1"), false));

    value.start();
    value.async_load_area_ips("ZZ");
    EXPECT_TRUE(wait_for_area(value, "ZZ"));
    value.stop();
}
