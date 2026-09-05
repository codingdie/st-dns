#include "integration_test_base.h"

namespace {
    class integration_test_environment : public ::testing::Environment {
    public:
        void SetUp() override { integration_test::load_config_once(); }

        void TearDown() override {
            // 在 GTest 生命周期内关闭后台运行时，避免静态析构顺序导致测试进程退出阻塞。
            st::dns::config::INSTANCE.unload();
        }
    };

    ::testing::Environment *const environment =
            ::testing::AddGlobalTestEnvironment(new integration_test_environment());
}
