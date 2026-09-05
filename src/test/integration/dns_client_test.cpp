//
// Created by codingdie on 2020/6/27.
//
#include <gtest/gtest.h>
#include "dns_client.h"
#include <iostream>
#include <vector>
#include <future>
#include <atomic>
#include <chrono>
#include <thread>

namespace {

void test_dns(const string &domain, const string &server, const uint32_t port, const string &type, const vector<pair<string, uint16_t>> areas) {
    std::promise<pair<std::vector<uint32_t>, bool>> promise;
    auto future = promise.get_future();
    auto complete = [&](std::vector<uint32_t> ips, bool loadAll) {
        promise.set_value({ips, loadAll});
    };
    if (type == "TCP") {
        dns_client::uniq().tcp_dns(domain, server, port, 10000, areas, complete);
    } else if (type == "TCP_SSL") {
        dns_client::uniq().tcp_tls_dns(domain, server, port, 10000, areas, complete);
    } else {
        dns_client::uniq().udp_dns(domain, server, port, 200, [=](std::vector<uint32_t> ips) {
            complete(ips, true);
        });
    }
    auto async_result = future.get();
    auto result = async_result.first;
    auto resultLoadAll = async_result.second;
    ASSERT_TRUE(result.size() > 0);
    ASSERT_TRUE(resultLoadAll);

    logger::INFO << domain << "ips:" << st::utils::ipv4::ips_to_str(result) << END;
}
void testDNS(const string &domain, const string &server, const uint32_t port, const string &type) {
    test_dns(domain, server, port, type, {});
}

class silent_tcp_server {
public:
    explicit silent_tcp_server(uint32_t hold_time) : acceptor(context, tcp::endpoint(tcp::v4(), 0)), hold_time(hold_time) {
        thread = std::thread([this]() {
            tcp::socket socket(context);
            boost::system::error_code ec;
            acceptor.accept(socket, ec);
            if (!ec) {
                std::this_thread::sleep_for(std::chrono::milliseconds(this->hold_time));
                socket.close(ec);
            }
        });
    }

    ~silent_tcp_server() {
        if (thread.joinable()) {
            thread.join();
        }
    }

    uint16_t port() const { return acceptor.local_endpoint().port(); }

private:
    boost::asio::io_context context;
    tcp::acceptor acceptor;
    uint32_t hold_time;
    std::thread thread;
};

void expect_single_timeout(const std::function<void(uint16_t, const dns_complete &)> &query) {
    silent_tcp_server server(200);
    std::promise<std::vector<uint32_t>> result_promise;
    auto result = result_promise.get_future();
    std::atomic_uint32_t callback_count(0);

    query(server.port(), [&result_promise, &callback_count](const std::vector<uint32_t> &ips) {
        if (callback_count.fetch_add(1) == 0) {
            result_promise.set_value(ips);
        }
    });

    ASSERT_EQ(std::future_status::ready, result.wait_for(std::chrono::seconds(1)));
    ASSERT_TRUE(result.get().empty());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(1, callback_count.load());
}

TEST(network_dns, tcp_timeout_completes_once) {
    expect_single_timeout([](uint16_t port, const dns_complete &complete) {
        dns_client::uniq().tcp_dns("timeout.test", "127.0.0.1", port, 30, complete);
    });
}

TEST(network_dns, tcp_tls_timeout_completes_once) {
    expect_single_timeout([](uint16_t port, const dns_complete &complete) {
        dns_client::uniq().tcp_tls_dns("timeout.test", "127.0.0.1", port, 30, complete);
    });
}


TEST(network_dns, udp) {
    auto begin = time::now();
    for (auto i = 0; i < 10; i++) {
        testDNS("baidu.com", "114.114.114.114", 53, "UDP");
    }
    logger::INFO << "test_udp_dns total cost" << time::now() - begin << END;
}


TEST(network_dns, tcp) {
    auto begin = time::now();
    for (auto i = 0; i < 10; i++) {
        testDNS("www.google.com", "8.8.8.8", 53, "TCP");
    }
    logger::INFO << "test_tcp_dns total cost" << time::now() - begin << END;
}

TEST(network_dns, tcp_tls) {
    logger::LEVEL = 0;
    auto begin = time::now();
    for (auto i = 0; i < 10; i++) {
        testDNS("www.google.com", "8.8.8.8", 853, "TCP_SSL");
    }
    logger::INFO << "test_tcp_dns total cost" << time::now() - begin << END;
}


TEST(network_dns, tcp_tls_resolve_multi_area) {
    logger::LEVEL = 0;
    logger::INFO << st::mem::malloc_size() << st::mem::free_size() << st::mem::leak_size() << string::npos << END;
    test_dns("www.google.com", "8.8.8.8", 853, "TCP_SSL", {{"US", 853}, {"JP", 853}, {"HK", 853}, {"TW", 853}});
    logger::INFO << st::mem::malloc_size() << st::mem::free_size() << st::mem::leak_size() << END;
}

} // namespace
