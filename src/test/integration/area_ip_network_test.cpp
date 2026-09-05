// Area-IP 依赖外部服务，单独归入集成测试。

#include "st.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

TEST(area_ip_network, resolves_japan_address) {
    st::areaip::manager::uniq().start();
    st::areaip::manager::uniq().is_area_ip("JP", "14.0.42.1");
    st::areaip::manager::uniq().is_area_ip("TW", "118.163.193.132");

    std::this_thread::sleep_for(std::chrono::seconds(5));
    bool result = st::areaip::manager::uniq().is_area_ip("JP", "14.0.42.1");
    st::areaip::manager::uniq().stop();
    ASSERT_TRUE(result);
}
