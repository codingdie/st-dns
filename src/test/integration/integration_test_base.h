//
// 集成测试公共基类
//
#pragma once

#include "dns_server.h"
#include "dns_client.h"
#include <gtest/gtest.h>
#include <thread>

class BaseTest : public ::testing::Test {
protected:
    dns_server *server = nullptr;
    thread *th = nullptr;
    void SetUp() override {
        st::dns::config::INSTANCE.load("../confs/test");
        dns_record_manager::uniq().clear();
        server = new dns_server(st::dns::config::INSTANCE);
        th = new thread([=]() { server->start(); });
        server->wait_start();
    }
    void TearDown() override {
        server->shutdown();
        th->join();
        delete th;
        delete server;
    }
};
