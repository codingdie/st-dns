// Area-IP 依赖外部服务，单独归入集成测试。

#include "st.h"
#include <gtest/gtest.h>

TEST(area_ip_network, resolves_japan_address) {
    auto &manager = st::areaip::manager::uniq();
    const auto japan_ip = st::utils::ipv4::str_to_ip("14.0.42.1");
    const auto taiwan_ip = st::utils::ipv4::str_to_ip("118.163.193.132");
    manager.start();
    manager.is_area_ip("JP", japan_ip);
    manager.is_area_ip("TW", taiwan_ip);

    bool japan_loaded = manager.wait_for_ip_info(japan_ip, 10000);
    bool taiwan_loaded = manager.wait_for_ip_info(taiwan_ip, 10000);
    bool result = manager.is_area_ip("JP", japan_ip);
    manager.stop();

    ASSERT_TRUE(japan_loaded);
    ASSERT_TRUE(taiwan_loaded);
    ASSERT_TRUE(result);
}
