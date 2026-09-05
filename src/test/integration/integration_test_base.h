//
// 集成测试公共基类
//
#pragma once

#include "dns_server.h"
#include "dns_client.h"
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace integration_test {
    inline void load_config_once() {
        static std::once_flag config_loaded;
        std::call_once(config_loaded, []() {
            // 地区 IP 段会在配置加载时同步读取或下载，整个集成测试进程只执行一次。
            st::dns::config::INSTANCE.load("../confs/test");
        });
    }
}

class BaseTest : public ::testing::Test {
protected:
    static void start_server_for_suite() {
        integration_test::load_config_once();
        // area_ip_network 用例会停止 Area-IP 运行时；重新启用后台服务但保留已加载的地区 IP 段。
        st::areaip::manager::uniq().start();
        dns_record_manager::uniq().clear();
        dns_record_manager::uniq().set_blacklist_ips({});
        auto &server = suite_server();
        auto &server_thread = suite_server_thread();
        server = new dns_server(st::dns::config::INSTANCE);
        dns_server *server_instance = server;
        server_thread = new thread([server_instance]() { server_instance->start(); });
        server->wait_start();
    }

    static void stop_server_for_suite() {
        auto &server = suite_server();
        auto &server_thread = suite_server_thread();
        if (server != nullptr) {
            server->shutdown();
        }
        if (server_thread != nullptr) {
            server_thread->join();
            delete server_thread;
            server_thread = nullptr;
        }
        delete server;
        server = nullptr;
    }

    void SetUp() override {
        dns_record_manager::uniq().clear();
        dns_record_manager::uniq().set_blacklist_ips({});
    }

    void TearDown() override { dns_record_manager::uniq().set_blacklist_ips({}); }

private:
    static dns_server *&suite_server() {
        static dns_server *server = nullptr;
        return server;
    }

    static thread *&suite_server_thread() {
        static thread *server_thread = nullptr;
        return server_thread;
    }
};
