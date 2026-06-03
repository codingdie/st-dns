# Config / Logger / APM / AreaIP Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `config` the single high-level lifecycle owner for logger, APM, and AreaIP runtime resources so tests can repeatedly `load/unload` without timeouts, crashes, or hidden background threads.

**Architecture:** Split lifecycle responsibilities so `logger` only manages normal log sinks, `apm_logger` manages its own async runtime, and `areaip::manager` exposes explicit `start/stop` instead of constructor-side effects. `config::load/unload` becomes the only place that orchestrates startup and shutdown order.

**Tech Stack:** C++11, Boost.Asio, Boost.Log, GoogleTest, CMake/CTest

---

## File Structure

- Modify: `src/core/config.h`
  - Add explicit loaded-state contract for runtime lifecycle orchestration.
- Modify: `src/core/config.cpp`
  - Centralize startup/shutdown ordering for logger, APM, and AreaIP.
- Modify: `src/common/main/utils/logger.h`
  - Ensure logger/APM lifecycle APIs are separately declared and stateful.
- Modify: `src/common/main/utils/logger.cpp`
  - Decouple `logger::init()` from `apm_logger::init()`, make both init/disable flows idempotent.
- Modify: `src/common/main/utils/area_ip.h`
  - Add explicit `start()/stop()` lifecycle API and started state.
- Modify: `src/common/main/utils/area_ip.cpp`
  - Remove constructor-side thread startup, move runtime resources into explicit start/stop.
- Modify: `src/common/test/unit_tests.cpp`
  - Add/load lifecycle regression tests for repeated config load/unload.
- Modify: `src/test/unit/cache_management_test.cpp`
  - Use `config::load/unload` around fixture lifetime.
- Modify: `src/test/unit/reverse_resolve_test.cpp`
  - Use `config::load/unload` around fixture lifetime.
- Modify: `src/test/integration/integration_test_base.h`
  - Ensure integration fixture tears down config runtime after server shutdown.

### Task 1: Decouple logger and APM lifecycle

**Files:**
- Modify: `src/common/main/utils/logger.h`
- Modify: `src/common/main/utils/logger.cpp`
- Test: `src/common/test/unit_tests.cpp`

- [ ] **Step 1: Write the failing lifecycle tests for logger/APM separation**

Add these tests near the existing logger tests in `src/common/test/unit_tests.cpp`:

```cpp
TEST(unit_tests, logger_init_disable_is_idempotent) {
    boost::property_tree::ptree tree;

    st::utils::logger::init(tree);
    st::utils::logger::disable();
    st::utils::logger::disable();

    st::utils::logger::init(tree);
    st::utils::logger::disable();

    SUCCEED();
}

TEST(unit_tests, apm_init_disable_is_idempotent) {
    st::utils::apm_logger::init();
    st::utils::apm_logger::disable(false);
    st::utils::apm_logger::disable(false);

    st::utils::apm_logger::init();
    st::utils::apm_logger::disable(false);

    SUCCEED();
}
```

- [ ] **Step 2: Run the new tests to capture current failures or instability**

Run:

```bash
ctest --output-on-failure -R 'unit_tests.logger_init_disable_is_idempotent|unit_tests.apm_init_disable_is_idempotent' -j1
```

Expected: at least one test fails, hangs, or exposes that `logger::init()` still has hidden APM coupling / repeated disable is unsafe.

- [ ] **Step 3: Separate logger init from APM init**

Update `src/common/main/utils/logger.cpp` so `logger::init()` only initializes normal logging and `logger::disable()` only tears that down. Keep `apm_logger::init()/disable()` separate and explicit.

Required code shape in `src/common/main/utils/logger.cpp`:

```cpp
void logger::disable() {
    boost::shared_ptr<logging::core> core = logging::core::get();
    core->flush();
    core->remove_all_sinks();
    core->reset_filter();
    INITED = false;
}

void logger::init(boost::property_tree::ptree &tree) {
    auto logConfig = tree.get_child_optional("log");
    if (logConfig.is_initialized()) {
        logger::LEVEL = logConfig.get().get<int>("level", 1);
        logger::TAG = logConfig.get().get<string>("tag", "default");
    }

    boost::shared_ptr<logging::core> core = logging::core::get();
    core->remove_all_sinks();
    core->reset_filter();

    typedef sinks::asynchronous_sink<sinks::text_file_backend> sink_t;
    boost::shared_ptr<sinks::text_file_backend> backend =
            boost::make_shared<sinks::text_file_backend>(
                    keywords::file_name = "/tmp/st/" + logger::TAG + ".log",
                    keywords::target_file_name = logger::TAG + ".log.%Y%m%d%H-%N",
                    keywords::rotation_size = 4 * 1024 * 1024);
    boost::shared_ptr<sink_t> sink(new sink_t(backend));
    sink->locked_backend()->set_file_collector(sinks::file::make_collector(
            keywords::target = "/tmp/st",
            keywords::max_size = 16 * 1024 * 1024,
            keywords::max_files = 4));
    sink->locked_backend()->scan_for_files();
    sink->locked_backend()->enable_final_rotation(false);
    sink->set_formatter(expr::stream
                        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                        << " " << expr::smessage);
    core->add_sink(sink);
    logging::add_common_attributes();
    logger::INITED = true;
}
```

Required code shape for `apm_logger::init()/disable()` in the same file:

```cpp
void apm_logger::init() {
    if (IO_CONTEXT_WORK != nullptr) {
        return;
    }
    IO_CONTEXT.restart();
    IO_CONTEXT_WORK = new boost::asio::io_context::work(IO_CONTEXT);
    unsigned int cpu_count = std::thread::hardware_concurrency();
    if (cpu_count == 0) {
        cpu_count = 1;
    }
    for (unsigned int i = 0; i < cpu_count; i++) {
        auto *th = new std::thread([&]() { IO_CONTEXT.run(); });
        LOG_THREADS.emplace_back(th);
    }
    schedule_log();
}

void apm_logger::disable(bool report_status_log) {
    if (IO_CONTEXT_WORK == nullptr) {
        report_apm_log_local(report_status_log);
        return;
    }
    LOG_TIMER.cancel();
    delete IO_CONTEXT_WORK;
    IO_CONTEXT_WORK = nullptr;
    IO_CONTEXT.stop();
    for (thread *th : LOG_THREADS) {
        th->join();
        delete th;
    }
    LOG_THREADS.clear();
    report_apm_log_local(report_status_log);
}
```

- [ ] **Step 4: Re-run the focused lifecycle tests**

Run:

```bash
ctest --output-on-failure -R 'unit_tests.logger_init_disable_is_idempotent|unit_tests.apm_init_disable_is_idempotent' -j1
```

Expected: both tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/common/main/utils/logger.h src/common/main/utils/logger.cpp src/common/test/unit_tests.cpp
git commit -m "refactor: split logger and apm lifecycles"
```

### Task 2: Make AreaIP runtime explicit and restartable

**Files:**
- Modify: `src/common/main/utils/area_ip.h`
- Modify: `src/common/main/utils/area_ip.cpp`
- Test: `src/common/test/unit_tests.cpp`

- [ ] **Step 1: Write the failing AreaIP start/stop regression test**

Add this test to `src/common/test/unit_tests.cpp`:

```cpp
TEST(unit_tests, area_ip_start_stop_is_idempotent) {
    auto &manager = st::areaip::manager::uniq();

    manager.start();
    manager.stop();
    manager.stop();

    manager.start();
    manager.stop();

    SUCCEED();
}
```

- [ ] **Step 2: Run the focused AreaIP lifecycle test**

Run:

```bash
ctest --output-on-failure -R 'unit_tests.area_ip_start_stop_is_idempotent' -j1
```

Expected: FAIL, hang, or require implementation because `areaip::manager` does not yet expose safe explicit lifecycle management.

- [ ] **Step 3: Move AreaIP thread startup out of the constructor**

Update `src/common/main/utils/area_ip.h` declarations to include explicit lifecycle APIs and started-state data:

```cpp
class manager {
public:
    manager();
    ~manager();
    void start();
    void stop();
    bool started() const;
    bool load_area_ips(const string &area_code);
    void async_load_area_ips(const string &area_code);
    // ... keep existing public API ...

private:
    bool started_ = false;
    boost::asio::io_context ctx;
    boost::asio::io_context::work *ctx_work = nullptr;
    std::thread *th = nullptr;

    boost::asio::io_context sche_ctx;
    boost::asio::io_context::work *sche_ctx_work = nullptr;
    std::thread *sche_th = nullptr;
    boost::asio::deadline_timer *sync_timer = nullptr;
    // ... keep other fields ...
};
```

Update `src/common/main/utils/area_ip.cpp` so the constructor only sets up lightweight state and `start()` performs runtime allocation:

```cpp
manager::manager()
    : random_engine(time::now()), last_load_ip_info_time(time::now()), last_load_area_ips_time(time::now()),
      ctx(), sche_ctx() {
    vector<area_ip_range> ip_ranges;
    ip_ranges.emplace_back(area_ip_range::parse("192.168.0.0/16", "LAN"));
    ip_ranges.emplace_back(area_ip_range::parse("10.0.0.0/8", "LAN"));
    ip_ranges.emplace_back(area_ip_range::parse("172.16.0.0/16", "LAN"));
    ip_ranges.emplace_back(area_ip_range::parse("0.0.0.0/8", "LAN"));
    default_caches.emplace("LAN", ip_ranges);
    file::create_if_not_exits(IP_NET_AREA_FILE);
}

void manager::start() {
    if (started_) {
        return;
    }
    ctx.restart();
    sche_ctx.restart();
    ctx_work = new boost::asio::io_context::work(ctx);
    sche_ctx_work = new boost::asio::io_context::work(sche_ctx);
    th = new thread([this]() { this->ctx.run(); });
    sche_th = new thread([this]() { this->sche_ctx.run(); });
    sync_timer = new boost::asio::deadline_timer(sche_ctx);
    started_ = true;
    sync_net_area_ip();
}

void manager::stop() {
    if (!started_) {
        return;
    }
    if (sync_timer != nullptr) {
        sync_timer->cancel();
    }
    delete sche_ctx_work;
    delete ctx_work;
    sche_ctx_work = nullptr;
    ctx_work = nullptr;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sche_ctx.stop();
    ctx.stop();
    if (th && th->joinable()) {
        th->join();
    }
    if (sche_th && sche_th->joinable()) {
        sche_th->join();
    }
    delete th;
    delete sche_th;
    delete sync_timer;
    th = nullptr;
    sche_th = nullptr;
    sync_timer = nullptr;
    started_ = false;
}

manager::~manager() {
    stop();
}
```

Also add:

```cpp
bool manager::started() const {
    return started_;
}
```

- [ ] **Step 4: Re-run the focused AreaIP lifecycle test**

Run:

```bash
ctest --output-on-failure -R 'unit_tests.area_ip_start_stop_is_idempotent' -j1
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/common/main/utils/area_ip.h src/common/main/utils/area_ip.cpp src/common/test/unit_tests.cpp
git commit -m "refactor: add explicit area ip runtime lifecycle"
```

### Task 3: Make config the single lifecycle orchestrator

**Files:**
- Modify: `src/core/config.h`
- Modify: `src/core/config.cpp`
- Test: `src/common/test/unit_tests.cpp`

- [ ] **Step 1: Write the failing config lifecycle regression test**

Add this test to `src/common/test/unit_tests.cpp`:

```cpp
TEST(unit_tests, config_load_unload_is_repeatable) {
    st::dns::config::INSTANCE.load("../confs/test");
    st::dns::config::INSTANCE.unload();
    st::dns::config::INSTANCE.unload();

    st::dns::config::INSTANCE.load("../confs/test");
    st::dns::config::INSTANCE.unload();

    ASSERT_EQ("127.0.0.1", st::dns::config::INSTANCE.ip);
    ASSERT_TRUE(st::dns::config::INSTANCE.servers.empty());
    ASSERT_TRUE(st::dns::config::INSTANCE.force_resolve_rules.empty());
}
```

- [ ] **Step 2: Run the focused config lifecycle test**

Run:

```bash
ctest --output-on-failure -R 'unit_tests.config_load_unload_is_repeatable' -j1
```

Expected: FAIL, hang, or expose current teardown ordering bugs.

- [ ] **Step 3: Implement config-owned startup/shutdown ordering**

Update `src/core/config.h` to keep explicit loaded state:

```cpp
class config {
public:
    static config INSTANCE;
    string ip = "127.0.0.1";
    int port = 53;
    string console_ip = "127.0.0.1";
    int console_port = 5757;
    uint32_t dns_cache_expire = 60 * 10;
    string base_conf_dir = "/usr/local/etc/st/dns";
    vector<remote_dns_server *> servers;
    vector<force_resolve_rule *> force_resolve_rules;
    st::areaip::area_ip_config area_ip_config;
    bool loaded = false;

    config() = default;
    ~config();
    void load(const string &base_conf_dir);
    void unload();
};
```

Update `src/core/config.cpp` so startup and shutdown are explicit and ordered:

```cpp
st::dns::config::~config() {
    unload();
}

void st::dns::config::unload() {
    if (!loaded) {
        return;
    }

    st::areaip::manager::uniq().stop();
    st::utils::apm_logger::disable();
    st::utils::logger::disable();

    for (auto server : servers) {
        delete server;
    }
    servers.clear();

    for (auto rule : force_resolve_rules) {
        delete rule;
    }
    force_resolve_rules.clear();

    ip = "127.0.0.1";
    port = 53;
    console_ip = "127.0.0.1";
    console_port = 5757;
    dns_cache_expire = 60 * 10;
    base_conf_dir = "/usr/local/etc/st/dns";
    loaded = false;
}

void st::dns::config::load(const string &base_conf_dir) {
    unload();
    this->base_conf_dir = base_conf_dir;
    string config_path = base_conf_dir + "/config.json";
    if (!st::utils::file::exists(config_path)) {
        logger::ERROR << "st-dns config file not exit！" << config_path << END;
        exit(1);
    }

    ptree tree;
    try {
        read_json(config_path, tree);
    } catch (json_parser_error &e) {
        logger::ERROR << " parse config file " + config_path + " error!" << e.message() << END;
        exit(1);
    }

    logger::init(tree);
    st::utils::apm_logger::init();

    this->ip = tree.get("ip", string("127.0.0.1"));
    this->port = tree.get("port", port);
    this->console_port = tree.get("console_port", console_port);
    this->console_ip = tree.get("console_ip", string("127.0.0.1"));
    this->dns_cache_expire = stoi(tree.get("dns_cache_expire", to_string(this->dns_cache_expire)));

    auto area_ip_config_node = tree.get_child_optional("area_ip_config");
    if (area_ip_config_node.is_initialized()) {
        this->area_ip_config.load(area_ip_config_node.get());
        areaip::manager::uniq().config(this->area_ip_config);
    }
    areaip::manager::uniq().start();

    // keep the existing server/rule loading logic here

    loaded = true;
}
```

Constraint while editing: keep existing server/rule parsing logic unchanged except for lifecycle order.

- [ ] **Step 4: Re-run the focused config lifecycle test**

Run:

```bash
ctest --output-on-failure -R 'unit_tests.config_load_unload_is_repeatable' -j1
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core/config.h src/core/config.cpp src/common/test/unit_tests.cpp
git commit -m "refactor: make config own runtime lifecycle"
```

### Task 4: Update test fixtures to use config lifecycle consistently

**Files:**
- Modify: `src/test/unit/cache_management_test.cpp`
- Modify: `src/test/unit/reverse_resolve_test.cpp`
- Modify: `src/test/integration/integration_test_base.h`
- Test: CTest targets below

- [ ] **Step 1: Write/retain fixture-based regression coverage**

No new test code is needed in this step; the existing suites already reproduce the lifecycle issue. Use them as the failing regression set:

- `cache_management.*`
- `reverse_resolve.*`
- `cache_integration_tests.*`

- [ ] **Step 2: Run the focused regression suites before fixture cleanup changes**

Run:

```bash
ctest --output-on-failure -R 'cache_management.cache_expiration|reverse_resolve.basic_reverse_resolve|cache_integration_tests.cache_miss_then_hit' -j1
```

Expected: prior to the fixture fix, these tests may hang, timeout, or crash during teardown.

- [ ] **Step 3: Make every fixture pair config load with config unload**

Update `src/test/unit/cache_management_test.cpp` fixture:

```cpp
class cache_management : public ::testing::Test {
protected:
    void SetUp() override {
        st::dns::config::INSTANCE.load("../confs/test");
    }

    void TearDown() override {
        st::dns::config::INSTANCE.unload();
    }
};
```

Update `src/test/unit/reverse_resolve_test.cpp` fixture:

```cpp
class reverse_resolve : public ::testing::Test {
protected:
    void SetUp() override {
        st::dns::config::INSTANCE.load("../confs/test");
    }

    void TearDown() override {
        st::dns::config::INSTANCE.unload();
    }
};
```

Update `src/test/integration/integration_test_base.h` teardown order so server stops before config unload:

```cpp
void TearDown() override {
    server->shutdown();
    th->join();
    delete th;
    delete server;
    st::dns::config::INSTANCE.unload();
}
```

- [ ] **Step 4: Re-run the focused regression suites**

Run:

```bash
ctest --output-on-failure -R 'cache_management.cache_expiration|reverse_resolve.basic_reverse_resolve|cache_integration_tests.cache_miss_then_hit' -j1
```

Expected: PASS without timeout or teardown crash.

- [ ] **Step 5: Commit**

```bash
git add src/test/unit/cache_management_test.cpp src/test/unit/reverse_resolve_test.cpp src/test/integration/integration_test_base.h
git commit -m "test: align fixtures with config runtime lifecycle"
```

### Task 5: Run full regression verification

**Files:**
- Modify: none
- Test: `build/`

- [ ] **Step 1: Build both test binaries from a clean compile step**

Run:

```bash
cmake --build build --target st-dns-unit-test -j2
cmake --build build --target st-dns-integration-test -j2
```

Expected: both targets build successfully.

- [ ] **Step 2: Run lifecycle-heavy targeted suites**

Run:

```bash
cd build && ctest --output-on-failure -R 'cache_management.*|reverse_resolve.*|cache_integration_tests.*' -j1
```

Expected: all targeted tests PASS without timeout, abort, or segfault.

- [ ] **Step 3: Run the full serial CTest suite**

Run:

```bash
cd build && ctest --output-on-failure -j1
```

Expected: all tests PASS.

- [ ] **Step 4: Inspect for residual teardown regressions**

Run:

```bash
grep -E 'Timeout|SegFault|Subprocess aborted|Exception' build/Testing/Temporary/LastTest.log || true
```

Expected: no matches.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "test: verify lifecycle refactor end-to-end"
```

## Self-Review

- Spec coverage:
  - `config` 统一编排：Task 3, Task 4
  - `logger`/`apm_logger` 解耦：Task 1
  - `apm` 默认始终运行：Task 3
  - `areaip::manager` 显式 `start/stop`：Task 2
  - 相关测试超时/崩溃回归：Task 4, Task 5
- Placeholder scan: no TBD/TODO placeholders remain.
- Type consistency:
  - `config::load/unload`
  - `logger::init/disable`
  - `apm_logger::init/disable`
  - `areaip::manager::start/stop/started`
  are referenced consistently across tasks.
